#include "task.h"
#include "kernel/mem/pmm.h"
#include "kernel/mem/vmm.h"
#include <stddef.h>
#include "kernel/elf.h"
#include "kernel/mem/string.h"
#include "kernel.h"
#include "kernel/mem/heap.h"
#include "fs/initrd.h"

// Variables globales pour le système de tâches
task_t* current_task = NULL;
task_t* task_queue = NULL;
int next_task_id = 0;

// Déclarations externes
extern page_directory_t* kernel_directory;
extern void print_string(const char* str);
extern void print_string_serial(const char* str);
extern void switch_task(cpu_state_t* old_state, cpu_state_t* new_state);

// Initialise le système de tâches
void tasking_init() {
    print_string_serial("Initialisation du systeme de taches...\n");

    // Crée la tâche kernel principale
    current_task = (task_t*)pmm_alloc_page();
    if (!current_task) {
        print_string_serial("ERREUR: Impossible d'allouer la tache kernel\n");
        return;
    }

    current_task->id = next_task_id++;
    current_task->state = TASK_RUNNING;
    current_task->type = TASK_TYPE_KERNEL;
    current_task->page_directory = (uint32_t*)kernel_directory;
    current_task->stack_base = 0;
    current_task->stack_size = 0;
    current_task->next = current_task;
    current_task->prev = current_task;

    task_queue = current_task;

    print_string_serial("Tache kernel creee avec ID 0\n");
}

// Ajoute une tâche à la file d'attente
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

// Crée une nouvelle tâche kernel
task_t* create_task(void (*entry_point)()) {
    // Alloue la mémoire pour la structure de tâche
    task_t* new_task = (task_t*)pmm_alloc_page();
    if (!new_task) {
        print_string_serial("ERREUR: Impossible d'allouer la tache\n");
        return NULL;
    }

    // Alloue la pile pour la tâche
    void* stack = pmm_alloc_page();
    if (!stack) {
        print_string_serial("ERREUR: Impossible d'allouer la pile\n");
        pmm_free_page(new_task);
        return NULL;
    }

    // Initialise la tâche
    new_task->id = next_task_id++;
    new_task->state = TASK_READY;
    new_task->type = TASK_TYPE_KERNEL;
    new_task->page_directory = (uint32_t*)kernel_directory;
    new_task->stack_base = (uint32_t)stack;
    new_task->stack_size = 4096;

    // Initialise l'état CPU
    new_task->cpu_state.eax = 0;
    new_task->cpu_state.ebx = 0;
    new_task->cpu_state.ecx = 0;
    new_task->cpu_state.edx = 0;
    new_task->cpu_state.esi = 0;
    new_task->cpu_state.edi = 0;
    new_task->cpu_state.ebp = 0;
    new_task->cpu_state.eip = (uint32_t)entry_point;
    new_task->cpu_state.esp = (uint32_t)stack + 4096 - 4;
    new_task->cpu_state.eflags = 0x202;
    new_task->cpu_state.cs = 0x08;
    new_task->cpu_state.ds = 0x10;
    new_task->cpu_state.es = 0x10;
    new_task->cpu_state.fs = 0x10;
    new_task->cpu_state.gs = 0x10;
    new_task->cpu_state.ss = 0x10;

    // Ajoute la tâche à la file d'attente
    add_task_to_queue(new_task);

    print_string_serial("Nouvelle tache creee\n");
    return new_task;
}

// Ordonnanceur Round-Robin
void schedule(cpu_state_t* cpu) {
    if (!current_task || !task_queue) {
        return;
    }

    // Si appelé depuis une interruption, sauvegarder l'état actuel
    if (cpu) {
        memcpy(&current_task->cpu_state, cpu, sizeof(cpu_state_t));
    }

    // Trouve la prochaine tâche prête
    task_t* next_task = current_task->next;
    while (next_task->state != TASK_READY && next_task != current_task) {
        next_task = next_task->next;
    }

    if (next_task == current_task || next_task->state != TASK_READY) {
        // Aucune autre tâche prête, on ne change rien
        return;
    }

    // Mise à jour de l'état
    if (current_task->state == TASK_RUNNING) {
        current_task->state = TASK_READY;
    }
    next_task->state = TASK_RUNNING;

    task_t* old_task = current_task;
    current_task = next_task;

    // Changer le répertoire de pages si nécessaire
    if (old_task->page_directory != current_task->page_directory) {
        vmm_switch_page_directory((page_directory_t*)current_task->page_directory);
    }

    // Si appelé depuis une interruption, met à jour la pile pour le retour
    if (cpu) {
        memcpy(cpu, &current_task->cpu_state, sizeof(cpu_state_t));
    } else {
        // Premier lancement, on ne revient jamais
        jump_to_task(&current_task->cpu_state);
    }
}

