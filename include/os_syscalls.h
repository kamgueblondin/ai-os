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
#define SYS_APPEND   21
/* prompt (EBX), buffer de reponse (ECX), taille du buffer (EDX) */
#define SYS_GPT2_GENERATE 22
/* EBX = PID cible, ECX = os_ipc_payload_t* */
#define SYS_IPC_SEND      23
/* EBX = os_ipc_message_t* */
#define SYS_IPC_RECV      24
/* EBX = nom de service ; le PID est celui de l’appelant Ring 3. */
#define SYS_SERVICE_REGISTER 25
/* EBX = nom de service ; EAX reçoit le PID associé. */
#define SYS_SERVICE_LOOKUP   26
/* EBX = nom de service ; seul son propriétaire peut le retirer. */
#define SYS_SERVICE_UNREGISTER 27
/* EBX = nom de service, ECX = PID bénéficiaire ; transfert par le propriétaire. */
#define SYS_SERVICE_GRANT 28
/* EBX = chemin, ECX = buffer, EDX = taille ; réservé au propriétaire de `vfs`. */
#define SYS_VFS_BACKEND_READ 29
/* EBX = nom ; abonne l’appelant Ring 3 aux changements de propriétaire. */
#define SYS_SERVICE_NOTIFY 30
/* EBX = chemin relatif, ECX = données, EDX = taille ; réservé au propriétaire de `vfs`. */
#define SYS_VFS_BACKEND_WRITE 31

#define MAX_SYSCALLS 32

/* IPC Foundation : messages courts, copies par valeur et retours non bloquants. */
#define OS_IPC_MAX_DATA 96U
#define OS_IPC_EMPTY       (-40)
#define OS_IPC_FULL        (-41)
#define OS_IPC_BAD_TARGET  (-42)
#define OS_IPC_BAD_MESSAGE (-43)

/* Registre Foundation : simple découverte de nom, pas une capability. */
#define OS_SERVICE_NAME_MAX 16U
#define OS_SERVICE_BAD_NAME  (-50)
#define OS_SERVICE_FULL      (-51)
#define OS_SERVICE_TAKEN     (-52)
#define OS_SERVICE_NOT_FOUND (-53)
#define OS_SERVICE_NOT_OWNER (-54)
#define OS_SERVICE_BAD_GRANTEE (-55)
#define OS_SERVICE_WATCH_FULL  (-56)
#define OS_VFS_BACKEND_DENIED (-61)

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

/* Charge fournie par l'émetteur : son identité est ajoutée par le noyau.
 * request_id est opaque et permet au protocole utilisateur de corréler une réponse.
 */
typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t request_id;
    uint8_t data[OS_IPC_MAX_DATA];
} os_ipc_payload_t;

/* Message délivré au destinataire depuis sa boîte aux lettres noyau. */
typedef struct {
    int32_t sender_pid;
    uint32_t type;
    uint32_t size;
    uint32_t request_id;
    uint8_t data[OS_IPC_MAX_DATA];
} os_ipc_message_t;

/* Notification synthétique, émise par le noyau (sender_pid = 0) dans l’IPC
 * existant. La charge est : nom NUL-paddé, ancien PID, nouveau PID, raison. */
#define OS_IPC_SERVICE_EVENT 0x53525601U
#define OS_SERVICE_EVENT_SIZE (OS_SERVICE_NAME_MAX + 12U)
#define OS_SERVICE_EVENT_PUBLISHED    1U
#define OS_SERVICE_EVENT_GRANTED      2U
#define OS_SERVICE_EVENT_UNREGISTERED 3U
#define OS_SERVICE_EVENT_PURGED       4U

typedef struct {
    char name[OS_SERVICE_NAME_MAX];
    int32_t old_owner_pid;
    int32_t new_owner_pid;
    uint32_t reason;
} os_service_event_t;

