#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "../task/task.h"
#include "os_syscalls.h"

typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi;
} syscall_params_t;

void syscall_init();
void syscall_handler(cpu_state_t* cpu);

void sys_exit(uint32_t exit_code);
void sys_putc(char c);
char sys_getc();
void sys_puts(const char* str);
void sys_yield();

void sys_gets(char* buffer, uint32_t size);
int sys_exec(const char* path, char* argv[]);
int sys_spawn(const char* path, char* argv[]);

int sys_listdir(const char* path, os_dirent_t* out, int max_n);
int sys_readfile(const char* path, char* buf, uint32_t max);
int sys_getpid(void);
int sys_ps(os_proc_t* out, int max_n);
int sys_kill(int pid);
uint32_t sys_ticks(void);
int sys_meminfo(os_meminfo_t* info);
int sys_mkdir(const char* path);
int sys_unlink(const char* path);
int sys_writefile(const char* path, const char* buf, uint32_t n);
int sys_stat(const char* path, os_dirent_t* out);
int sys_rename(const char* oldpath, const char* newpath);
int sys_copy(const char* src, const char* dst);
int sys_append(const char* path, const char* buf, uint32_t n);
int sys_gpt2_generate(const char* prompt, char* out, uint32_t max);
int sys_ipc_send(int target_pid, const os_ipc_payload_t* payload);
int sys_ipc_receive(os_ipc_message_t* out);
int sys_service_register(const char* name);
int sys_service_lookup(const char* name);
int sys_service_unregister(const char* name);
int sys_service_grant(const char* name, int target_pid);
int sys_service_notify(const char* name);
int sys_vfs_backend_read(const char* path, char* buffer, uint32_t max);
int sys_vfs_backend_write(const char* path, const char* data, uint32_t size);
int sys_vfs_initrd_read(const char* path, char* buffer, uint32_t max);
int sys_vfs_overlay_read(const char* path, char* buffer, uint32_t max);
int sys_vfs_overlay_unlink(const char* path);

void keyboard_add_char_to_buffer(char c);

#endif
