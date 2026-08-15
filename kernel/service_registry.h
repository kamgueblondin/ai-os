#ifndef SERVICE_REGISTRY_H
#define SERVICE_REGISTRY_H

#include <stdint.h>
#include "os_syscalls.h"

#define SERVICE_REGISTRY_CAPACITY 8U
#define SERVICE_REGISTRY_WATCH_CAPACITY 8U
#define SERVICE_REGISTRY_BACKEND_CAPACITY OS_SERVICE_BACKEND_CAPACITY
#define SERVICE_BACKEND_RIGHT_READ 1U
#define SERVICE_BACKEND_RIGHT_MUTATE 2U
#define SERVICE_BACKEND_RIGHT_ALL (SERVICE_BACKEND_RIGHT_READ | SERVICE_BACKEND_RIGHT_MUTATE)

typedef struct {
    int32_t pid;
    char name[OS_SERVICE_NAME_MAX];
    uint32_t backend_generation;
} service_registry_entry_t;

typedef struct {
    int32_t pid;
    char name[OS_SERVICE_NAME_MAX];
} service_registry_watch_t;

void service_registry_init(void);
int service_registry_register(const char* name, int32_t pid);
int service_registry_lookup(const char* name);
int service_registry_remove(const char* name, int32_t pid);
int service_registry_grant(const char* name, int32_t owner_pid, int32_t grantee_pid);
int service_registry_remove_pid(int32_t pid);
int service_registry_subscribe(const char* name, int32_t pid);
int service_registry_collect_watchers(const char* name, int32_t* out, uint32_t max);
int service_registry_remove_watcher_pid(int32_t pid);
int service_registry_collect_owned(int32_t pid, service_registry_entry_t* out, uint32_t max);
int service_registry_pid_is_owner(int32_t pid);
int service_registry_name_valid(const char* name);
int service_registry_backend_grant(const char* name, int32_t owner_pid, int32_t grantee_pid);
int service_registry_backend_grant_scoped(const char* name, int32_t owner_pid, int32_t grantee_pid, uint32_t rights);
int service_registry_backend_revoke(const char* name, int32_t owner_pid, int32_t grantee_pid);
int service_registry_backend_rights(const char* name, int32_t owner_pid, int32_t grantee_pid, uint32_t* out_rights);
int service_registry_backend_list(const char* name, int32_t owner_pid, os_service_backend_list_t* out_list);
int service_registry_backend_observe(const char* name, int32_t owner_pid, uint32_t expected_generation,
                                     os_service_backend_snapshot_t* out_snapshot);
int service_registry_backend_allowed(const char* name, int32_t pid);
int service_registry_backend_allowed_for(const char* name, int32_t pid, uint32_t right);
void service_registry_backend_remove_name(const char* name);
void service_registry_backend_remove_pid(int32_t pid);

#endif
