/* kernel_mocks.c - Mocks simples pour les fonctions kernel manquantes */

#include <stdint.h>

// Mock pour syscall_handler
typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi;
} cpu_state_t;

void syscall_handler(cpu_state_t* state) {
    // Mock simple - ne fait rien
    (void)state;
}

// Mock pour les fonctions de task
typedef struct {
    int dummy;
} task_t;

task_t* current_task = NULL;
int g_reschedule_needed = 0;

void sys_yield(void) {
    // Mock simple
    g_reschedule_needed = 1;
}

void add_task_to_queue(task_t* task) {
    // Mock simple - ne fait rien
    (void)task;
}

// Mock pour les syscalls
void sys_putc(char c) {
    // Mock pour les tests - utilise la fonction externe mock_putc si disponible
    extern void mock_putc(char c) __attribute__((weak));
    if (mock_putc) {
        mock_putc(c);
    }
}

char sys_getc(void) {
    // Mock pour les tests - utilise la fonction externe mock_getc si disponible
    extern char mock_getc(void) __attribute__((weak));
    if (mock_getc) {
        return mock_getc();
    }
    return '\0';
}

int sys_gets(char* buffer, int size) {
    // Mock simple
    if (buffer && size > 0) {
        buffer[0] = '\0';
    }
    return 0;
}

int sys_exec(const char* path, char* const argv[]) {
    // Mock simple - retourne toujours une erreur
    (void)path;
    (void)argv;
    return -1;
}

void sys_puts(const char* str) {
    // Mock simple - utilise putc pour chaque caractère
    if (str) {
        while (*str) {
            sys_putc(*str);
            str++;
        }
    }
}

void sys_exit(int code) {
    // Mock pour les tests - utilise la fonction externe si disponible
    extern void mock_task_exit_with_code(uint32_t code) __attribute__((weak));
    if (mock_task_exit_with_code) {
        mock_task_exit_with_code((uint32_t)code);
    }
}

// Mocks pour les fonctions de task management
void tasking_init(void) {
    // Mock simple
}

task_t* create_task(void (*entry_point)(void), const char* name, int priority) {
    // Mock simple - retourne NULL
    (void)entry_point;
    (void)name;
    (void)priority;
    return NULL;
}

task_t* task_queue = NULL;

void schedule(void) {
    // Mock simple
}

// Fonctions additionnelles pour test_task
task_t* find_task_waiting_for_input(void) {
    // Mock simple
    return NULL;
}

int get_task_count(void) {
    // Mock simple
    return 0;
}

void task_yield(void) {
    // Mock simple
}

void task_exit(void) {
    // Mock simple
}

// Variables globales nécessaires
int next_task_id = 1;

// Fonctions additionnelles pour test_task
void remove_task(task_t* task) {
    // Mock simple
    (void)task;
}

task_t* get_task_by_id(int id) {
    // Mock simple
    (void)id;
    return NULL;
}

// Mock pour syscall_init
void syscall_init(void) {
    // Mock simple - ne fait rien
}
