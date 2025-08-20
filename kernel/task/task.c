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
    
    new_task->stack = user_stack;
    
    // Setup initial context for user mode
    setup_user_context(new_task);
    
    // Add to scheduler
    add_task_to_scheduler(new_task);
    
    return new_task;
}

uint32_t* create_user_page_directory() {
    // Allocate new page directory
    uint32_t* page_dir = alloc_page_aligned();
    if (!page_dir) {
        return NULL;
    }
    
    // Clear page directory
    memset(page_dir, 0, 0x1000);
    
    // Map kernel space (first 1GB) for system calls
    for (uint32_t i = 0; i < 256; i++) {
        page_dir[i] = get_kernel_page_table(i);
    }
    
    // User space (last 3GB) will be mapped as needed
    for (uint32_t i = 256; i < 1024; i++) {
        page_dir[i] = 0; // No access initially
    }
    
    return page_dir;
}

void setup_user_context(task_t* task) {
    // Setup initial register state for user mode
    task->registers.eax  = 0;
    task->registers.ebx  = 0;
    task->registers.ecx  = 0;
    task->registers.edx  = 0;
    task->registers.esi  = 0;
    task->registers.edi  = 0;
    task->registers.esp  = task->stack; // User stack
    task->registers.ebp  = task->stack;
    task->registers.eip  = task->entry_point;
    
    // Set user mode segments
    task->registers.cs   = USER_CODE_SELECTOR;
    task->registers.ds   = USER_DATA_SELECTOR;
    task->registers.es   = USEREDAtA_SELECTOR;
    task->registers.fs   = USER_DATA_SELECTOR;
    task->registers.gs   = USER_DATA_SELECTOR;
    task->registers.ss   = USEREDAtA_SELECTOR;
    
    // Enable interrupts in user mode
    task->registers.eflags = 0x202; // IF + IOPL 3
}

uint32_t allocate_user_stack(uint32_t* page_dir) {
    // Allocate a 4KB stack in user space
    uint32_t stack_base = 0xC0000000; // Top of user space
    uint32_t stack_page = alloc_page();
    
    if (!stack_page) {
        return 0;
    }
    
    // Map stack page into user space
    map_page(page_dir, stack_base - 0x1000, stack_page, PAGE_USER | PAGE_RW);
    
    // Return top of stack
    return stack_base;
}