/* kernel_mocks.c - Mocks et simulations pour le kernel AI-OS dans l'environnement de test */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "kernel/mem/vmm.h"
#include "kernel/task/task.h"
#include "kernel/syscall/syscall.h"
#include "kernel/ata.h"
#include "fs/overlay.h"

// === GESTION MÉMOIRE MOCKS ===
extern void* test_malloc(size_t size);
extern void test_free(void* ptr);
extern uint32_t mock_timer_get_ticks(void);

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
    task->priority = OS_TASK_PRIORITY_NORMAL;
    task->vmm_dir = (vmm_directory_t*)test_malloc(sizeof(vmm_directory_t));
    task->kernel_stack_p = 0;
    task->parent_pid = -1;
    task->created_ticks = mock_timer_get_ticks();
    task->last_scheduled_ticks = task->created_ticks;
    task->run_ticks = 0U;
    task->switch_count = 0U;
    task->last_child_pid = -1;
    task->last_child_exit_code = 0;
    task->last_child_exit_reason = 0U;
    task->last_child_finished_ticks = 0U;
    task->child_exit_history_start = 0U;
    task->child_exit_history_count = 0U;
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

void task_reparent_children(task_t* departing) {
    task_t* t = task_queue;
    if (!departing) return;
    while (t) {
        if (t != departing && t->parent_pid == departing->id) {
            t->parent_pid = departing->parent_pid;
        }
        t = t->next;
    }
}

uint32_t task_count_direct_children(int pid) {
    task_t* t = task_queue;
    uint32_t count = 0U;
    while (t) {
        if (t->parent_pid == pid) count++;
        t = t->next;
    }
    return count;
}

int task_can_create_child(int pid) {
    if (!get_task_by_id(pid)) return OS_TASK_NOT_FOUND;
    if (task_count_direct_children(pid) >= OS_TASK_CHILD_CAPACITY) {
        return OS_TASK_CHILD_LIMIT;
    }
    return 0;
}

int task_can_create_global(void) {
    return (uint32_t)get_task_count() >= OS_TASK_GLOBAL_CAPACITY ? OS_TASK_GLOBAL_LIMIT : 0;
}

int task_wait_for_child(int requester_pid, int child_pid) {
    task_t* parent = get_task_by_id(requester_pid);
    task_t* child = get_task_by_id(child_pid);
    if (!parent || !child) return OS_TASK_NOT_FOUND;
    if (child->parent_pid != requester_pid) return OS_TASK_NOT_CHILD;
    if (child->waiter_pid != 0 && child->waiter_pid != requester_pid) {
        return OS_TASK_CONTROL_DENIED;
    }
    child->waiter_pid = requester_pid;
    parent->state = TASK_WAITING;
    return 0;
}

