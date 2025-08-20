#include "idt.h"
#include "interrupts.h"
#include "multiboot.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "task/task.h"
#include "timer.h"
#include "syscall/syscall.h"
#include "elf.h"
#include "../fs/initrd.h"

// Enhanced AI-OS kernel with real user mode transitions

void transition_to_user_mode() {
    print_string("Transitioning to user mode...\n");
    
    // Create user task from shell ELF
    task_t* shell_task = create_task_from_initrd_file("shell");
    if (!shell_task) {
        print_string("ERROR: Failed to create shell task\n");
        return;
    }
    
    print_string("Shell task created successfully\n");
    
    // Start scheduler for task switching
    init_scheduler_timer();
    
    // Begin task scheduling - this will switch to user mode
    schedule();
    
    // Should not reach here in normal operation
    print_string("ERROR: Returned from user mode unexpectedly\n");
}

void kernel_main() {
    print_string("AI-OS Kernel Starting...\n");
    
    // Initialize core systems
    init_gdt();
    init_idt();
    init_pit(100); // 100Hz timer
    init_pmm();
    init_vmm();
    init_tasking();
    init_initrd();
    
    // Enable interrupts
    sti();
    
    print_string("Core systems initialized\n");
    
    // Transition to user mode instead of kernel shell simulation
    transition_to_user_mode();
    
    // Kernel should remain in scheduling loop
    while (1) {
        halt();
    }
}
