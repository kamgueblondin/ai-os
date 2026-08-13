/* os_syscalls.h - ABI Ring 3 / noyau (numéros et structures POD uniquement) */

#ifndef OS_SYSCALLS_H
#define OS_SYSCALLS_H

#include <stdint.h>

#define SYS_EXIT     0
#define SYS_PUTC     1
#define SYS_GETC     2
#define SYS_PUTS     3
#define SYS_YIELD    4
#define SYS_GETS     5
#define SYS_EXEC     6
#define SYS_SPAWN    7
#define SYS_LISTDIR  8
#define SYS_READFILE 9
#define SYS_GETPID   10
#define SYS_PS       11
#define SYS_KILL     12
#define SYS_TICKS    13
#define SYS_MEMINFO  14
#define SYS_MKDIR    15
#define SYS_UNLINK   16
#define SYS_WRITEFILE 17
#define SYS_STAT     18
#define SYS_RENAME   19
#define SYS_COPY     20

#define MAX_SYSCALLS 21

#define OS_NAME_MAX 64
#define OS_PROC_NAME_MAX 32

#define OS_DIRENT_FILE 0
#define OS_DIRENT_DIR  1

#define OS_TASK_RUNNING   0
#define OS_TASK_READY     1
#define OS_TASK_WAITING   2
#define OS_TASK_TERMINATED 4

#define OS_TASK_KERNEL 0
#define OS_TASK_USER   1

typedef struct {
    char name[OS_NAME_MAX];
    uint32_t size;
    uint32_t flags; /* OS_DIRENT_FILE / OS_DIRENT_DIR */
} os_dirent_t;

typedef struct {
    int32_t pid;
    int32_t state;
    int32_t type;
    char name[OS_PROC_NAME_MAX];
} os_proc_t;

typedef struct {
    uint32_t total_pages;
    uint32_t used_pages;
    uint32_t free_pages;
} os_meminfo_t;

#endif
