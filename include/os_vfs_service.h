#ifndef OS_VFS_SERVICE_H
#define OS_VFS_SERVICE_H

#include <stdint.h>
#include "os_syscalls.h"

#define OS_IPC_VFS_READ       0x56465301U
#define OS_IPC_VFS_READ_REPLY 0x56465302U

#define OS_VFS_PATH_MAX 48U
#define OS_VFS_READ_MAX 80U

#define OS_VFS_STATUS_OK        0
#define OS_VFS_STATUS_INVALID  (-60)
#define OS_VFS_STATUS_TRUNCATED 1

typedef struct {
    char path[OS_VFS_PATH_MAX];
} os_vfs_read_request_t;

typedef struct {
    int32_t status;
    uint32_t size;
    uint8_t data[OS_VFS_READ_MAX];
} os_vfs_read_reply_t;

static inline int os_vfs_path_is_safe(const char* path) {
    uint32_t i;
    if (!path || path[0] == '\0') return 0;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) {
        if (path[i] == '\0') return 1;
        if (path[i] == '.' && i + 1U < OS_VFS_PATH_MAX && path[i + 1U] == '.') return 0;
    }
    return 0;
}

static inline int os_vfs_make_read_request(os_ipc_payload_t* payload, const char* path) {
    uint32_t i;
    if (!payload || !os_vfs_path_is_safe(path)) return OS_VFS_STATUS_INVALID;
    payload->type = OS_IPC_VFS_READ;
    payload->size = OS_VFS_PATH_MAX;
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

static inline int os_vfs_make_read_reply(os_ipc_payload_t* payload, int32_t status,
                                  const uint8_t* data, uint32_t size) {
    uint32_t i;
    if (!payload || size > OS_VFS_READ_MAX || (size > 0U && !data)) return OS_VFS_STATUS_INVALID;
    payload->type = OS_IPC_VFS_READ_REPLY;
    payload->size = 8U + OS_VFS_READ_MAX;
    os_vfs_encode_i32(&payload->data[0], status);
    os_vfs_encode_u32(&payload->data[4], size);
    for (i = 0U; i < size; i++) payload->data[8U + i] = data[i];
    for (; i < OS_VFS_READ_MAX; i++) payload->data[8U + i] = 0U;
    for (i = 8U + OS_VFS_READ_MAX; i < OS_IPC_MAX_DATA; i++) payload->data[i] = 0U;
    return 0;
}

static inline int os_vfs_parse_read_reply(const os_ipc_message_t* message,
                                   os_vfs_read_reply_t* reply_out) {
    uint32_t i;
    if (!message || !reply_out || message->type != OS_IPC_VFS_READ_REPLY ||
        message->size != 8U + OS_VFS_READ_MAX) return OS_VFS_STATUS_INVALID;
    reply_out->status = os_vfs_decode_i32(&message->data[0]);
    reply_out->size = os_vfs_decode_u32(&message->data[4]);
    if (reply_out->size > OS_VFS_READ_MAX) return OS_VFS_STATUS_INVALID;
    for (i = 0U; i < OS_VFS_READ_MAX; i++) reply_out->data[i] = message->data[8U + i];
    return 0;
}

#endif
