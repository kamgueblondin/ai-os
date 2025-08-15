#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "../task/task.h"

// Numéros des appels système
#define SYS_EXIT    0
#define SYS_PUTC    1
#define SYS_GETC    2
#define SYS_PUTS    3
#define SYS_YIELD   4

// Structure pour passer les paramètres des syscalls
typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi;
} syscall_params_t;

// Fonctions publiques
void syscall_init();
void syscall_handler(cpu_state_t* cpu);

// Fonctions utilitaires pour les syscalls
void sys_exit(uint32_t exit_code);
void sys_putc(char c);
char sys_getc();
void sys_puts(const char* str);
void sys_yield();

#endif