int task_kill(int requester_pid, int pid) {
    task_t* t;
    if (pid == 0) return -2;
    t = get_task_by_id(pid);
    if (!t) return -1;
    if (requester_pid == pid) return -3;
    if (requester_pid != t->parent_pid) return OS_TASK_CONTROL_DENIED;
    task_report_parent_exit(t, OS_TASK_EXIT_KILLED, OS_TASK_EVENT_KILLED);
    task_wake_waiter(t);
    task_reparent_children(t);
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
        out[count].parent_pid = t->parent_pid;
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

int task_fill_metrics(int pid, os_task_metrics_t* out) {
    task_t* t;
    uint32_t now;
    uint32_t run;
    if (!out) return OS_TASK_NOT_FOUND;
    memset(out, 0, sizeof(*out));
    t = get_task_by_id(pid);
    if (!t) return OS_TASK_NOT_FOUND;
    now = mock_timer_get_ticks();
    run = t->run_ticks;
    if (t == current_task && t->state == TASK_RUNNING && now >= t->last_scheduled_ticks) {
        run += now - t->last_scheduled_ticks;
    }
    out->pid = t->id;
    out->parent_pid = t->parent_pid;
    out->state = (t->state == TASK_RUNNING) ? OS_TASK_RUNNING :
                 (t->state == TASK_READY) ? OS_TASK_READY :
                 (t->state == TASK_TERMINATED) ? OS_TASK_TERMINATED : OS_TASK_WAITING;
    out->type = (t->type == TASK_TYPE_USER) ? OS_TASK_USER : OS_TASK_KERNEL;
    out->priority = t->priority;
    out->created_ticks = t->created_ticks;
    out->age_ticks = now >= t->created_ticks ? now - t->created_ticks : 0U;
    out->run_ticks = run;
    out->switch_count = t->switch_count;
    out->direct_children = task_count_direct_children(t->id);
    return 0;
}

int task_fill_capacity(os_task_capacity_t* out) {
    uint32_t active;
    if (!out) return OS_TASK_NOT_FOUND;
    active = (uint32_t)get_task_count();
    out->active = active;
    out->capacity = OS_TASK_GLOBAL_CAPACITY;
    out->available = active < OS_TASK_GLOBAL_CAPACITY ? OS_TASK_GLOBAL_CAPACITY - active : 0U;
    return 0;
}

int task_set_priority(int requester_pid, int pid, uint32_t priority) {
    task_t* t;
    if (priority < OS_TASK_PRIORITY_LOW || priority > OS_TASK_PRIORITY_HIGH) {
        return OS_TASK_BAD_PRIORITY;
    }
    t = get_task_by_id(pid);
    if (!t) return OS_TASK_NOT_FOUND;
    if (requester_pid != t->id && requester_pid != t->parent_pid) {
        return OS_TASK_CONTROL_DENIED;
    }
    t->priority = priority;
    return 0;
}

int task_set_name(int requester_pid, int pid, const char* name) {
    task_t* t;
    int i = 0;
    if (!name || !name[0]) return OS_TASK_BAD_NAME;
    while (name[i]) {
        unsigned char c = (unsigned char)name[i];
        if (i >= OS_PROC_NAME_MAX - 1 || c < 32U || c > 126U) return OS_TASK_BAD_NAME;
        i++;
    }
    t = get_task_by_id(pid);
    if (!t) return OS_TASK_NOT_FOUND;
    if (requester_pid != t->id && requester_pid != t->parent_pid) return OS_TASK_CONTROL_DENIED;
    for (i = 0; name[i]; i++) t->name[i] = name[i];
    t->name[i] = '\0';
    return 0;
}

int task_get_child_result(int requester_pid, int child_pid, os_task_exit_result_t* out) {
    task_t* parent;
    if (!out) return OS_TASK_NO_CHILD_RESULT;
    parent = get_task_by_id(requester_pid);
    if (!parent) return OS_TASK_NOT_FOUND;
    if (parent->last_child_pid != child_pid) return OS_TASK_NO_CHILD_RESULT;
    out->child_pid = parent->last_child_pid;
    out->exit_code = parent->last_child_exit_code;
    out->reason = parent->last_child_exit_reason;
    out->finished_ticks = parent->last_child_finished_ticks;
    return 0;
}

int task_fill_child_result_history(int requester_pid, os_task_exit_history_t* out) {
    task_t* parent;
    uint32_t i;
    if (!out) return OS_TASK_NO_CHILD_RESULT;
    memset(out, 0, sizeof(*out));
    parent = get_task_by_id(requester_pid);
    if (!parent) return OS_TASK_NOT_FOUND;
    out->count = parent->child_exit_history_count;
    for (i = 0U; i < out->count; i++) {
        uint32_t index = (parent->child_exit_history_start + i) % OS_TASK_EXIT_HISTORY_CAPACITY;
        out->entries[i] = parent->child_exit_history[index];
    }
    return 0;
}

void schedule(cpu_state_t* cpu) {
    task_t* next_task = NULL;
    task_t* candidate;
    uint32_t best_priority = 0U;
    mock_task_switch_called++;
    if (!current_task || !task_queue) return;

    /* Le parcours commence après la tâche courante : à priorité égale,
     * l’ordre de la liste reste donc un round-robin déterministe. */
    candidate = current_task;
    do {
        candidate = candidate->next ? candidate->next : task_queue;
        if (candidate->state == TASK_READY && candidate->type == TASK_TYPE_USER &&
            (!next_task || candidate->priority > best_priority)) {
            next_task = candidate;
            best_priority = candidate->priority;
        }
    } while (candidate != current_task);

    if (!next_task) {
        candidate = current_task;
        do {
            candidate = candidate->next ? candidate->next : task_queue;
            if (candidate->state == TASK_READY || candidate->state == TASK_RUNNING) {
                next_task = candidate;
                break;
            }
        } while (candidate != current_task);
    }
    if (!next_task) next_task = current_task;

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

void task_wake_waiter(task_t* child) {
    task_t* parent;
    if (!child || child->waiter_pid <= 0) return;
    parent = get_task_by_id(child->waiter_pid);
    if (parent && parent->state == TASK_WAITING) {
        parent->state = TASK_READY;
    }
    child->waiter_pid = 0;
}

static void mock_task_event_send(ipc_endpoint_t* endpoint, const os_ipc_payload_t* payload) {
    os_ipc_message_t* message;
    uint32_t i;
    if (!endpoint || !payload || payload->size > OS_IPC_MAX_DATA ||
        endpoint->count >= IPC_ENDPOINT_CAPACITY) return;
    message = &endpoint->messages[endpoint->write_index];
    message->sender_pid = 0;
    message->type = payload->type;
    message->size = payload->size;
    message->request_id = payload->request_id;
    for (i = 0U; i < payload->size; i++) message->data[i] = payload->data[i];
    endpoint->write_index = (endpoint->write_index + 1U) % IPC_ENDPOINT_CAPACITY;
    endpoint->count++;
}

void task_report_parent_exit(task_t* child, int exit_code, uint32_t reason) {
    task_t* parent;
    os_ipc_payload_t payload;
    os_task_exit_result_t result;
    uint32_t index;
    if (!child || child->parent_pid <= 0) return;
    parent = get_task_by_id(child->parent_pid);
    if (!parent || parent->type != TASK_TYPE_USER || parent->state == TASK_TERMINATED) return;
    result.child_pid = child->id;
    result.exit_code = exit_code;
    result.reason = reason;
    result.finished_ticks = mock_timer_get_ticks();
    parent->last_child_pid = result.child_pid;
    parent->last_child_exit_code = result.exit_code;
    parent->last_child_exit_reason = result.reason;
    parent->last_child_finished_ticks = result.finished_ticks;
    if (parent->child_exit_history_count < OS_TASK_EXIT_HISTORY_CAPACITY) {
        index = (parent->child_exit_history_start + parent->child_exit_history_count) %
                OS_TASK_EXIT_HISTORY_CAPACITY;
        parent->child_exit_history_count++;
    } else {
        index = parent->child_exit_history_start;
        parent->child_exit_history_start = (parent->child_exit_history_start + 1U) %
                                           OS_TASK_EXIT_HISTORY_CAPACITY;
    }
    parent->child_exit_history[index] = result;
    if (os_task_make_event(&payload, child->id, reason) != 0) return;
    mock_task_event_send(&parent->ipc_endpoint, &payload);
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
    overlay_init();
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

static void mock_ird_norm(const char* in, char* out, int max) {
    int i = 0;
    if (!in) {
        out[0] = '\0';
        return;
    }
    if (in[0] == '.' && in[1] == '/') in += 2;
    while (*in == '/') in++;
    while (in[i] && i < max - 1) {
        out[i] = in[i];
        i++;
    }
    out[i] = '\0';
    while (i > 0 && out[i - 1] == '/') {
        out[--i] = '\0';
    }
}

static void mock_ird_basename(const char* path, char* out, int max) {
    const char* base = path ? path : "";
    int i;
    for (i = 0; path && path[i]; i++) {
        if (path[i] == '/') base = path + i + 1;
    }
    i = 0;
    while (base[i] && i < max - 1) {
        out[i] = base[i];
        i++;
    }
    out[i] = '\0';
}

int initrd_is_file(const char* path) {
    char want[64];
    unsigned i;
    mock_ird_norm(path, want, 64);
    if (!want[0]) return 0;
    for (i = 0; i < sizeof(mock_initrd) / sizeof(mock_initrd[0]); i++) {
        if (strcmp(want, mock_initrd[i].name) == 0) return 1;
    }
    return 0;
}

int initrd_read_into(const char* path, char* buf, uint32_t max) {
    char want[64];
    unsigned i;
    mock_ird_norm(path, want, 64);
    if (!want[0] || !buf || max == 0) return -1;
    for (i = 0; i < sizeof(mock_initrd) / sizeof(mock_initrd[0]); i++) {
        if (strcmp(want, mock_initrd[i].name) == 0) {
            uint32_t copy = mock_initrd[i].size;
            if (copy > max) copy = max;
            memcpy(buf, mock_initrd[i].data, copy);
            return (int)copy;
        }
    }
    return -1;
}

int initrd_is_dir(const char* path) {
    char want[64];
    unsigned i;
    int plen;
    mock_ird_norm(path, want, 64);
    if (!want[0]) return 1;
    plen = (int)strlen(want);
    for (i = 0; i < sizeof(mock_initrd) / sizeof(mock_initrd[0]); i++) {
        const char* n = mock_initrd[i].name;
        int j = 0;
        while (j < plen && n[j] == want[j]) j++;
        if (j == plen && n[j] == '/') return 1;
    }
    return 0;
}

int initrd_stat(const char* path, os_dirent_t* out) {
    char want[64];
    unsigned i;
    if (!path || !out) return -1;
    mock_ird_norm(path, want, 64);
    if (!want[0]) {
        out->name[0] = '/';
        out->name[1] = '\0';
        out->size = 0;
        out->flags = OS_DIRENT_DIR;
        return 0;
    }
    if (initrd_is_dir(want)) {
        mock_ird_basename(want, out->name, OS_NAME_MAX);
        out->size = 0;
        out->flags = OS_DIRENT_DIR;
        return 0;
    }
    for (i = 0; i < sizeof(mock_initrd) / sizeof(mock_initrd[0]); i++) {
        if (strcmp(want, mock_initrd[i].name) == 0) {
            mock_ird_basename(want, out->name, OS_NAME_MAX);
            out->size = mock_initrd[i].size;
            out->flags = OS_DIRENT_FILE;
            return 0;
        }
    }
    return -1;
}

static int mock_initrd_listdir(const char* path, os_dirent_t* out, int max_n) {
    char prefix[64];
    int plen;
    int count = 0;
    unsigned i;
    if (!out || max_n <= 0) return -1;
    mock_ird_norm(path ? path : "/", prefix, 64);
    plen = (int)strlen(prefix);
    for (i = 0; i < sizeof(mock_initrd) / sizeof(mock_initrd[0]) && count < max_n; i++) {
        const char* have = mock_initrd[i].name;
        const char* rest;
        int slash = -1;
        int k;
        if (plen == 0) {
            rest = have;
        } else {
            int j = 0;
            while (j < plen && have[j] == prefix[j]) j++;
            if (j != plen || have[j] != '/') continue;
            rest = have + plen + 1;
        }
        if (!rest[0]) continue;
        k = 0;
        while (rest[k]) {
            if (rest[k] == '/') {
                slash = k;
                break;
            }
            k++;
        }
        if (slash >= 0) {
            char dname[OS_NAME_MAX];
            int dup = 0;
            int e;
            int nlen = slash;
            if (nlen >= OS_NAME_MAX) nlen = OS_NAME_MAX - 1;
            for (e = 0; e < nlen; e++) dname[e] = rest[e];
            dname[nlen] = '\0';
            for (e = 0; e < count; e++) {
                if (out[e].flags == OS_DIRENT_DIR && strcmp(out[e].name, dname) == 0) {
                    dup = 1;
                    break;
                }
            }
            if (!dup) {
                int t = 0;
                while (dname[t] && t < OS_NAME_MAX - 1) {
                    out[count].name[t] = dname[t];
                    t++;
                }
                out[count].name[t] = '\0';
                out[count].size = 0;
                out[count].flags = OS_DIRENT_DIR;
                count++;
            }
        } else {
            int t = 0;
            while (rest[t] && t < OS_NAME_MAX - 1) {
                out[count].name[t] = rest[t];
                t++;
            }
            out[count].name[t] = '\0';
            out[count].size = mock_initrd[i].size;
            out[count].flags = OS_DIRENT_FILE;
            count++;
        }
    }
    return count;
}

int sys_listdir(const char* path, os_dirent_t* out, int max_n) {
    int n;
    if (!path || !out || max_n <= 0) return -1;
    if (!overlay_is_dir(path) && !initrd_is_dir(path)) return -1;
    n = mock_initrd_listdir(path, out, max_n);
    if (n < 0) n = 0;
    return overlay_listdir(path, out, n, max_n);
}

int sys_readfile(const char* path, char* buf, uint32_t max) {
    int n;
    char want[64];
    unsigned i;
    if (!path || !buf || max == 0) return -1;
    n = overlay_read(path, buf, max);
    if (n >= 0) return n;
    if (n == OV_ERR_ISDIR) return n;
    mock_ird_norm(path, want, 64);
    for (i = 0; i < sizeof(mock_initrd) / sizeof(mock_initrd[0]); i++) {
        if (strcmp(want, mock_initrd[i].name) == 0) {
            uint32_t copy = mock_initrd[i].size;
            if (copy > max) copy = max;
            memcpy(buf, mock_initrd[i].data, copy);
            return (int)copy;
        }
    }
    return -1;
}

int sys_mkdir(const char* path) {
    if (!path) return -1;
    return overlay_mkdir(path);
}

int sys_unlink(const char* path) {
    if (!path) return -1;
    return overlay_unlink(path);
}

int sys_writefile(const char* path, const char* buf, uint32_t n) {
    if (!path || (n > 0 && !buf)) return -1;
    return overlay_write(path, buf, n);
}

int sys_stat(const char* path, os_dirent_t* out) {
    if (!path || !out) return -1;
    if (overlay_stat(path, out) == OV_OK) return 0;
    return initrd_stat(path, out);
}

int sys_rename(const char* oldpath, const char* newpath) {
    if (!oldpath || !newpath) return -1;
    return overlay_rename(oldpath, newpath);
}

int sys_copy(const char* src, const char* dst) {
    if (!src || !dst) return -1;
    return overlay_copy(src, dst);
}

int sys_append(const char* path, const char* buf, uint32_t n) {
    if (!path || (n > 0 && !buf)) return -1;
    return overlay_append(path, buf, n);
}

int sys_getpid(void) {
    return current_task ? current_task->id : 0;
}

int sys_ps(os_proc_t* out, int max_n) {
    return task_fill_ps(out, max_n);
}

int sys_kill(int pid) {
    if (!current_task) return OS_TASK_CONTROL_DENIED;
    return task_kill(current_task->id, pid);
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
            if (current_task) task_report_parent_exit(current_task, (int)state->ebx,
                                                      OS_TASK_EVENT_EXITED);
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
        case SYS_TASK_METRICS:
            state->eax = (uint32_t)task_fill_metrics((int)state->ebx, (os_task_metrics_t*)state->ecx);
            break;
        case SYS_TASK_SET_PRIORITY:
            state->eax = (uint32_t)task_set_priority(current_task ? current_task->id : -1,
                                                      (int)state->ebx, state->ecx);
            break;
        case SYS_TASK_WAIT:
            state->eax = (uint32_t)task_wait_for_child(current_task ? current_task->id : -1,
                                                        (int)state->ebx);
            break;
        case SYS_TASK_SET_NAME:
            state->eax = (uint32_t)task_set_name(current_task ? current_task->id : -1,
                                                  (int)state->ebx, (const char*)state->ecx);
            break;
        case SYS_TASK_CAPACITY:
            state->eax = (uint32_t)task_fill_capacity((os_task_capacity_t*)state->ebx);
            break;
        case SYS_TASK_CHILD_RESULT:
            state->eax = (uint32_t)task_get_child_result(current_task ? current_task->id : -1,
                                                         (int)state->ebx,
                                                         (os_task_exit_result_t*)state->ecx);
            break;
        case SYS_TASK_CHILD_RESULT_LIST:
            state->eax = (uint32_t)task_fill_child_result_history(current_task ? current_task->id : -1,
                                                                   (os_task_exit_history_t*)state->ebx);
            break;
        case SYS_MKDIR:
            state->eax = (uint32_t)sys_mkdir((const char*)state->ebx);
            break;
        case SYS_UNLINK:
            state->eax = (uint32_t)sys_unlink((const char*)state->ebx);
            break;
        case SYS_WRITEFILE:
            state->eax = (uint32_t)sys_writefile((const char*)state->ebx, (const char*)state->ecx, state->edx);
            break;
        case SYS_STAT:
            state->eax = (uint32_t)sys_stat((const char*)state->ebx, (os_dirent_t*)state->ecx);
            break;
        case SYS_RENAME:
            state->eax = (uint32_t)sys_rename((const char*)state->ebx, (const char*)state->ecx);
            break;
        case SYS_COPY:
            state->eax = (uint32_t)sys_copy((const char*)state->ebx, (const char*)state->ecx);
            break;
        case SYS_APPEND:
            state->eax = (uint32_t)sys_append((const char*)state->ebx, (const char*)state->ecx, state->edx);
            break;
        default:
            break;
    }
}

int ata_init(void) {
    return -1;
}

int ata_present(void) {
    return 0;
}

int ata_read_sectors(uint32_t lba, uint32_t count, void* buf) {
    (void)lba;
    (void)count;
    (void)buf;
    return -1;
}

int ata_write_sectors(uint32_t lba, uint32_t count, const void* buf) {
    (void)lba;
    (void)count;
    (void)buf;
    return -1;
}
