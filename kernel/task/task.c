#include "task.h"
#include "kernel/mem/pmm.h"
#include "kernel/mem/vmm.h"
#include <stddef.h>
#include "kernel/elf.h"
#include "kernel/mem/string.h"
#include "kernel.h"
#include "kernel/mem/heap.h"
#include "fs/initrd.h"
#include "kernel/gdt.h"

// Variables globales
task_t* current_task = NULL;
task_t* task_queue = NULL;
int next_task_id = 0;
volatile int g_reschedule_needed = 0;

// Externes
extern vmm_directory_t* kernel_directory;
extern vmm_directory_t* current_directory;
extern void print_string_serial(const char* str);

// Prototypes
vmm_directory_t* create_user_vmm_directory();
uint32_t allocate_user_stack(vmm_directory_t* vmm_dir);
void setup_initial_user_context(task_t* task, uint32_t entry_point, uint32_t stack_top);


void tasking_init() {
    current_task = (task_t*)kmalloc(sizeof(task_t));
    current_task->id = next_task_id++;
    current_task->state = TASK_RUNNING;
    current_task->type = TASK_TYPE_KERNEL;
    current_task->vmm_dir = kernel_directory;
    current_task->next = current_task;
    current_task->prev = current_task;
    task_queue = current_task;
    print_string_serial("Tache kernel creee.\n");
}

void add_task_to_queue(task_t* task) {
    if (!task_queue) {
        task_queue = task;
        task->next = task;
        task->prev = task;
    } else {
        task->next = task_queue;
        task->prev = task_queue->prev;
        task_queue->prev->next = task;
        task_queue->prev = task;
    }
}

// Déclaration de la fonction assembleur pour le changement de contexte
extern void jump_to_task(cpu_state_t* next_state);

static void unlink_task(task_t* task) {
    if (!task_queue || !task) return;
    if (task->next == task) {
        // Single element in queue
        task_queue = NULL;
        current_task = NULL;
        return;
    }
    task->prev->next = task->next;
    task->next->prev = task->prev;
    if (task_queue == task) task_queue = task->next;
    if (current_task == task) current_task = task->next;
}

void schedule(cpu_state_t* cpu) {
    asm volatile("cli"); // Désactiver les interruptions pour la planification
    if (!current_task) {
        asm volatile("sti");
        return;
    }

    // Si la tache courante est terminee, ne pas sauver l'etat; retirer de la file
    if (current_task->state != TASK_TERMINATED) {
        // Sauvegarder l'état de la tâche actuelle
        memcpy(&current_task->cpu_state, cpu, sizeof(cpu_state_t));
    } else {
        print_string_serial("[SCHED] removing terminated task\n");
        unlink_task(current_task);
        if (!task_queue) {
            asm volatile("sti");
            while(1) asm volatile("hlt");
        }
    }

    // Si la tâche tournait, elle est maintenant prête à être replanifiée plus tard
    if (current_task->state == TASK_RUNNING) {
        current_task->state = TASK_READY;
    }

    // Chercher la prochaine tâche prête.
    // C'est un simple ordonnanceur round-robin qui ignore les tâches en attente.
    task_t* next_task = current_task ? current_task : task_queue;
    while (1) {
        next_task = next_task->next;

        // Si la tâche est prête à s'exécuter, on la choisit.
        if (next_task->state == TASK_READY) {
            break;
        }

        // Si on a fait un tour complet et que personne n'est prêt,
        // et que la tâche actuelle ne peut plus tourner, on choisit la tâche kernel (idle).
        // Cela evite un blocage si toutes les taches sont en attente.
        if (next_task == current_task) {
            // Si la tache courante est en attente, on doit trouver une autre tache.
            if (current_task->state != TASK_READY && current_task->state != TASK_RUNNING) {
                 // On cherche la tache kernel (ID 0) comme dernier recours.
                task_t* kernel_task = task_queue;
                while(kernel_task->id != 0) kernel_task = kernel_task->next;
                next_task = kernel_task;
            }
            // Si la tache courante est encore prete, on la laisse tourner.
            break;
        }
    }

    print_string_serial("[SCHED] switching to task ");
    write_serial('0' + (next_task->id % 10));
    print_string_serial("\n");
    current_task = next_task;
    current_task->state = TASK_RUNNING;

    // Mettre à jour le TSS avec la pile noyau de la nouvelle tâche
    if (current_task->type == TASK_TYPE_USER) {
        tss_set_stack(0x10, current_task->kernel_stack_p);
    }

    // Changer de répertoire de pages si nécessaire
    if (current_directory != current_task->vmm_dir) {
        vmm_switch_page_directory(current_task->vmm_dir->physical_addr);
        current_directory = current_task->vmm_dir;
    }

    // Sauter à la nouvelle tâche. Ne retourne jamais.
    jump_to_task(&current_task->cpu_state);
}

