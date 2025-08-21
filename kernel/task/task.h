#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "kernel/mem/vmm.h" // Inclure pour vmm_directory_t

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
// L'ordre doit correspondre à ce qui est poussé sur la pile par les ISR stubs
typedef struct cpu_state {
    // Pushed by pushad
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    // Pushed by our ISR stub
    uint32_t ds, es, fs, gs;
    // Pushed by the CPU on interrupt
    uint32_t eip, cs, eflags, useresp, ss;
} cpu_state_t;

// Structure pour une tâche
typedef struct task {
    int id;
    cpu_state_t cpu_state;
    task_state_t state;
    task_type_t type;          // Type de tâche (kernel/user)
    vmm_directory_t* vmm_dir;  // Répertoire de pages de la tâche
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
void schedule(cpu_state_t* cpu);
void jump_to_task(cpu_state_t* state);
void task_exit();
void task_yield();

// Fonctions utilitaires
task_t* get_task_by_id(int id);
void remove_task(task_t* task);
void add_task_to_queue(task_t* task);
int get_task_count();

#endif

