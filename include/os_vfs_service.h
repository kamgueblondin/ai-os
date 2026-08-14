#ifndef OS_VFS_SERVICE_H
#define OS_VFS_SERVICE_H

#include <stdint.h>
#include "os_syscalls.h"

#define OS_IPC_VFS_READ       0x56465301U
#define OS_IPC_VFS_READ_REPLY 0x56465302U
#define OS_IPC_VFS_GRANT      0x56465303U
#define OS_IPC_VFS_WRITE      0x56465304U
#define OS_IPC_VFS_WRITE_REPLY 0x56465305U
#define OS_IPC_VFS_REMOVE      0x56465306U
#define OS_IPC_VFS_REMOVE_REPLY 0x56465307U
#define OS_IPC_VFS_RENAME       0x56465308U
#define OS_IPC_VFS_RENAME_REPLY 0x56465309U
#define OS_IPC_VFS_MOUNT_ADD       0x5646530aU
#define OS_IPC_VFS_MOUNT_ADD_REPLY 0x5646530bU
#define OS_IPC_VFS_MOUNT_REMOVE       0x5646530cU
#define OS_IPC_VFS_MOUNT_REMOVE_REPLY 0x5646530dU

#define OS_VFS_PATH_MAX 48U
#define OS_VFS_GRANT_REQUEST_SIZE 4U
#define OS_VFS_READ_MAX 80U
#define OS_VFS_WRITE_MAX 44U
#define OS_VFS_WRITE_REQUEST_SIZE (OS_VFS_PATH_MAX + 4U + OS_VFS_WRITE_MAX)
#define OS_VFS_WRITE_REPLY_SIZE 4U
#define OS_VFS_RENAME_REQUEST_SIZE (OS_VFS_PATH_MAX * 2U)
#define OS_VFS_MOUNT_ADD_REQUEST_SIZE (OS_VFS_PATH_MAX + 4U)
#define OS_VFS_MOUNT_REMOVE_REQUEST_SIZE OS_VFS_PATH_MAX
#define OS_VFS_MOUNT_REPLY_SIZE 4U
#define OS_VFS_MOUNT_SOURCE_INITRD 1U
#define OS_VFS_MOUNT_SOURCE_OVERLAY 2U

#define OS_VFS_STATUS_OK          0
#define OS_VFS_STATUS_INVALID    (-60)
#define OS_VFS_STATUS_NOT_MOUNTED (-61)
#define OS_VFS_STATUS_MOUNT_FULL  (-62)
#define OS_VFS_STATUS_MOUNT_EXISTS (-63)
#define OS_VFS_STATUS_TRUNCATED   1

typedef struct {
    char path[OS_VFS_PATH_MAX];
} os_vfs_read_request_t;

typedef struct {
    int32_t status;
    uint32_t size;
    uint8_t data[OS_VFS_READ_MAX];
} os_vfs_read_reply_t;

typedef struct {
    int32_t status;
} os_vfs_write_reply_t;

typedef struct {
    int32_t status;
} os_vfs_remove_reply_t;

typedef struct {
    int32_t status;
} os_vfs_rename_reply_t;

typedef struct {
    int32_t status;
} os_vfs_mount_reply_t;

static inline int os_vfs_path_is_safe(const char* path) {
    uint32_t i;
    if (!path || path[0] == '\0') return 0;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) {
        if (path[i] == '\0') return 1;
        if (path[i] == '.' && i + 1U < OS_VFS_PATH_MAX && path[i + 1U] == '.') return 0;
    }
    return 0;
}

/* Un montage est un préfixe de répertoire terminé par '/'. Le chemin relatif
 * retourné ne peut pas être vide : lire la racine d'un montage n'est pas une
 * opération de lecture de fichier. */
static inline int os_vfs_mount_prefix_is_valid(const char* mount) {
    uint32_t i = 0U;
    if (!os_vfs_path_is_safe(mount)) return 0;
    while (mount[i] != '\0') i++;
    return i > 0U && mount[i - 1U] == '/';
}

static inline int os_vfs_mount_source_is_valid(uint32_t source) {
    return source == OS_VFS_MOUNT_SOURCE_INITRD || source == OS_VFS_MOUNT_SOURCE_OVERLAY;
}

static inline int os_vfs_match_mount(const char* path, const char* mount,
                                     const char** relative_out) {
    uint32_t i = 0U;
    if (!os_vfs_path_is_safe(path) || !os_vfs_path_is_safe(mount)) return 0;
    while (mount[i] != '\0') {
        if (path[i] == '\0' || path[i] != mount[i]) return 0;
        i++;
    }
    if (i == 0U || mount[i - 1U] != '/' || path[i] == '\0') return 0;
    if (relative_out) *relative_out = path + i;
    return 1;
}

