#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include "os_syscalls.h"

/* Une capacité réduite évite qu'une tâche monopolise le tas noyau. */
#define IPC_ENDPOINT_CAPACITY 4U

typedef struct {
    os_ipc_message_t messages[IPC_ENDPOINT_CAPACITY];
    uint32_t read_index;
    uint32_t write_index;
    uint32_t count;
} ipc_endpoint_t;

void ipc_endpoint_init(ipc_endpoint_t* endpoint);
int ipc_endpoint_send(ipc_endpoint_t* endpoint, int32_t sender_pid,
                      const os_ipc_payload_t* payload);
int ipc_endpoint_receive(ipc_endpoint_t* endpoint, os_ipc_message_t* out);

#endif
