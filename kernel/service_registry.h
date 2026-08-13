#ifndef SERVICE_REGISTRY_H
#define SERVICE_REGISTRY_H

#include <stdint.h>
#include "os_syscalls.h"

#define SERVICE_REGISTRY_CAPACITY 8U

typedef struct {
    int32_t pid;
    char name[OS_SERVICE_NAME_MAX];
} service_registry_entry_t;

void service_registry_init(void);
int service_registry_register(const char* name, int32_t pid);
int service_registry_lookup(const char* name);
int service_registry_remove(const char* name, int32_t pid);
int service_registry_remove_pid(int32_t pid);
int service_registry_name_valid(const char* name);

#endif
