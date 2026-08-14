#include "service_registry.h"

static service_registry_entry_t service_entries[SERVICE_REGISTRY_CAPACITY];
static service_registry_watch_t service_watches[SERVICE_REGISTRY_WATCH_CAPACITY];
typedef struct { int32_t owner_pid; int32_t grantee_pid; char name[OS_SERVICE_NAME_MAX]; } service_backend_cap_t;
static service_backend_cap_t service_backend_caps[SERVICE_REGISTRY_BACKEND_CAPACITY];

static int name_equal(const char* left, const char* right) {
    uint32_t i;
    for (i = 0U; i < OS_SERVICE_NAME_MAX; i++) {
        if (left[i] != right[i]) return 0;
        if (left[i] == '\0') return 1;
    }
    return 0;
}

static void copy_name(char* destination, const char* source) {
    uint32_t i;
    for (i = 0U; i < OS_SERVICE_NAME_MAX; i++) {
        destination[i] = source[i];
        if (source[i] == '\0') {
            for (i++; i < OS_SERVICE_NAME_MAX; i++) destination[i] = '\0';
            return;
        }
    }
}

int service_registry_name_valid(const char* name) {
    uint32_t i;
    if (!name || name[0] == '\0') return 0;
    for (i = 0U; i < OS_SERVICE_NAME_MAX; i++) {
        char c = name[i];
        if (c == '\0') return 1;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return 0;
}

void service_registry_init(void) {
    uint32_t i;
    for (i = 0U; i < SERVICE_REGISTRY_CAPACITY; i++) {
        service_entries[i].pid = 0;
        service_entries[i].name[0] = '\0';
    }
    for (i = 0U; i < SERVICE_REGISTRY_WATCH_CAPACITY; i++) {
        service_watches[i].pid = 0;
        service_watches[i].name[0] = '\0';
    }
    for (i = 0U; i < SERVICE_REGISTRY_BACKEND_CAPACITY; i++) {
        service_backend_caps[i].owner_pid = 0;
        service_backend_caps[i].grantee_pid = 0;
        service_backend_caps[i].name[0] = '\0';
    }
}

int service_registry_register(const char* name, int32_t pid) {
    uint32_t i;
    int free_slot = -1;
    if (!service_registry_name_valid(name) || pid <= 0) return OS_SERVICE_BAD_NAME;
    for (i = 0U; i < SERVICE_REGISTRY_CAPACITY; i++) {
        if (service_entries[i].pid == 0) {
            if (free_slot < 0) free_slot = (int)i;
            continue;
        }
        if (name_equal(service_entries[i].name, name)) {
            if (service_entries[i].pid == pid) return 0;
            return OS_SERVICE_TAKEN;
        }
    }
    if (free_slot < 0) return OS_SERVICE_FULL;
    service_entries[free_slot].pid = pid;
    copy_name(service_entries[free_slot].name, name);
    return 0;
}

int service_registry_lookup(const char* name) {
    uint32_t i;
    if (!service_registry_name_valid(name)) return OS_SERVICE_BAD_NAME;
    for (i = 0U; i < SERVICE_REGISTRY_CAPACITY; i++) {
        if (service_entries[i].pid > 0 && name_equal(service_entries[i].name, name)) {
            return service_entries[i].pid;
        }
    }
    return OS_SERVICE_NOT_FOUND;
}

int service_registry_remove(const char* name, int32_t pid) {
    uint32_t i;
    if (!service_registry_name_valid(name) || pid <= 0) return OS_SERVICE_BAD_NAME;
    for (i = 0U; i < SERVICE_REGISTRY_CAPACITY; i++) {
        if (service_entries[i].pid == pid && name_equal(service_entries[i].name, name)) {
            service_entries[i].pid = 0;
            service_entries[i].name[0] = '\0';
            service_registry_backend_remove_name(name);
            return 0;
        }
    }
    return OS_SERVICE_NOT_FOUND;
}

int service_registry_grant(const char* name, int32_t owner_pid, int32_t grantee_pid) {
    uint32_t i;
    if (!service_registry_name_valid(name) || owner_pid <= 0) return OS_SERVICE_BAD_NAME;
    if (grantee_pid <= 0) return OS_SERVICE_BAD_GRANTEE;
    for (i = 0U; i < SERVICE_REGISTRY_CAPACITY; i++) {
        if (name_equal(service_entries[i].name, name)) {
            if (service_entries[i].pid != owner_pid) return OS_SERVICE_NOT_OWNER;
            service_entries[i].pid = grantee_pid;
            service_registry_backend_remove_name(name);
            return 0;
        }
    }
    return OS_SERVICE_NOT_FOUND;
}

int service_registry_collect_owned(int32_t pid, service_registry_entry_t* out, uint32_t max) {
    uint32_t i;
    uint32_t count = 0U;
    if (pid <= 0 || (!out && max > 0U)) return OS_SERVICE_NOT_FOUND;
    for (i = 0U; i < SERVICE_REGISTRY_CAPACITY; i++) {
        if (service_entries[i].pid == pid) {
            if (count < max) {
                out[count].pid = pid;
                copy_name(out[count].name, service_entries[i].name);
            }
            count++;
        }
    }
    return (int)count;
}

int service_registry_pid_is_owner(int32_t pid) {
    uint32_t i;
    if (pid <= 0) return 0;
    for (i = 0U; i < SERVICE_REGISTRY_CAPACITY; i++) {
        if (service_entries[i].pid == pid) return 1;
    }
    return 0;
}

int service_registry_remove_pid(int32_t pid) {
    uint32_t i;
    int removed = 0;
    if (pid <= 0) return OS_SERVICE_NOT_FOUND;
    service_registry_backend_remove_pid(pid);
    for (i = 0U; i < SERVICE_REGISTRY_CAPACITY; i++) {
        if (service_entries[i].pid == pid) {
            service_entries[i].pid = 0;
            service_entries[i].name[0] = '\0';
            removed++;
        }
    }
    return removed > 0 ? 0 : OS_SERVICE_NOT_FOUND;
}

void service_registry_backend_remove_name(const char* name) {
    uint32_t i;
    if (!service_registry_name_valid(name)) return;
    for (i = 0U; i < SERVICE_REGISTRY_BACKEND_CAPACITY; i++) {
        if (service_backend_caps[i].owner_pid > 0 && name_equal(service_backend_caps[i].name, name)) {
            service_backend_caps[i].owner_pid = 0; service_backend_caps[i].grantee_pid = 0; service_backend_caps[i].name[0] = '\0';
        }
    }
}

void service_registry_backend_remove_pid(int32_t pid) {
    uint32_t i;
    if (pid <= 0) return;
    for (i = 0U; i < SERVICE_REGISTRY_BACKEND_CAPACITY; i++) {
        if (service_backend_caps[i].owner_pid == pid || service_backend_caps[i].grantee_pid == pid) {
            service_backend_caps[i].owner_pid = 0; service_backend_caps[i].grantee_pid = 0; service_backend_caps[i].name[0] = '\0';
        }
    }
}

int service_registry_backend_allowed(const char* name, int32_t pid) {
    uint32_t i;
    for (i = 0U; i < SERVICE_REGISTRY_BACKEND_CAPACITY; i++) {
        if (service_backend_caps[i].grantee_pid == pid && name_equal(service_backend_caps[i].name, name) &&
            service_registry_lookup(name) == service_backend_caps[i].owner_pid) return 1;
    }
    return 0;
}

int service_registry_backend_grant(const char* name, int32_t owner_pid, int32_t grantee_pid) {
    uint32_t i; int free_slot = -1;
    if (!service_registry_name_valid(name) || owner_pid <= 0 || grantee_pid <= 0) return OS_SERVICE_BAD_NAME;
    if (service_registry_lookup(name) != owner_pid) return OS_SERVICE_NOT_OWNER;
    for (i = 0U; i < SERVICE_REGISTRY_BACKEND_CAPACITY; i++) {
        if (service_backend_caps[i].owner_pid == owner_pid && service_backend_caps[i].grantee_pid == grantee_pid && name_equal(service_backend_caps[i].name, name)) return 0;
        if (service_backend_caps[i].owner_pid == 0 && free_slot < 0) free_slot = (int)i;
    }
    if (free_slot < 0) return OS_SERVICE_FULL;
    service_backend_caps[free_slot].owner_pid = owner_pid; service_backend_caps[free_slot].grantee_pid = grantee_pid;
    copy_name(service_backend_caps[free_slot].name, name); return 0;
}

int service_registry_backend_revoke(const char* name, int32_t owner_pid, int32_t grantee_pid) {
    uint32_t i;
    if (!service_registry_name_valid(name) || owner_pid <= 0 || grantee_pid <= 0) return OS_SERVICE_BAD_NAME;
    if (service_registry_lookup(name) != owner_pid) return OS_SERVICE_NOT_OWNER;
    for (i = 0U; i < SERVICE_REGISTRY_BACKEND_CAPACITY; i++) {
        if (service_backend_caps[i].owner_pid == owner_pid && service_backend_caps[i].grantee_pid == grantee_pid &&
            name_equal(service_backend_caps[i].name, name)) {
            service_backend_caps[i].owner_pid = 0; service_backend_caps[i].grantee_pid = 0; service_backend_caps[i].name[0] = '\0';
            return 0;
        }
    }
    return OS_SERVICE_NOT_FOUND;
}

int service_registry_subscribe(const char* name, int32_t pid) {
    uint32_t i;
    int free_slot = -1;
    if (!service_registry_name_valid(name) || pid <= 0) return OS_SERVICE_BAD_NAME;
    for (i = 0U; i < SERVICE_REGISTRY_WATCH_CAPACITY; i++) {
        if (service_watches[i].pid == 0) {
            if (free_slot < 0) free_slot = (int)i;
            continue;
        }
        if (service_watches[i].pid == pid && name_equal(service_watches[i].name, name)) {
            return 0;
        }
    }
    if (free_slot < 0) return OS_SERVICE_WATCH_FULL;
    service_watches[free_slot].pid = pid;
    copy_name(service_watches[free_slot].name, name);
    return 0;
}

int service_registry_collect_watchers(const char* name, int32_t* out, uint32_t max) {
    uint32_t i;
    uint32_t count = 0U;
    if (!service_registry_name_valid(name) || (!out && max > 0U)) return OS_SERVICE_BAD_NAME;
    for (i = 0U; i < SERVICE_REGISTRY_WATCH_CAPACITY; i++) {
        if (service_watches[i].pid > 0 && name_equal(service_watches[i].name, name)) {
            if (count < max) out[count] = service_watches[i].pid;
            count++;
        }
    }
    return (int)count;
}

int service_registry_remove_watcher_pid(int32_t pid) {
    uint32_t i;
    int removed = 0;
    if (pid <= 0) return OS_SERVICE_NOT_FOUND;
    for (i = 0U; i < SERVICE_REGISTRY_WATCH_CAPACITY; i++) {
        if (service_watches[i].pid == pid) {
            service_watches[i].pid = 0;
            service_watches[i].name[0] = '\0';
            removed++;
        }
    }
    return removed > 0 ? 0 : OS_SERVICE_NOT_FOUND;
}