static inline int os_vfs_make_read_request(os_ipc_payload_t* payload, const char* path,
                                           uint32_t request_id) {
    uint32_t i;
    if (!payload || !os_vfs_path_is_safe(path)) return OS_VFS_STATUS_INVALID;
    payload->type = OS_IPC_VFS_READ;
    payload->size = OS_VFS_PATH_MAX;
    payload->request_id = request_id;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) {
        if (path[i] == '\0') break;
        payload->data[i] = (uint8_t)path[i];
    }
    for (; i < OS_IPC_MAX_DATA; i++) payload->data[i] = 0U;
    return 0;
}

static inline int os_vfs_parse_read_request(const os_ipc_message_t* message, char* path_out) {
    uint32_t i;
    if (!message || !path_out || message->type != OS_IPC_VFS_READ ||
        message->size != OS_VFS_PATH_MAX) return OS_VFS_STATUS_INVALID;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) path_out[i] = (char)message->data[i];
    if (!os_vfs_path_is_safe(path_out)) return OS_VFS_STATUS_INVALID;
    return 0;
}

static inline void os_vfs_encode_i32(uint8_t* out, int32_t value) {
    uint32_t raw = (uint32_t)value;
    out[0] = (uint8_t)(raw & 0xffU);
    out[1] = (uint8_t)((raw >> 8) & 0xffU);
    out[2] = (uint8_t)((raw >> 16) & 0xffU);
    out[3] = (uint8_t)((raw >> 24) & 0xffU);
}

static inline int32_t os_vfs_decode_i32(const uint8_t* in) {
    uint32_t raw = (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
                   ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
    return (int32_t)raw;
}

static inline void os_vfs_encode_u32(uint8_t* out, uint32_t value) {
    out[0] = (uint8_t)(value & 0xffU);
    out[1] = (uint8_t)((value >> 8) & 0xffU);
    out[2] = (uint8_t)((value >> 16) & 0xffU);
    out[3] = (uint8_t)((value >> 24) & 0xffU);
}

static inline uint32_t os_vfs_decode_u32(const uint8_t* in) {
    return (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
           ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
}

static inline int os_vfs_make_write_request(os_ipc_payload_t* payload, const char* path,
                                            const uint8_t* data, uint32_t size,
                                            uint32_t request_id) {
    uint32_t i;
    if (!payload || !os_vfs_path_is_safe(path) || size > OS_VFS_WRITE_MAX ||
        (size > 0U && !data)) return OS_VFS_STATUS_INVALID;
    payload->type = OS_IPC_VFS_WRITE;
    payload->size = OS_VFS_WRITE_REQUEST_SIZE;
    payload->request_id = request_id;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) {
        payload->data[i] = (uint8_t)path[i];
        if (path[i] == '\0') {
            for (i++; i < OS_VFS_PATH_MAX; i++) payload->data[i] = 0U;
            break;
        }
    }
    os_vfs_encode_u32(&payload->data[OS_VFS_PATH_MAX], size);
    for (i = 0U; i < size; i++) payload->data[OS_VFS_PATH_MAX + 4U + i] = data[i];
    for (; i < OS_VFS_WRITE_MAX; i++) payload->data[OS_VFS_PATH_MAX + 4U + i] = 0U;
    return 0;
}

static inline int os_vfs_parse_write_request(const os_ipc_message_t* message,
                                             char* path_out, uint8_t* data_out,
                                             uint32_t* size_out) {
    uint32_t i;
    uint32_t size;
    if (!message || !path_out || !data_out || !size_out || message->type != OS_IPC_VFS_WRITE ||
        message->size != OS_VFS_WRITE_REQUEST_SIZE) return OS_VFS_STATUS_INVALID;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) path_out[i] = (char)message->data[i];
    if (!os_vfs_path_is_safe(path_out)) return OS_VFS_STATUS_INVALID;
    size = os_vfs_decode_u32(&message->data[OS_VFS_PATH_MAX]);
    if (size > OS_VFS_WRITE_MAX) return OS_VFS_STATUS_INVALID;
    for (i = 0U; i < size; i++) data_out[i] = message->data[OS_VFS_PATH_MAX + 4U + i];
    *size_out = size;
    return 0;
}

