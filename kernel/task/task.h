#ifndef TASK_H
#define TASK_H

#include <stdint.h>
#include "kernel/mem/vmm.h" // Inclure pour vmm_directory_t

// États possibles d'une tâche
typedef enum {
    TASK_RUNNING,
    TASK_READY,
    TASK_WAITING,
    TASK_WAITING_FOR_INPUT,
    TASK_TERMINATED
} task_state_t;

// Types de tâches
typedef enum {
    TASK_TYPE_KERNEL,
    TASK_TYPE_USER
} task_type_t;

// Structure pour sauvegarder l'état du CPU
// L'ordre doit correspondre à ce qui est poussé sur la pile par les ISR stubs.
// L'ordre des push dans l'ISR est: push ds, es, fs, gs, puis pushad.
// La pile (de l'adresse la plus basse à la plus haute) est donc: edi, esi, ..., eax, gs, fs, es, ds.
typedef struct cpu_state {
    // Pushed by PUSHAD
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    // Pushed manually by our ISR stub
    uint32_t gs, fs, es, ds;
    // Pushed by the CPU on interrupt from user mode
    uint32_t eip, cs, eflags, useresp, ss;
} cpu_state_t;

// Structure pour une tâche
typedef struct task {
    int id;
    cpu_state_t cpu_state;
    task_state_t state;
    task_type_t type;          // Type de tâche (kernel/user)
    vmm_directory_t* vmm_dir;  // Répertoire de pages de la tâche
    uint32_t kernel_stack_p;   // Pointeur vers le sommet de la pile noyau
    volatile int is_about_to_wait; // Utilisé pour signaler l'intention de se mettre en attente
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
task_t* find_task_waiting_for_input();

#endif

