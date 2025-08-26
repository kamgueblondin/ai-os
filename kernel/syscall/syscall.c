#include "syscall.h"
#include "../kernel.h"
#include "../interrupts.h"
#include "../task/task.h"
#include "../keyboard.h"
#include "../elf.h"
#include "../../fs/initrd.h"
#include "../mem/string.h"

// External functions from kernel.c or elsewhere
extern void print_string_serial(const char* str);
extern void print_char(char c, int x, int y, char color);
extern void write_serial(char a);


// ==============================================================================
// SYSCALL IMPLEMENTATIONS
// ==============================================================================

/**
 * @brief Reads from a file descriptor.
 *
 * Currently, only fd=0 (stdin) is supported, which reads from the keyboard.
 * This is a simplified, polling version. It is inefficient but functional
 * without a full scheduler sleep/wake mechanism.
 *
 * @param fd The file descriptor (must be 0).
 * @param ubuf Pointer to the user-space buffer.
 * @param len Maximum number of bytes to read.
 * @return The number of bytes read, or -1 on error.
 */
long sys_read(int fd, void* ubuf, unsigned long len) {
    // For now, we only support stdin
    if (fd != 0 || !ubuf || len == 0) {
        return -1;
    }

    // This is unsafe. A real kernel would use copy_to_user and validate the pointer.
    char* kbuf = (char*)ubuf;
    unsigned long n_read = 0;

    while (n_read < len) {
        int c = kbd_pop_char();
        if (c != -1) {
            // Character available
            kbuf[n_read] = (char)c;
            n_read++;
            if (c == '\n') {
                break; // Stop reading on newline
            }
        } else {
            // No character available, yield CPU and try again
            asm volatile("int $0x30"); // Yield/schedule
        }
    }

    return n_read;
}

/**
 * @brief Executes a program from the initrd.
 *
 * This is a blocking implementation. It creates a new task and waits for it
 * to terminate.
 *
 * @param path The path to the executable in the initrd.
 * @param argv Argument vector (currently unused).
 * @return 0 on success, -1 on failure.
 */
int sys_exec(const char* path, char* argv[]) {
    (void)argv; // argv is not used in this version.

    // This is a placeholder for a more robust check of user pointers
    if (!path) {
        return -1;
    }

    task_t* new_task = create_task_from_initrd_file(path);
    if (!new_task) {
        return -1; // Failed to create the task
    }

    // Block until the new task finishes. A real shell would run it in the background.
    while (new_task->state != TASK_TERMINATED) {
        asm volatile("int $0x30"); // Yield
    }

    // TODO: Clean up the terminated task resources
    return 0; // Success
}


// ==============================================================================
// SYSCALL DISPATCHER
// ==============================================================================

void syscall_handler(cpu_state_t* cpu) {
    // Re-enable interrupts as soon as possible, but after saving state.
    // The ISR stub in assembly should save all registers before calling this C handler.
    asm volatile("sti");

    switch (cpu->eax) {
        case SYS_EXIT:
            // TODO: Add proper exit code handling
            current_task->state = TASK_TERMINATED;
            asm volatile("int $0x30"); // Yield, we won't return
            break;

        case SYS_PUTC:
            print_char((char)cpu->ebx, -1, -1, 0x0F);
            write_serial((char)cpu->ebx);
            break;

        case SYS_PUTS:
            {
                // This is unsafe, a proper kernel would use copy_from_user
                char* str = (char*)cpu->ebx;
                if (str) {
                    // Limiting the length to prevent kernel hangs
                    for (int i = 0; i < 4096 && str[i] != '\0'; i++) {
                        print_char(str[i], -1, -1, 0x0F);
                        write_serial(str[i]);
                    }
                }
            }
            break;

        case SYS_READ:
            // eax = sys_read(ebx: fd, ecx: buf, edx: len)
            cpu->eax = sys_read((int)cpu->ebx, (void*)cpu->ecx, (unsigned long)cpu->edx);
            break;

        case SYS_EXEC:
            // eax = sys_exec(ebx: path, ecx: argv)
            cpu->eax = sys_exec((const char*)cpu->ebx, (char**)cpu->ecx);
            break;

        case SYS_YIELD:
            asm volatile("int $0x30"); // Yield the CPU
            break;

        default:
            // Unknown syscall number
            cpu->eax = -1; // Return an error
            break;
    }
}

void syscall_init() {
    // Register the syscall handler for interrupt 0x80
    register_interrupt_handler(0x80, (interrupt_handler_t)syscall_handler);
}