static inline int os_vfs_make_write_reply(os_ipc_payload_t* payload, int32_t status,
                                          uint32_t request_id) {
    uint32_t i;
    if (!payload) return OS_VFS_STATUS_INVALID;
    payload->type = OS_IPC_VFS_WRITE_REPLY;
    payload->size = OS_VFS_WRITE_REPLY_SIZE;
    payload->request_id = request_id;
    os_vfs_encode_i32(&payload->data[0], status);
    for (i = OS_VFS_WRITE_REPLY_SIZE; i < OS_IPC_MAX_DATA; i++) payload->data[i] = 0U;
    return 0;
}

static inline int os_vfs_parse_write_reply(const os_ipc_message_t* message,
                                           os_vfs_write_reply_t* reply_out,
                                           uint32_t expected_request_id) {
    if (!message || !reply_out || message->type != OS_IPC_VFS_WRITE_REPLY ||
        message->size != OS_VFS_WRITE_REPLY_SIZE || message->request_id != expected_request_id) {
        return OS_VFS_STATUS_INVALID;
    }
    reply_out->status = os_vfs_decode_i32(&message->data[0]);
    return 0;
}

static inline int os_vfs_make_remove_request(os_ipc_payload_t* payload, const char* path,
                                             uint32_t request_id) {
    uint32_t i;
    if (!payload || !os_vfs_path_is_safe(path)) return OS_VFS_STATUS_INVALID;
    payload->type = OS_IPC_VFS_REMOVE;
    payload->size = OS_VFS_PATH_MAX;
    payload->request_id = request_id;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) {
        payload->data[i] = (uint8_t)path[i];
        if (path[i] == '\0') {
            for (i++; i < OS_IPC_MAX_DATA; i++) payload->data[i] = 0U;
            break;
        }
    }
    return 0;
}

static inline int os_vfs_parse_remove_request(const os_ipc_message_t* message,
                                              char* path_out) {
    uint32_t i;
    if (!message || !path_out || message->type != OS_IPC_VFS_REMOVE ||
        message->size != OS_VFS_PATH_MAX) return OS_VFS_STATUS_INVALID;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) path_out[i] = (char)message->data[i];
    return os_vfs_path_is_safe(path_out) ? 0 : OS_VFS_STATUS_INVALID;
}

static inline int os_vfs_make_remove_reply(os_ipc_payload_t* payload, int32_t status,
                                           uint32_t request_id) {
    uint32_t i;
    if (!payload) return OS_VFS_STATUS_INVALID;
    payload->type = OS_IPC_VFS_REMOVE_REPLY;
    payload->size = OS_VFS_WRITE_REPLY_SIZE;
    payload->request_id = request_id;
    os_vfs_encode_i32(&payload->data[0], status);
    for (i = OS_VFS_WRITE_REPLY_SIZE; i < OS_IPC_MAX_DATA; i++) payload->data[i] = 0U;
    return 0;
}

static inline int os_vfs_parse_remove_reply(const os_ipc_message_t* message,
                                            os_vfs_remove_reply_t* reply_out,
                                            uint32_t expected_request_id) {
    if (!message || !reply_out || message->type != OS_IPC_VFS_REMOVE_REPLY ||
        message->size != OS_VFS_WRITE_REPLY_SIZE || message->request_id != expected_request_id) {
        return OS_VFS_STATUS_INVALID;
    }
    reply_out->status = os_vfs_decode_i32(&message->data[0]);
    return 0;
}

static inline int os_vfs_make_rename_request(os_ipc_payload_t* payload,
                                             const char* old_path, const char* new_path,
                                             uint32_t request_id) {
    uint32_t i;
    if (!payload || !os_vfs_path_is_safe(old_path) || !os_vfs_path_is_safe(new_path)) {
        return OS_VFS_STATUS_INVALID;
    }
    payload->type = OS_IPC_VFS_RENAME;
    payload->size = OS_VFS_RENAME_REQUEST_SIZE;
    payload->request_id = request_id;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) {
        payload->data[i] = (uint8_t)old_path[i];
        if (old_path[i] == '\0') {
            for (i++; i < OS_VFS_PATH_MAX; i++) payload->data[i] = 0U;
            break;
        }
    }
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) {
        payload->data[OS_VFS_PATH_MAX + i] = (uint8_t)new_path[i];
        if (new_path[i] == '\0') {
            for (i++; i < OS_VFS_PATH_MAX; i++) payload->data[OS_VFS_PATH_MAX + i] = 0U;
            break;
        }
    }
    return 0;
}

