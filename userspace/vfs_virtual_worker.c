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

static uint32_t append_text(uint8_t* data, uint32_t offset, const char* text) {
    uint32_t index = 0U;
    while (text[index] != '\0' && offset < OS_VFS_READ_MAX) data[offset++] = (uint8_t)text[index++];
    return offset;
}

static uint32_t append_uint(uint8_t* data, uint32_t offset, uint32_t value) {
    char digits[10];
    uint32_t count = 0U;
    if (value == 0U) {
        if (offset < OS_VFS_READ_MAX) data[offset++] = (uint8_t)'0';
        return offset;
    }
    while (value > 0U && count < (uint32_t)sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (count > 0U && offset < OS_VFS_READ_MAX) data[offset++] = (uint8_t)digits[--count];
    return offset;
}

static uint32_t format_stats(uint8_t* data, uint32_t reads, uint32_t writes,
                             uint32_t removes, uint32_t renames) {
    uint32_t size = 0U;
    size = append_text(data, size, "reads=");
    size = append_uint(data, size, reads);
    size = append_text(data, size, "\nwrites=");
    size = append_uint(data, size, writes);
    size = append_text(data, size, "\nremoves=");
    size = append_uint(data, size, removes);
    size = append_text(data, size, "\nrenames=");
    size = append_uint(data, size, renames);
    if (size < OS_VFS_READ_MAX) data[size++] = (uint8_t)'\n';
    return size;
}

void main(void) {
    static const uint8_t info[] = "vfsserver ring3 policy\n";
    os_ipc_message_t message;
    os_ipc_payload_t reply;
    char path[OS_VFS_PATH_MAX];
    uint8_t data[OS_VFS_READ_MAX];
    if (service_register("vfs-virtual") != 0) {
        puts("vfsvirtual register failed\n");
        for (;;) yield();
    }
    puts("vfsvirtual ready\n");
    for (;;) {
        int received = ipc_receive(&message);
        if (received == 0 && message.type == OS_IPC_VFS_WORKER_READ &&
            os_vfs_parse_worker_read_request(&message, path) == OS_VFS_STATUS_OK) {
            uint32_t size = 0U;
            int32_t status = OS_VFS_STATUS_NOT_MOUNTED;
            if (string_equal(path, "vfs-info")) {
                puts("vfsvirtual read vfs-info\n");
                size = (uint32_t)(sizeof(info) - 1U);
                status = OS_VFS_STATUS_OK;
            }
            if (os_vfs_make_worker_read_reply(&reply, status,
                                              status == OS_VFS_STATUS_OK ? info : (const uint8_t*)0,
                                              size, message.request_id) == OS_VFS_STATUS_OK) {
                (void)ipc_send(message.sender_pid, &reply);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_WORKER_STATS) {
            uint32_t reads, writes, removes, renames;
            if (os_vfs_parse_worker_stats_request(&message, &reads, &writes, &removes, &renames)
                == OS_VFS_STATUS_OK) {
                uint32_t size = format_stats(data, reads, writes, removes, renames);
                puts("vfsvirtual format stats\n");
                if (os_vfs_make_worker_read_reply(&reply, OS_VFS_STATUS_OK, data, size,
                                                  message.request_id) == OS_VFS_STATUS_OK) {
                    (void)ipc_send(message.sender_pid, &reply);
                }
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_WORKER_MOUNT) {
            uint32_t writable;
            if (os_vfs_parse_worker_mount_request(&message, path, &writable) == OS_VFS_STATUS_OK) {
                uint32_t size = append_text(data, 0U, path);
                size = append_text(data, size, writable ? " rw\n" : " ro\n");
                puts("vfsvirtual format mount\n");
                if (os_vfs_make_worker_read_reply(&reply, OS_VFS_STATUS_OK, data, size,
                                                  message.request_id) == OS_VFS_STATUS_OK) {
                    (void)ipc_send(message.sender_pid, &reply);
                }
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_WORKER_WRITE) {
            uint8_t write_buf[OS_VFS_WRITE_MAX];
            uint32_t write_len = 0U;
            if (os_vfs_parse_worker_write_request(&message, path, write_buf, &write_len) == OS_VFS_STATUS_OK) {
                puts("vfsvirtual write ");
                puts(path);
                puts("\n");
                if (os_vfs_make_write_reply(&reply, OS_VFS_STATUS_OK, message.request_id) == OS_VFS_STATUS_OK) {
                    (void)ipc_send(message.sender_pid, &reply);
                }
            }
        }
        yield();
    }
}
