#include "task.h"
#include "../mem/vmm.h"
#include "../mem/pmm.h"

// Task system with proper memory isolation for AI-OS

task_t* create_user_task(uint32_t entry_point) {
    task_t* new_task = kmalloc(sizeof(task_t));
    if (!new_task) {
        return NULL;
    }
    
    new_task->id = next_pid++;
    new_task->state = TASK_READY;
    new_task->privilege_level = 3; // Ring 3 user mode
    new_task->entry_point = entry_point;
    
    // Create isolated page directory - KEY FIX
    new_task->page_directory = create_user_page_directory();
    if (!new_task->page_directory) {
        kfree(new_task);
        return NULL;
    }
    
    // Allocate user stack in user space
    uint32_t user_stack = allocate_user_stack(new_task->page_directory);
    if (!user_stack) {
        free_user_page_directory(new_task->page_directory);
        kfree(new_task);
        return NULL;
    }
    
    new_task->esp = user_stack;
    
    // Set up user mode context
    setup_user_context(new_task, entry_point);
    
    add_to_ready_queue(new_task);
    total_tasks++;
    
    print_string("User task created with isolated memory\n");
    return new_task;
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