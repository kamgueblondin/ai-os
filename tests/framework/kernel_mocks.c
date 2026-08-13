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

int task_kill(int pid) {
    task_t* t;
    if (pid == 0) return -2;
    t = get_task_by_id(pid);
    if (!t) return -1;
    if (current_task && t->id == current_task->id) return -3;
    t->state = TASK_TERMINATED;
    remove_task(t);
    return 0;
}

int task_fill_ps(os_proc_t* out, int max_n) {
    int count = 0;
    task_t* t = task_queue;
    if (!out || max_n <= 0) return 0;
    while (t && count < max_n) {
        int i = 0;
        out[count].pid = t->id;
        out[count].state = (t->state == TASK_RUNNING) ? OS_TASK_RUNNING :
                           (t->state == TASK_READY) ? OS_TASK_READY :
                           (t->state == TASK_TERMINATED) ? OS_TASK_TERMINATED : OS_TASK_WAITING;
        out[count].type = (t->type == TASK_TYPE_USER) ? OS_TASK_USER : OS_TASK_KERNEL;
        while (t->name[i] && i < OS_PROC_NAME_MAX - 1) {
            out[count].name[i] = t->name[i];
            i++;
        }
        out[count].name[i] = '\0';
        count++;
        t = t->next;
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
    (void)path;
    (void)argv;
    return -1;
}

static struct {
    const char* name;
    const char* data;
    uint32_t size;
} mock_initrd[] = {
    {"hello.txt", "hello from initrd\n", 18},
    {"bin/shell", "ELF", 3},
};

int sys_listdir(const char* path, os_dirent_t* out, int max_n) {
    int count = 0;
    const char* p = path ? path : "/";
    if (!out || max_n <= 0) return -1;
    if (p[0] == '/') p++;
    if (p[0] == '\0' || (p[0] == '.' && p[1] == '\0')) {
        int has_bin = 0;
        for (unsigned i = 0; i < sizeof(mock_initrd) / sizeof(mock_initrd[0]) && count < max_n; i++) {
            const char* n = mock_initrd[i].name;
            const char* slash = 0;
            const char* s = n;
            while (*s) {
                if (*s == '/') slash = s;
                s++;
            }
            if (slash) {
                if (!has_bin && count < max_n) {
                    out[count].name[0] = 'b';
                    out[count].name[1] = 'i';
                    out[count].name[2] = 'n';
                    out[count].name[3] = '\0';
                    out[count].size = 0;
                    out[count].flags = OS_DIRENT_DIR;
                    count++;
                    has_bin = 1;
                }
            } else {
                int k = 0;
                while (n[k] && k < OS_NAME_MAX - 1) {
                    out[count].name[k] = n[k];
                    k++;
                }
                out[count].name[k] = '\0';
                out[count].size = mock_initrd[i].size;
                out[count].flags = OS_DIRENT_FILE;
                count++;
            }
        }
        return count;
    }
    return 0;
}

int sys_readfile(const char* path, char* buf, uint32_t max) {
    const char* p = path ? path : "";
    if (!buf || max == 0) return -1;
    if (p[0] == '/') p++;
    for (unsigned i = 0; i < sizeof(mock_initrd) / sizeof(mock_initrd[0]); i++) {
        if (strcmp(p, mock_initrd[i].name) == 0) {
            uint32_t n = mock_initrd[i].size;
            if (n > max) n = max;
            memcpy(buf, mock_initrd[i].data, n);
            return (int)n;
        }
    }
    return -1;
}

int sys_getpid(void) {
    return current_task ? current_task->id : 0;
}

int sys_ps(os_proc_t* out, int max_n) {
    return task_fill_ps(out, max_n);
}

int sys_kill(int pid) {
    return task_kill(pid);
}

uint32_t sys_ticks(void) {
    return 42;
}

int sys_meminfo(os_meminfo_t* info) {
    if (!info) return -1;
    info->total_pages = 32768;
    info->used_pages = 100;
    info->free_pages = 32668;
    return 0;
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
        case SYS_LISTDIR:
            state->eax = (uint32_t)sys_listdir((const char*)state->ebx, (os_dirent_t*)state->ecx, (int)state->edx);
            break;
        case SYS_READFILE:
            state->eax = (uint32_t)sys_readfile((const char*)state->ebx, (char*)state->ecx, state->edx);
            break;
        case SYS_GETPID:
            state->eax = (uint32_t)sys_getpid();
            break;
        case SYS_PS:
            state->eax = (uint32_t)sys_ps((os_proc_t*)state->ebx, (int)state->ecx);
            break;
        case SYS_KILL:
            state->eax = (uint32_t)sys_kill((int)state->ebx);
            break;
        case SYS_TICKS:
            state->eax = sys_ticks();
            break;
        case SYS_MEMINFO:
            state->eax = (uint32_t)sys_meminfo((os_meminfo_t*)state->ebx);
            break;
        default:
            break;
    }
}
