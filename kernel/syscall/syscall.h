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
#define SYS_GETS    5  // Nouveau: Lire une ligne depuis le clavier
#define SYS_EXEC    6  // Nouveau: Exécuter un programme

// Nombre total d'appels système
#define MAX_SYSCALLS 7

// Structure pour passer les paramètres des syscalls
typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi;
} syscall_params_t;

// Fonctions publiques
void syscall_init();
void syscall_handler(cpu_state_t* cpu);

// Fonctions utilitaires pour les syscalls existants
void sys_exit(uint32_t exit_code);
void sys_putc(char c);
char sys_getc();
void sys_puts(const char* str);
void sys_yield();

#include <stddef.h> // Pour size_t

// Définition de __user pour l'annotation de l'espace d'adressage
#ifndef __user
#define __user
#endif

// Nouveaux appels système
int sys_gets(char __user *dst, size_t maxlen);
int sys_exec(const char* path, char* argv[]);

// Ajoute un caractère au buffer d'entrée global du clavier.
void keyboard_add_char_to_buffer(char c);

#endif

