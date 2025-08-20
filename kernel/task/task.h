#ifndef TASK_H
#define TASK_H

#include <stdint.h>

// États possibles d'une tâche
typedef enum {
    TASK_RUNNING,
    TASK_READY,
    TASK_WAITING,
    TASK_TERMINATED
} task_state_t;

// Types de tâches
typedef enum {
    TASK_TYPE_KERNEL,
    TASK_TYPE_USER
} task_type_t;

// Structure pour sauvegarder l'état du CPU
typedef struct cpu_state {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp;
    uint32_t eip, esp, eflags; // Pointeur d'instruction, pointeur de pile, flags
    uint32_t cs, ds, es, fs, gs, ss; // Registres de segment
} cpu_state_t;

// Structure pour une tâche
typedef struct task {
    int id;
    cpu_state_t cpu_state;
    task_state_t state;
    task_type_t type;          // Type de tâche (kernel/user)
    uint32_t* page_directory;  // Répertoire de pages de la tâche
    uint32_t stack_base;       // Base de la pile de la tâche
    uint32_t stack_size;       // Taille de la pile
    struct task* next;         // Pour la liste chaînée de tâches
    struct task* prev;         // Liste doublement chaînée
} task_t;

// Variables globales
extern task_t* current_task;
extern task_t* task_queue;
extern int next_task_id;

// Fonctions publiques
void tasking_init();
task_t* create_task(void (*entry_point)());
task_t* create_task_from_initrd_file(const char* filename);
task_t* load_elf_task(uint8_t* elf_data, uint32_t size);
void schedule();
void task_exit();
void task_yield();

// Fonctions utilitaires
task_t* get_task_by_id(int id);
void remove_task(task_t* task);
void add_task_to_queue(task_t* task);
int get_task_count();

// Fonction assembleur pour le changement de contexte
extern void context_switch(cpu_state_t* old_state, cpu_state_t* new_state);

#endif