void task_exit() {
    if (!current_task) return;

    current_task->state = TASK_TERMINATED;
    print_string_serial("[TASK_EXIT] terminating, scheduling now\n");

    // Utiliser l'appel système pour quitter proprement
    // Cela déclenchera le scheduler sans sauvegarder l'état de cette tâche
    asm volatile("int $0x80" : : "a"(0), "b"(0));

    // On ne devrait jamais arriver ici
    while(1) { asm volatile("hlt"); }
}

task_t* create_task_from_initrd_file(const char* filename) {
    uint8_t* file_data = (uint8_t*)initrd_read_file(filename);
    if (!file_data) {
        print_string_serial("ERREUR: Fichier non trouve dans l'initrd\n");
        return NULL;
    }

    vmm_directory_t* vmm_dir = create_user_vmm_directory();
    if (!vmm_dir) {
        print_string_serial("ERREUR: Creation VMM directory a echoue\n");
        return NULL;
    }

    uint32_t entry_point = 0;
    uint32_t user_stack_top = 0;

    // --- Critical section for new task's address space ---
    vmm_directory_t* old_dir = current_directory;
    vmm_switch_page_directory(vmm_dir->physical_addr);
    current_directory = vmm_dir;

    // Load the executable into the new address space
    entry_point = elf_load(file_data, vmm_dir);
    if (entry_point != 0) {
        // Allocate the user stack in the new address space
        user_stack_top = allocate_user_stack(vmm_dir);
    }

    // Restore the kernel's address space
    vmm_switch_page_directory(old_dir->physical_addr);
    current_directory = old_dir;
    // --- End of critical section ---

    if (entry_point == 0 || user_stack_top == 0) {
        print_string_serial("ERREUR: Chargement ELF ou allocation de pile a echoue\n");
        // TODO: Liberer vmm_dir et toutes les pages allouées
        return NULL;
    }

    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    new_task->id = next_task_id++;
    new_task->state = TASK_READY;
    new_task->type = TASK_TYPE_USER;
    new_task->vmm_dir = vmm_dir;

    // Allouer une pile noyau pour cette tâche
    new_task->kernel_stack_p = (uint32_t)kmalloc(4096) + 4096;

    setup_initial_user_context(new_task, entry_point, user_stack_top);
    add_task_to_queue(new_task);

    print_string_serial("Tache utilisateur creee avec succes\n");
    return new_task;
}

