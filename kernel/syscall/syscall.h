#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "../task/task.h" // For cpu_state_t, if needed by handler signature

// System call numbers based on the new, cleaner design.
// This follows standard conventions (e.g., POSIX-like).
typedef enum {
  SYS_EXIT  = 0,
  SYS_PUTC  = 1, // Retaining for simplicity
  SYS_PUTS  = 2,
  SYS_READ  = 3, // New standard read syscall
  SYS_EXEC  = 4,
  SYS_YIELD = 5,
} sysno_t;

// Structure for CPU state passed to the syscall handler
// This should be defined in a header included by this one, like task.h
// Ensure cpu_state_t is defined.

// Public kernel-side functions
void syscall_init(void);
void syscall_handler(cpu_state_t* cpu);

// Kernel-side implementations of the syscalls
// These are called by the main syscall_handler
long sys_read(int fd, void* ubuf, unsigned long len);
int sys_exec(const char* path, char* argv[]);

#endif // SYSCALL_H
