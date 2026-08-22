#include "os_vfs_service.h"

static void putc(char value) {
    asm volatile("int $0x80" : : "a"(SYS_PUTC), "b"(value));
}

static void puts(const char* text) {
    uint32_t index = 0U;
    while (text[index] != '\0') putc(text[index++]);
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

static void yield(void) {
    asm volatile("int $0x80" : : "a"(SYS_YIELD));
}

static int string_equal(const char* left, const char* right) {
    uint32_t index = 0U;
    while (left[index] != '\0' && right[index] != '\0') {
        if (left[index] != right[index]) return 0;
        index++;
    }
    return left[index] == right[index];
}

void main(void) {
    static const uint8_t info[] = "vfsserver ring3 policy\n";
    os_ipc_message_t message;
    os_ipc_payload_t reply;
    char path[OS_VFS_PATH_MAX];
    if (service_register("vfs-virtual") != 0) {
        puts("vfsvirtual register failed\n");
        for (;;) yield();
    }
    puts("vfsvirtual ready\n");
    for (;;) {
        int status = ipc_receive(&message);
        if (status == 0 && message.type == OS_IPC_VFS_WORKER_READ &&
            os_vfs_parse_worker_read_request(&message, path) == OS_VFS_STATUS_OK) {
            uint32_t size = 0U;
            int32_t reply_status = OS_VFS_STATUS_NOT_MOUNTED;
            if (string_equal(path, "vfs-info")) {
                puts("vfsvirtual read vfs-info\n");
                size = (uint32_t)(sizeof(info) - 1U);
                reply_status = OS_VFS_STATUS_OK;
            }
            if (os_vfs_make_worker_read_reply(&reply, reply_status,
                                              reply_status == OS_VFS_STATUS_OK ? info : (const uint8_t*)0,
                                              size, message.request_id) == OS_VFS_STATUS_OK) {
                (void)ipc_send(message.sender_pid, &reply);
            }
        }
        yield();
    }
}
