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

static int service_grant(const char* name, int target_pid) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SERVICE_GRANT), "b"(name), "c"(target_pid));
    return result;
}

static void print_int(int value) {
    char digits[12];
    int n = 0;
    unsigned int number;
    if (value < 0) { putc('-'); number = (unsigned int)(-value); }
    else number = (unsigned int)value;
    if (number == 0U) { putc('0'); return; }
    while (number > 0U && n < 11) { digits[n++] = (char)('0' + (number % 10U)); number /= 10U; }
    while (n > 0) putc(digits[--n]);
}

static int backend_read(const char* path, char* buffer, uint32_t max) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_BACKEND_READ), "b"(path), "c"(buffer), "d"(max));
    return result;
}

static void yield(void) {
    asm volatile("int $0x80" : : "a"(SYS_YIELD));
}

static int string_equal(const char* left, const char* right) {
    uint32_t i = 0U;
    while (left[i] != '\0' && right[i] != '\0') {
        if (left[i] != right[i]) return 0;
        i++;
    }
    return left[i] == right[i];
}

/* Première source VFS isolée : métadonnée synthétique fournie par le service. */
static int read_virtual(const char* path, uint8_t* data, uint32_t* size) {
    static const char info[] = "vfsserver ring3 policy\n";
    uint32_t i;
    if (!string_equal(path, "vfs-info")) return 0;
    for (i = 0U; info[i] != '\0'; i++) data[i] = (uint8_t)info[i];
    *size = i;
    return 1;
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
            int status;
            puts("vfsserver read request\n");
            status = os_vfs_parse_read_request(&message, path);
            uint32_t size = 0U;
            if (status == 0) {
                if (read_virtual(path, data, &size)) {
                    puts("vfsserver virtual vfs-info\n");
                } else {
                    int read = backend_read(path, (char*)data, OS_VFS_READ_MAX);
                    if (read < 0) status = read;
                    else size = (uint32_t)read;
                }
            }
            if (os_vfs_make_read_reply(&reply_payload, status, data, size,
                                       message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_GRANT) {
            int target_pid;
            puts("vfsserver grant request\n");
            int status = os_vfs_parse_grant_request(&message, &target_pid);
            if (status == 0) {
                puts("vfsserver grant vfs ");
                print_int(target_pid);
                puts("\n");
                status = service_grant("vfs", target_pid);
            }
            if (status != 0) {
                puts("vfsserver grant rc ");
                print_int(status);
                puts("\n");
            }
        } else if (received == 0) {
            puts("vfsserver unsupported message\n");
        }
        yield();
    }
}
