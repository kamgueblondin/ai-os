/* kernel_mocks.c - Mocks et simulations pour le kernel AI-OS dans l'environnement de test */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "kernel/mem/vmm.h"
#include "kernel/task/task.h"
#include "kernel/syscall/syscall.h"

// === GESTION MÉMOIRE MOCKS ===
extern void* test_malloc(size_t size);
extern void test_free(void* ptr);

// === TASK MANAGEMENT MOCK SIMULATION (LINEAR DOUBLE-LINKED LIST) ===
task_t* current_task = NULL;
task_t* task_queue = NULL;
int next_task_id = 1;
volatile int g_reschedule_needed = 0;

// Global counter for task switches
int mock_task_switch_called = 0;

void tasking_init(void) {
    current_task = NULL;
    task_queue = NULL;
    next_task_id = 1;
    g_reschedule_needed = 0;
}

task_t* create_task(void (*entry_point)(void)) {
    task_t* task = (task_t*)test_malloc(sizeof(task_t));
    if (!task) return NULL;
    memset(task, 0, sizeof(task_t));
    task->id = next_task_id++;
    task->state = TASK_READY;
    task->type = TASK_TYPE_KERNEL;
    task->vmm_dir = (vmm_directory_t*)test_malloc(sizeof(vmm_directory_t));
    task->kernel_stack_p = 0;
    task->next = NULL;
    task->prev = NULL;
    (void)entry_point;
    return task;
}

void add_task_to_queue(task_t* task) {
    if (!task) return;
    if (!task_queue) {
        task_queue = task;
        task->next = NULL;
        task->prev = NULL;
    } else {
        task_t* tail = task_queue;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = task;
        task->prev = tail;
        task->next = NULL;
    }
}

void remove_task(task_t* task) {
    if (!task || !task_queue) return;
    if (task->prev) {
        task->prev->next = task->next;
    } else {
        task_queue = task->next;
    }
    if (task->next) {
        task->next->prev = task->prev;
    }
    if (current_task == task) {
        current_task = task_queue;
    }
}

task_t* get_task_by_id(int id) {
    task_t* temp = task_queue;
    while (temp) {
        if (temp->id == id) return temp;
        temp = temp->next;
    }
    return NULL;
}

task_t* find_task_waiting_for_input(void) {
    task_t* temp = task_queue;
    while (temp) {
        if (temp->state == TASK_WAITING_FOR_INPUT) return temp;
        temp = temp->next;
    }
    return NULL;
}

int get_task_count(void) {
    int count = 0;
    task_t* temp = task_queue;
    while (temp) {
        count++;
        temp = temp->next;
    }
    return count;
}

void schedule(cpu_state_t* cpu) {
    mock_task_switch_called++;
    if (!current_task || !task_queue) return;

    // Linear round-robin scheduling simulation
    task_t* next_task = current_task->next;
    if (!next_task) {
        next_task = task_queue;
    }

    while (next_task != current_task) {
        if (next_task->state == TASK_READY || next_task->state == TASK_RUNNING) {
            break;
        }
        next_task = next_task->next;
        if (!next_task) {
            next_task = task_queue;
        }
    }

    if (cpu && current_task != next_task) {
        // Mock save and switch state
        current_task->cpu_state = *cpu;
        *cpu = next_task->cpu_state;
    }

    if (current_task->state == TASK_RUNNING && current_task != next_task) {
        current_task->state = TASK_READY;
    }
    current_task = next_task;
    current_task->state = TASK_RUNNING;
    g_reschedule_needed = 0;
}

void task_yield(void) {
    g_reschedule_needed = 1;
}

void task_exit(void) {
    if (current_task) {
        current_task->state = TASK_TERMINATED;
    }
}

// === SYSTEM CALLS MOCK SIMULATION ===

void sys_yield(void) {
    g_reschedule_needed = 1;
}

void syscall_init(void) {
    // Mock simple - ne fait rien
}

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

void sys_gets(char* buffer, uint32_t size) {
    if (!buffer || size == 0) return;
    uint32_t i = 0;
    while (i < size - 1) {
        char c = sys_getc();
        if (c == 0 || c == '\n') {
            break;
        }
        buffer[i++] = c;
    }
    buffer[i] = '\0';
}

int sys_exec(const char* path, char* argv[]) {
    // Mock simple - retourne toujours une erreur
    (void)path;
    (void)argv;
    return -1;
}

void sys_puts(const char* str) {
    // Mock simple - utilise putc pour chaque caractère
    // Security check: Protect against invalid/unmapped address dereferences in host user-space
    if (str && (uint32_t)str < 0xC0000000 && (uint32_t)str != 0) {
        while (*str) {
            sys_putc(*str);
            str++;
        }
    }
}

void sys_exit(uint32_t code) {
    // Mock pour les tests - utilise la fonction externe si disponible
    extern void mock_task_exit_with_code(uint32_t code) __attribute__((weak));
    if (mock_task_exit_with_code) {
        mock_task_exit_with_code(code);
    }
}

// Mock syscall dispatcher to dispatch registers
void syscall_handler(cpu_state_t* state) {
    if (!state) return;

    // Simulate real kernel/syscall/syscall.c dispatching behavior for the tests
    switch (state->eax) {
        case SYS_EXIT:
            sys_exit(state->ebx);
            break;
        case SYS_PUTC:
            sys_putc(state->ebx);
            break;
        case SYS_GETC:
            state->eax = sys_getc();
            break;
        case SYS_PUTS:
            sys_puts((const char*)state->ebx);
            break;
        case SYS_GETS:
            sys_gets((char*)state->ebx, state->ecx);
            break;
        case SYS_YIELD:
            sys_yield();
            break;
        default:
            break;
    }
}