// Termine la tâche courante
void task_exit() {
    if (!current_task) return;

    current_task->state = TASK_TERMINATED;

    // Libère les ressources
    if (current_task->stack_base && current_task->type == TASK_TYPE_KERNEL) {
        pmm_free_page((void*)current_task->stack_base);
    }

    // Retire de la file d'attente
    if (current_task->next == current_task) {
        // Dernière tâche
        task_queue = NULL;
        current_task = NULL;
    } else {
        current_task->prev->next = current_task->next;
        current_task->next->prev = current_task->prev;

        if (task_queue == current_task) {
            task_queue = current_task->next;
        }

        task_t* old_task = current_task;
        current_task = current_task->next;
        current_task->state = TASK_RUNNING;

        pmm_free_page(old_task);
    }

    // Force un changement de contexte
    asm volatile("int $0x30");
}

// Obtient la tâche courante
task_t* get_current_task() {
    return current_task;
}

// Déclarations des fonctions "privées"
page_directory_t* create_user_page_directory();
uint32_t allocate_user_stack(page_directory_t* page_dir);
void setup_initial_user_context(task_t* task, uint32_t entry_point, uint32_t stack_top);

// Crée une tâche complète à partir d'un fichier dans l'initrd
task_t* create_task_from_initrd_file(const char* filename) {
    print_string_serial("Creation de la tache a partir de: ");
    print_string_serial(filename);
    print_string_serial("\n");

    uint8_t* file_data = (uint8_t*)initrd_read_file(filename);
    if (!file_data) {
        print_string_serial("ERREUR: Fichier non trouve dans l'initrd\n");
        return NULL;
    }

    page_directory_t* page_directory = create_user_page_directory();
    if (!page_directory) {
        print_string_serial("ERREUR: Impossible de creer le repertoire de pages\n");
        return NULL;
    }

    // On doit switcher temporairement pour que elf_load fonctionne sur le bon VMM
    page_directory_t* old_directory = (page_directory_t*)current_task->page_directory;
    vmm_switch_page_directory(page_directory);

    uint32_t entry_point = elf_load(file_data, page_directory);

    // Revenir à l'ancien répertoire
    vmm_switch_page_directory(old_directory);

    if (entry_point == 0) {
        print_string_serial("ERREUR: Chargement ELF a echoue\n");
        // TODO: Libérer page_directory
        return NULL;
    }

    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    if (!new_task) {
        print_string_serial("ERREUR: kmalloc pour la tache a echoue\n");
        return NULL;
    }

    new_task->id = next_task_id++;
    new_task->state = TASK_READY;
    new_task->type = TASK_TYPE_USER;
    new_task->page_directory = (uint32_t*)page_directory;

    uint32_t user_stack_top = allocate_user_stack(page_directory);
    if (!user_stack_top) {
        print_string_serial("ERREUR: allocation pile utilisateur a echoue\n");
        kfree(new_task);
        return NULL;
    }

    setup_initial_user_context(new_task, entry_point, user_stack_top);

    add_task_to_queue(new_task);

    print_string_serial("Tache utilisateur creee avec succes\n");
    return new_task;
}

void setup_initial_user_context(task_t* task, uint32_t entry_point, uint32_t stack_top) {
    memset(&task->cpu_state, 0, sizeof(cpu_state_t));
    task->cpu_state.eip = entry_point;
    task->cpu_state.esp = stack_top;
    task->cpu_state.eflags = 0x202; // Activer les interruptions
    task->cpu_state.cs = 0x1B;     // Sélecteur de code utilisateur (0x18 | 3)
    task->cpu_state.ds = 0x23;     // Sélecteur de données utilisateur (0x20 | 3)
    task->cpu_state.es = 0x23;
    task->cpu_state.fs = 0x23;
    task->cpu_state.gs = 0x23;
    task->cpu_state.ss = 0x23;     // Sélecteur de pile utilisateur
}

page_directory_t* create_user_page_directory() {
    page_directory_t* dir = (page_directory_t*)kmalloc_aligned(sizeof(page_directory_t));
    if (!dir) return NULL;
    memset(dir, 0, sizeof(page_directory_t));

    // Clone les mappages du noyau
    for (int i = 768; i < 1024; i++) { // Espace noyau est le dernier Go
        if (kernel_directory->tables[i]) {
            dir->tables[i] = kernel_directory->tables[i];
            dir->tablesPhysical[i] = kernel_directory->tablesPhysical[i];
        }
    }
    dir->physicalAddr = (uint32_t)dir->tablesPhysical;

    return dir;
}

uint32_t allocate_user_stack(page_directory_t* page_dir) {
    // La pile utilisateur est définie dans userspace/start.s à 0x50000000
    uint32_t stack_top = 0x50000000;
    uint32_t stack_bottom = stack_top - PAGE_SIZE;

    void* stack_phys = pmm_alloc_page();
    if (!stack_phys) {
        return 0;
    }

    vmm_map_page_in_directory(page_dir, stack_phys, (void*)stack_bottom, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    return stack_top;
}
