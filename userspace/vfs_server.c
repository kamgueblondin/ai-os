#include "os_syscalls.h"
#include "os_vfs_service.h"

static void putc(char c) {
    asm volatile("int $0x80" : : "a"(SYS_PUTC), "b"(c));
}

static void puts(const char* text) {
    int i = 0;
    while (text[i] != '\0') putc(text[i++]);
}

static int ipc_receive(os_ipc_message_t* message) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_IPC_RECV), "b"(message));
    return result;
}

static int ipc_send(int target_pid, const os_ipc_payload_t* payload) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_IPC_SEND), "b"(target_pid), "c"(payload));
    return result;
}

static int service_register(const char* name) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SERVICE_REGISTER), "b"(name));
    return result;
}

static int read_file(const char* path, char* buffer, uint32_t max) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_READFILE), "b"(path), "c"(buffer), "d"(max));
    return result;
}

static void yield(void) {
    asm volatile("int $0x80" : : "a"(SYS_YIELD));
}

void main(void) {
    os_ipc_message_t message;
    os_ipc_payload_t reply_payload;
    char path[OS_VFS_PATH_MAX];
    uint8_t data[OS_VFS_READ_MAX];
    if (service_register("vfs") != 0) {
        puts("vfsserver register failed\n");
        for (;;) yield();
    }
    puts("vfsserver ready vfs\n");
    for (;;) {
        int received = ipc_receive(&message);
        if (received == 0 && message.type == OS_IPC_VFS_READ) {
            int status = os_vfs_parse_read_request(&message, path);
            uint32_t size = 0U;
            if (status == 0) {
                int read = read_file(path, (char*)data, OS_VFS_READ_MAX);
                if (read < 0) status = read;
                else size = (uint32_t)read;
            }
            if (os_vfs_make_read_reply(&reply_payload, status, data, size,
                                       message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        }
        yield();
    }
}
