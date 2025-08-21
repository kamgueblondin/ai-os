#include "task.h"
#include "kernel/mem/pmm.h"
#include "kernel/mem/vmm.h"
#include <stddef.h>
#include "kernel/elf.h"
#include "kernel/mem/string.h"
#include "kernel.h"
#include "kernel/mem/heap.h"
#include "fs/initrd.h"

// Variables globales
task_t* current_task = NULL;
task_t* task_queue = NULL;
int next_task_id = 0;

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

void schedule(cpu_state_t* cpu) {
    if (!current_task) return;
    memcpy(&current_task->cpu_state, cpu, sizeof(cpu_state_t));

    current_task = current_task->next;
    while(current_task->state != TASK_READY) {
        current_task = current_task->next;
    }
    current_task->state = TASK_RUNNING;

    if (current_directory != current_task->vmm_dir) {
        vmm_switch_page_directory(current_task->vmm_dir->physical_dir);
        current_directory = current_task->vmm_dir;
    }

    memcpy(cpu, &current_task->cpu_state, sizeof(cpu_state_t));
}

void task_exit() {
    if (!current_task) return;

    current_task->state = TASK_TERMINATED;
    // TODO: Free memory and other resources

    // For now, just halt
    asm volatile("int $0x30");
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

    vmm_directory_t* old_dir = current_directory;
    vmm_switch_page_directory(vmm_dir->physical_dir);
    current_directory = vmm_dir;

    uint32_t entry_point = elf_load(file_data, vmm_dir);

    vmm_switch_page_directory(old_dir->physical_dir);
    current_directory = old_dir;

    if (entry_point == 0) {
        print_string_serial("ERREUR: Chargement ELF a echoue\n");
        // TODO: Liberer vmm_dir
        return NULL;
    }

    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    new_task->id = next_task_id++;
    new_task->state = TASK_READY;
    new_task->type = TASK_TYPE_USER;
    new_task->vmm_dir = vmm_dir;

    uint32_t user_stack_top = allocate_user_stack(vmm_dir);
    setup_initial_user_context(new_task, entry_point, user_stack_top);
    add_task_to_queue(new_task);

    print_string_serial("Tache utilisateur creee avec succes\n");
    return new_task;
}

void setup_initial_user_context(task_t* task, uint32_t entry_point, uint32_t stack_top) {
    memset(&task->cpu_state, 0, sizeof(cpu_state_t));
    task->cpu_state.eip = entry_point;
    task->cpu_state.useresp = stack_top;
    task->cpu_state.eflags = 0x202;
    task->cpu_state.cs = 0x1B;
    task->cpu_state.ds = 0x23;
    task->cpu_state.es = 0x23;
    task->cpu_state.fs = 0x23;
    task->cpu_state.gs = 0x23;
    task->cpu_state.ss = 0x23;
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

uint32_t allocate_user_stack(vmm_directory_t* vmm_dir) {
    uint32_t stack_top = 0xB0000000;
    uint32_t stack_bottom = stack_top - PAGE_SIZE;

    void* stack_phys = pmm_alloc_page();
    if (!stack_phys) return 0;

    vmm_map_page_in_directory(vmm_dir, stack_phys, (void*)stack_bottom, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    return stack_top;
}