static inline void os_service_encode_i32(uint8_t* out, int32_t value) {
    uint32_t raw = (uint32_t)value;
    out[0] = (uint8_t)(raw & 0xffU);
    out[1] = (uint8_t)((raw >> 8) & 0xffU);
    out[2] = (uint8_t)((raw >> 16) & 0xffU);
    out[3] = (uint8_t)((raw >> 24) & 0xffU);
}

static inline int32_t os_service_decode_i32(const uint8_t* in) {
    uint32_t raw = (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
                   ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
    return (int32_t)raw;
}

static inline int os_service_make_event(os_ipc_payload_t* payload, const char* name,
                                        int32_t old_owner_pid, int32_t new_owner_pid,
                                        uint32_t reason) {
    uint32_t i;
    int terminated = 0;
    if (!payload || !name || old_owner_pid < 0 || new_owner_pid < 0 ||
        reason < OS_SERVICE_EVENT_PUBLISHED || reason > OS_SERVICE_EVENT_PURGED) return -1;
    for (i = 0U; i < OS_SERVICE_NAME_MAX; i++) {
        payload->data[i] = (uint8_t)name[i];
        if (name[i] == '\0') {
            terminated = 1;
            for (i++; i < OS_SERVICE_NAME_MAX; i++) payload->data[i] = 0U;
            break;
        }
    }
    if (!terminated) return -1;
    payload->type = OS_IPC_SERVICE_EVENT;
    payload->size = OS_SERVICE_EVENT_SIZE;
    payload->request_id = 0U;
    os_service_encode_i32(&payload->data[OS_SERVICE_NAME_MAX], old_owner_pid);
    os_service_encode_i32(&payload->data[OS_SERVICE_NAME_MAX + 4U], new_owner_pid);
    payload->data[OS_SERVICE_NAME_MAX + 8U] = (uint8_t)(reason & 0xffU);
    payload->data[OS_SERVICE_NAME_MAX + 9U] = (uint8_t)((reason >> 8) & 0xffU);
    payload->data[OS_SERVICE_NAME_MAX + 10U] = (uint8_t)((reason >> 16) & 0xffU);
    payload->data[OS_SERVICE_NAME_MAX + 11U] = (uint8_t)((reason >> 24) & 0xffU);
    for (i = OS_SERVICE_EVENT_SIZE; i < OS_IPC_MAX_DATA; i++) payload->data[i] = 0U;
    return 0;
}

static inline int os_service_parse_event(const os_ipc_message_t* message,
                                         os_service_event_t* event_out) {
    uint32_t i;
    uint32_t reason;
    if (!message || !event_out || message->sender_pid != 0 ||
        message->type != OS_IPC_SERVICE_EVENT || message->size != OS_SERVICE_EVENT_SIZE ||
        message->request_id != 0U) return -1;
    for (i = 0U; i < OS_SERVICE_NAME_MAX; i++) event_out->name[i] = (char)message->data[i];
    if (event_out->name[OS_SERVICE_NAME_MAX - 1U] != '\0') return -1;
    event_out->old_owner_pid = os_service_decode_i32(&message->data[OS_SERVICE_NAME_MAX]);
    event_out->new_owner_pid = os_service_decode_i32(&message->data[OS_SERVICE_NAME_MAX + 4U]);
    reason = (uint32_t)message->data[OS_SERVICE_NAME_MAX + 8U] |
             ((uint32_t)message->data[OS_SERVICE_NAME_MAX + 9U] << 8) |
             ((uint32_t)message->data[OS_SERVICE_NAME_MAX + 10U] << 16) |
             ((uint32_t)message->data[OS_SERVICE_NAME_MAX + 11U] << 24);
    if (event_out->old_owner_pid < 0 || event_out->new_owner_pid < 0 ||
        reason < OS_SERVICE_EVENT_PUBLISHED || reason > OS_SERVICE_EVENT_PURGED) return -1;
    event_out->reason = reason;
    return 0;
}

#endif
