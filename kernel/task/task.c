#include "task.h"
#include "../mem/vmm.h"
#include "../mem/pmm.h"
#include "../fs/initrd.h"
#include "../elf.h"

// Task system with proper memory isolation for AI-OS

task_t* create_task_from_initrd_file(const char* filename) {
    // 1. Find file in initrd
    char* file_data = initrd_read_file(filename);
    if (!file_data) {
        print_string_serial("ERROR: Could not find file in initrd: ");
        print_string_serial(filename);
        print_string_serial("\n");
        return NULL;
    }

    // 2. Create an isolated page directory for the task
    uint32_t* page_directory = create_user_page_directory();
    if (!page_directory) {
        print_string_serial("ERROR: Could not create page directory for new task.\n");
        return NULL;
    }

    // Switch to the new page directory to load the ELF
    // This is a temporary switch to allow vmm_get_physical_address to work correctly during ELF loading
    uint32_t* old_directory = current_directory;
    vmm_switch_page_directory(page_directory);

    // 3. Load the ELF executable into the new address space
    uint32_t entry_point = elf_load((uint8_t*)file_data, page_directory);

    // Switch back to the kernel directory
    vmm_switch_page_directory(old_directory);

    if (entry_point == 0) {
        print_string_serial("ERROR: Failed to load ELF file.\n");
        // TODO: Free the page directory
        return NULL;
    }

    // 4. Create the task structure
    task_t* new_task = (task_t*)kmalloc(sizeof(task_t));
    if (!new_task) {
        print_string_serial("ERROR: Failed to allocate task structure.\n");
        // TODO: Free page directory
        return NULL;
    }

    new_task->id = next_pid++;
    new_task->state = TASK_READY;
    new_task->privilege_level = 3; // Ring 3
    new_task->page_directory = page_directory;
    new_task->entry_point = entry_point;

    // 5. Allocate a user stack
    uint32_t user_stack_top = allocate_user_stack(new_task->page_directory);
    if (!user_stack_top) {
        print_string_serial("ERROR: Failed to allocate user stack.\n");
        kfree(new_task);
        // TODO: Free page directory
        return NULL;
    }
    new_task->esp = user_stack_top;

    // 6. Set up the user context for the first switch
    setup_initial_user_context(new_task, entry_point, user_stack_top);

    // 7. Add to ready queue
    add_to_ready_queue(new_task);
    total_tasks++;

    print_string("User task created from file with isolated memory\n");
    return new_task;
}

void setup_initial_user_context(task_t* task, uint32_t entry_point, uint32_t stack_top) {
    // This function sets up the stack for the iret instruction
    // that will be used to jump to user mode.
    task->cpu_state.eip = entry_point;
    task->cpu_state.cs = 0x1B; // User code segment selector
    task->cpu_state.eflags = 0x202; // Interrupts enabled, and bit 1 is always 1
    task->cpu_state.esp = stack_top;
    task->cpu_state.ss = 0x23; // User data segment selector

    // Also set other user-mode segments
    task->cpu_state.ds = 0x23;
    task->cpu_state.es = 0x23;
    task->cpu_state.fs = 0x23;
    task->cpu_state.gs = 0x23;
}

uint32_t* create_user_page_directory() {
    uint32_t* user_page_dir = (uint32_t*)kmalloc_aligned(4096);
    if (!user_page_dir) {
        return NULL;
    }
    
    memset(user_page_dir, 0, 4096);
    
    // Copy kernel mappings (first 256 entries for kernel space)
    extern uint32_t kernel_directory;
    uint32_t* kernel_page_dir = (uint32_t*)kernel_directory;
    
    for (int i = 0; i < 256; i++) {
        user_page_dir[i] = kernel_page_dir[i];
    }
    
    // User space starts at 0x40000000 (entries 256-1023)
    // These remain zero initially and are allocated on demand
    
    return user_page_dir;
}