static inline int os_vfs_parse_rename_request(const os_ipc_message_t* message,
                                              char* old_path_out, char* new_path_out) {
    uint32_t i;
    if (!message || !old_path_out || !new_path_out || message->type != OS_IPC_VFS_RENAME ||
        message->size != OS_VFS_RENAME_REQUEST_SIZE) return OS_VFS_STATUS_INVALID;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) {
        old_path_out[i] = (char)message->data[i];
        new_path_out[i] = (char)message->data[OS_VFS_PATH_MAX + i];
    }
    return os_vfs_path_is_safe(old_path_out) && os_vfs_path_is_safe(new_path_out)
        ? 0 : OS_VFS_STATUS_INVALID;
}

static inline int os_vfs_make_rename_reply(os_ipc_payload_t* payload, int32_t status,
                                           uint32_t request_id) {
    uint32_t i;
    if (!payload) return OS_VFS_STATUS_INVALID;
    payload->type = OS_IPC_VFS_RENAME_REPLY;
    payload->size = OS_VFS_WRITE_REPLY_SIZE;
    payload->request_id = request_id;
    os_vfs_encode_i32(&payload->data[0], status);
    for (i = OS_VFS_WRITE_REPLY_SIZE; i < OS_IPC_MAX_DATA; i++) payload->data[i] = 0U;
    return 0;
}

static inline int os_vfs_parse_rename_reply(const os_ipc_message_t* message,
                                            os_vfs_rename_reply_t* reply_out,
                                            uint32_t expected_request_id) {
    if (!message || !reply_out || message->type != OS_IPC_VFS_RENAME_REPLY ||
        message->size != OS_VFS_WRITE_REPLY_SIZE || message->request_id != expected_request_id) {
        return OS_VFS_STATUS_INVALID;
    }
    reply_out->status = os_vfs_decode_i32(&message->data[0]);
    return 0;
}

static inline int os_vfs_make_mount_add_request(os_ipc_payload_t* payload,
                                                const char* mount, uint32_t source,
                                                uint32_t request_id) {
    uint32_t i;
    if (!payload || !os_vfs_mount_prefix_is_valid(mount) || !os_vfs_mount_source_is_valid(source)) {
        return OS_VFS_STATUS_INVALID;
    }
    payload->type = OS_IPC_VFS_MOUNT_ADD;
    payload->size = OS_VFS_MOUNT_ADD_REQUEST_SIZE;
    payload->request_id = request_id;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) payload->data[i] = (uint8_t)mount[i];
    os_vfs_encode_u32(&payload->data[OS_VFS_PATH_MAX], source);
    for (i = OS_VFS_MOUNT_ADD_REQUEST_SIZE; i < OS_IPC_MAX_DATA; i++) payload->data[i] = 0U;
    return 0;
}

static inline int os_vfs_parse_mount_add_request(const os_ipc_message_t* message,
                                                 char* mount_out, uint32_t* source_out) {
    uint32_t i;
    if (!message || !mount_out || !source_out || message->type != OS_IPC_VFS_MOUNT_ADD ||
        message->size != OS_VFS_MOUNT_ADD_REQUEST_SIZE) return OS_VFS_STATUS_INVALID;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) mount_out[i] = (char)message->data[i];
    *source_out = os_vfs_decode_u32(&message->data[OS_VFS_PATH_MAX]);
    return os_vfs_mount_prefix_is_valid(mount_out) && os_vfs_mount_source_is_valid(*source_out)
        ? 0 : OS_VFS_STATUS_INVALID;
}

static inline int os_vfs_make_mount_remove_request(os_ipc_payload_t* payload,
                                                    const char* mount, uint32_t request_id) {
    uint32_t i;
    if (!payload || !os_vfs_mount_prefix_is_valid(mount)) return OS_VFS_STATUS_INVALID;
    payload->type = OS_IPC_VFS_MOUNT_REMOVE;
    payload->size = OS_VFS_MOUNT_REMOVE_REQUEST_SIZE;
    payload->request_id = request_id;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) payload->data[i] = (uint8_t)mount[i];
    for (i = OS_VFS_PATH_MAX; i < OS_IPC_MAX_DATA; i++) payload->data[i] = 0U;
    return 0;
}

static inline int os_vfs_parse_mount_remove_request(const os_ipc_message_t* message,
                                                    char* mount_out) {
    uint32_t i;
    if (!message || !mount_out || message->type != OS_IPC_VFS_MOUNT_REMOVE ||
        message->size != OS_VFS_MOUNT_REMOVE_REQUEST_SIZE) return OS_VFS_STATUS_INVALID;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) mount_out[i] = (char)message->data[i];
    return os_vfs_mount_prefix_is_valid(mount_out) ? 0 : OS_VFS_STATUS_INVALID;
}