void setup_initial_user_context(task_t* task, uint32_t entry_point, uint32_t stack_top) {
    memset(&task->cpu_state, 0, sizeof(cpu_state_t));
    
    // Configuration des registres généraux
    task->cpu_state.eax = 0;
    task->cpu_state.ebx = 0;
    task->cpu_state.ecx = 0;
    task->cpu_state.edx = 0;
    task->cpu_state.esi = 0;
    task->cpu_state.edi = 0;
    task->cpu_state.ebp = 0;
    
    // Configuration de l'exécution
    task->cpu_state.eip = entry_point;
    task->cpu_state.useresp = stack_top;
    task->cpu_state.eflags = 0x202; // Interruptions activées
    
    // Configuration des segments utilisateur (Ring 3)
    task->cpu_state.cs = 0x1B;  // Code segment utilisateur (Ring 3)
    task->cpu_state.ds = 0x23;  // Data segment utilisateur (Ring 3)
    task->cpu_state.es = 0x23;
    task->cpu_state.fs = 0x23;
    task->cpu_state.gs = 0x23;
    task->cpu_state.ss = 0x23;  // Stack segment utilisateur (Ring 3)
    
    print_string_serial("User context setup complete. EIP=0x");
    print_hex_serial(task->cpu_state.eip);
    print_string_serial(", ESP=0x");
    print_hex_serial(task->cpu_state.useresp);
    print_string_serial("\n");
}

vmm_directory_t* create_user_vmm_directory() {
    print_string_serial("create_user_vmm_directory: start\n");
    vmm_directory_t* dir = (vmm_directory_t*)kmalloc(sizeof(vmm_directory_t));
    if (!dir) {
        print_string_serial("create_user_vmm_directory: kmalloc for dir failed\n");
        return NULL;
    }
    memset(dir, 0, sizeof(vmm_directory_t));

    dir->tables = (page_table_t**)kmalloc(sizeof(page_table_t*) * 1024);
    if (!dir->tables) {
        print_string_serial("create_user_vmm_directory: kmalloc for tables failed\n");
        kfree(dir);
        return NULL;
    }
    memset(dir->tables, 0, sizeof(page_table_t*) * 1024);

    dir->physical_dir = (page_directory_t*)kmalloc_aligned(sizeof(page_directory_t));
    if (!dir->physical_dir) {
        print_string_serial("create_user_vmm_directory: kmalloc_aligned for physical_dir failed\n");
        kfree(dir->tables);
        kfree(dir);
        return NULL;
    }
    memset(dir->physical_dir, 0, sizeof(page_directory_t));
    dir->physical_addr = (uint32_t)dir->physical_dir;

    // Clone kernel space
    for (int i = 0; i < 1024; i++) {
         if (kernel_directory->tables[i]) {
            dir->tables[i] = kernel_directory->tables[i];
            dir->physical_dir->tablesPhysical[i] = kernel_directory->physical_dir->tablesPhysical[i];
        }
    }
    return dir;
}

#define USER_STACK_TOP 0xB0000000
#define USER_STACK_PAGES 16
#define USER_STACK_SIZE (USER_STACK_PAGES * PAGE_SIZE)
#define USER_STACK_BOTTOM (USER_STACK_TOP - USER_STACK_SIZE)

uint32_t allocate_user_stack(vmm_directory_t* vmm_dir) {
    print_string_serial("Allocating user stack...\n");
    for (uint32_t addr = USER_STACK_BOTTOM; addr < USER_STACK_TOP; addr += PAGE_SIZE) {
        void* stack_phys_page = pmm_alloc_page();
        if (!stack_phys_page) {
            print_string_serial("ERROR: Could not allocate physical page for user stack\n");
            // In a real scenario, we should free previously allocated pages
            return 0;
        }
        vmm_map_page_in_directory(vmm_dir, stack_phys_page, (void*)addr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }

    print_string_serial("User stack allocated at 0x");
    print_hex_serial(USER_STACK_BOTTOM);
    print_string_serial(" - 0x");
    print_hex_serial(USER_STACK_TOP);
    print_string_serial("\n");

    return USER_STACK_TOP;
}

task_t* find_task_waiting_for_input() {
    if (!task_queue) {
        return NULL;
    }

    task_t* temp = task_queue;
    do {
        if (temp->state == TASK_WAITING_FOR_INPUT) {
            return temp;
        }
        temp = temp->next;
    } while (temp != task_queue);

    return NULL;
}