static inline int os_vfs_make_mount_reply(os_ipc_payload_t* payload, uint32_t type,
                                          int32_t status, uint32_t request_id) {
    uint32_t i;
    if (!payload || (type != OS_IPC_VFS_MOUNT_ADD_REPLY && type != OS_IPC_VFS_MOUNT_REMOVE_REPLY)) {
        return OS_VFS_STATUS_INVALID;
    }
    payload->type = type;
    payload->size = OS_VFS_MOUNT_REPLY_SIZE;
    payload->request_id = request_id;
    os_vfs_encode_i32(&payload->data[0], status);
    for (i = OS_VFS_MOUNT_REPLY_SIZE; i < OS_IPC_MAX_DATA; i++) payload->data[i] = 0U;
    return 0;
}

static inline int os_vfs_parse_mount_reply(const os_ipc_message_t* message, uint32_t type,
                                           os_vfs_mount_reply_t* reply_out,
                                           uint32_t expected_request_id) {
    if (!message || !reply_out ||
        (type != OS_IPC_VFS_MOUNT_ADD_REPLY && type != OS_IPC_VFS_MOUNT_REMOVE_REPLY) ||
        message->type != type || message->size != OS_VFS_MOUNT_REPLY_SIZE ||
        message->request_id != expected_request_id) return OS_VFS_STATUS_INVALID;
    reply_out->status = os_vfs_decode_i32(&message->data[0]);
    return 0;
}

static inline int os_vfs_make_grant_request(os_ipc_payload_t* payload, int32_t target_pid) {
    uint32_t i;
    if (!payload || target_pid <= 0) return OS_VFS_STATUS_INVALID;
    payload->type = OS_IPC_VFS_GRANT;
    payload->size = OS_VFS_GRANT_REQUEST_SIZE;
    payload->request_id = 0U;
    os_vfs_encode_i32(&payload->data[0], target_pid);
    for (i = OS_VFS_GRANT_REQUEST_SIZE; i < OS_IPC_MAX_DATA; i++) payload->data[i] = 0U;
    return 0;
}

static inline int os_vfs_parse_grant_request(const os_ipc_message_t* message, int32_t* target_pid) {
    if (!message || !target_pid || message->type != OS_IPC_VFS_GRANT ||
        message->size != OS_VFS_GRANT_REQUEST_SIZE) return OS_VFS_STATUS_INVALID;
    *target_pid = os_vfs_decode_i32(&message->data[0]);
    return *target_pid > 0 ? 0 : OS_VFS_STATUS_INVALID;
}

static inline int os_vfs_make_read_reply(os_ipc_payload_t* payload, int32_t status,
                                  const uint8_t* data, uint32_t size, uint32_t request_id) {
    uint32_t i;
    if (!payload || size > OS_VFS_READ_MAX || (size > 0U && !data)) return OS_VFS_STATUS_INVALID;
    payload->type = OS_IPC_VFS_READ_REPLY;
    payload->size = 8U + OS_VFS_READ_MAX;
    payload->request_id = request_id;
    os_vfs_encode_i32(&payload->data[0], status);
    os_vfs_encode_u32(&payload->data[4], size);
    for (i = 0U; i < size; i++) payload->data[8U + i] = data[i];
    for (; i < OS_VFS_READ_MAX; i++) payload->data[8U + i] = 0U;
    for (i = 8U + OS_VFS_READ_MAX; i < OS_IPC_MAX_DATA; i++) payload->data[i] = 0U;
    return 0;
}

static inline int os_vfs_parse_read_reply(const os_ipc_message_t* message,
                                   os_vfs_read_reply_t* reply_out,
                                   uint32_t expected_request_id) {
    uint32_t i;
    if (!message || !reply_out || message->type != OS_IPC_VFS_READ_REPLY ||
        message->size != 8U + OS_VFS_READ_MAX ||
        message->request_id != expected_request_id) return OS_VFS_STATUS_INVALID;
    reply_out->status = os_vfs_decode_i32(&message->data[0]);
    reply_out->size = os_vfs_decode_u32(&message->data[4]);
    if (reply_out->size > OS_VFS_READ_MAX) return OS_VFS_STATUS_INVALID;
    for (i = 0U; i < OS_VFS_READ_MAX; i++) reply_out->data[i] = message->data[8U + i];
    return 0;
}

#endif
