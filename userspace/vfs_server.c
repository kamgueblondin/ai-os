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

static int service_backend_grant(const char* name, int target_pid) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SERVICE_BACKEND_GRANT), "b"(name), "c"(target_pid));
    return result;
}

static int service_backend_grant_scoped(const char* name, int target_pid, uint32_t rights) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SERVICE_BACKEND_GRANT_SCOPED), "b"(name), "c"(target_pid), "d"(rights));
    return result;
}

static int service_backend_revoke(const char* name, int target_pid) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SERVICE_BACKEND_REVOKE), "b"(name), "c"(target_pid));
    return result;
}

static int service_backend_status(const char* name, int target_pid, uint32_t* rights) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SERVICE_BACKEND_STATUS), "b"(name), "c"(target_pid), "d"(rights));
    return result;
}

static int service_backend_list(const char* name, os_service_backend_list_t* list) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SERVICE_BACKEND_LIST), "b"(name), "c"(list));
    return result;
}

static int service_backend_observe(const char* name, uint32_t expected_generation,
                                   os_service_backend_snapshot_t* snapshot) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_SERVICE_BACKEND_OBSERVE), "b"(name),
                 "c"(expected_generation), "d"(snapshot));
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

static int string_equal_ascii_fold(const char* left, const char* right) {
    uint32_t i = 0U;
    while (left[i] != '\0' && right[i] != '\0') {
        char a = left[i];
        char b = right[i];
        if (a >= 'a' && a <= 'z') a = (char)(a - ('a' - 'A'));
        if (b >= 'a' && b <= 'z') b = (char)(b - ('a' - 'A'));
        if (a != b) return 0;
        i++;
    }
    return left[i] == right[i];
}

static int backend_initrd_read(const char* path, char* buffer, uint32_t max) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_INITRD_READ), "b"(path), "c"(buffer), "d"(max));
    return result;
}

static int backend_overlay_read(const char* path, char* buffer, uint32_t max) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_OVERLAY_READ), "b"(path), "c"(buffer), "d"(max));
    return result;
}
static int backend_fat16_read(const char* path, char* buffer, uint32_t max) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_FAT16_READ), "b"(path), "c"(buffer), "d"(max));
    return result;
}
static int backend_fat16_listdir(const char* path, os_dirent_t* out, int max_n) {
    int result;
    if (!path || path[0] != '/' || path[1] != '\0') return -1;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_FAT16_LIST), "b"(out), "c"(max_n));
    return result;
}
static int backend_fat16_listdir_page(const char* path, os_dirent_t* out, uint32_t start) {
    int result;
    if (!path || path[0] != '/' || path[1] != '\0') return -1;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_FAT16_LIST_PAGE), "b"(out),
                 "c"(OS_VFS_LIST_ENTRY_MAX + 1U), "d"(start));
    return result;
}
static int backend_fat16_stat(const char* path, os_dirent_t* out) {
    os_dirent_t entries[OS_VFS_LIST_ENTRY_MAX + 1U];
    int count, i;
    if (!path || !out || path[0] == '\0' || path[0] == '/') return -1;
    count = backend_fat16_listdir("/", entries, (int)(OS_VFS_LIST_ENTRY_MAX + 1U));
    if (count < 0) return count;
    for (i = 0; i < count; i++) {
        if (string_equal_ascii_fold(entries[i].name, path)) { *out = entries[i]; return 0; }
    }
    return -1;
}
static int backend_fat32_read(const char* path, char* buffer, uint32_t max) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_FAT32_READ), "b"(path), "c"(buffer), "d"(max));
    return result;
}
static int backend_fat32_listdir(const char* path, os_dirent_t* out, int max_n) {
    int result;
    if (!path || path[0] != '/' || path[1] != '\0') return -1;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_FAT32_LIST), "b"(out), "c"(max_n));
    return result;
}
static int backend_fat32_listdir_page(const char* path, os_dirent_t* out, uint32_t start) {
    int result;
    if (!path || path[0] != '/' || path[1] != '\0') return -1;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_FAT32_LIST_PAGE), "b"(out),
                 "c"(OS_VFS_LIST_ENTRY_MAX + 1U), "d"(start));
    return result;
}
static int backend_fat32_stat(const char* path, os_dirent_t* out) {
    os_dirent_t entries[OS_VFS_LIST_ENTRY_MAX + 1U];
    int count, i;
    if (!path || !out || path[0] == '\0' || path[0] == '/') return -1;
    count = backend_fat32_listdir("/", entries, (int)(OS_VFS_LIST_ENTRY_MAX + 1U));
    if (count < 0) return count;
    for (i = 0; i < count; i++) {
        if (string_equal_ascii_fold(entries[i].name, path)) { *out = entries[i]; return 0; }
    }
    return -1;
}

static int backend_initrd_stat(const char* path, os_dirent_t* out) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_INITRD_STAT), "b"(path), "c"(out));
    return result;
}

static int backend_overlay_stat(const char* path, os_dirent_t* out) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_OVERLAY_STAT), "b"(path), "c"(out));
    return result;
}

static int backend_initrd_listdir(const char* path, os_dirent_t* out, int max_n) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_INITRD_LISTDIR),
                 "b"(path), "c"(out), "d"(max_n));
    return result;
}

static int backend_overlay_listdir(const char* path, os_dirent_t* out, int max_n) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_OVERLAY_LISTDIR),
                 "b"(path), "c"(out), "d"(max_n));
    return result;
}

static int backend_initrd_listdir_page(const char* path, os_dirent_t* out, uint32_t start) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_INITRD_LISTDIR_PAGE),
                 "b"(path), "c"(out), "d"(start));
    return result;
}

static int backend_overlay_listdir_page(const char* path, os_dirent_t* out, uint32_t start) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_OVERLAY_LISTDIR_PAGE),
                 "b"(path), "c"(out), "d"(start));
    return result;
}

static int backend_write(const char* path, const uint8_t* data, uint32_t size) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_BACKEND_WRITE), "b"(path), "c"(data), "d"(size));
    return result;
}

static int backend_mkdir(const char* path) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_OVERLAY_MKDIR), "b"(path));
    return result;
}

static int backend_rmdir(const char* path) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_OVERLAY_RMDIR), "b"(path));
    return result;
}

static int backend_remove(const char* path) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_OVERLAY_UNLINK), "b"(path));
    return result;
}

static int backend_rename(const char* oldpath, const char* newpath) {
    int result;
    asm volatile("int $0x80" : "=a"(result) : "a"(SYS_VFS_OVERLAY_RENAME), "b"(oldpath), "c"(newpath));
    return result;
}

typedef struct {
    uint32_t source;
    int (*read)(const char* path, char* buffer, uint32_t max);
    int (*stat)(const char* path, os_dirent_t* out);
    int (*list)(const char* path, os_dirent_t* out, int max_n);
    int (*list_page)(const char* path, os_dirent_t* out, uint32_t start);
    int (*write)(const char* path, const uint8_t* data, uint32_t size);
    int (*mkdir)(const char* path);
    int (*rmdir)(const char* path);
    int (*remove)(const char* path);
    int (*rename)(const char* oldpath, const char* newpath);
} vfs_backend_ops_t;

static const vfs_backend_ops_t vfs_backend_ops[] = {
    { OS_VFS_MOUNT_SOURCE_INITRD, backend_initrd_read, backend_initrd_stat,
      backend_initrd_listdir, backend_initrd_listdir_page, 0, 0, 0, 0, 0 },
    { OS_VFS_MOUNT_SOURCE_OVERLAY, backend_overlay_read, backend_overlay_stat,
      backend_overlay_listdir, backend_overlay_listdir_page, backend_write, backend_mkdir,
      backend_rmdir, backend_remove, backend_rename },
    { OS_VFS_MOUNT_SOURCE_FAT16, backend_fat16_read, backend_fat16_stat,
      backend_fat16_listdir, backend_fat16_listdir_page, 0, 0, 0, 0, 0 },
    { OS_VFS_MOUNT_SOURCE_FAT32, backend_fat32_read, backend_fat32_stat,
      backend_fat32_listdir, backend_fat32_listdir_page, 0, 0, 0, 0, 0 },
};

static const vfs_backend_ops_t* vfs_backend_ops_for(uint32_t source) {
    uint32_t i;
    for (i = 0U; i < (uint32_t)(sizeof(vfs_backend_ops) / sizeof(vfs_backend_ops[0])); i++) {
        if (vfs_backend_ops[i].source == source) return &vfs_backend_ops[i];
    }
    return 0;
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

/* Table locale de montages : les entrées de démarrage sont protégées ; quatre
 * alias dynamiques restent possibles. Les noms dynamiques sont bornés pour
 * que la source virtuelle `vfs-mounts` tienne toujours dans 80 octets. */
#define VFS_MOUNT_MAX 8U
#define VFS_BOOT_MOUNT_COUNT 4U
#define VFS_DYNAMIC_MOUNT_MAX 13U
typedef struct {
    char prefix[OS_VFS_PATH_MAX];
    uint32_t source;
    uint32_t protected_mount;
} vfs_mount_t;
static vfs_mount_t vfs_mounts[VFS_MOUNT_MAX] = {
    { "initrd/", OS_VFS_MOUNT_SOURCE_INITRD, 1U },
    { "overlay/", OS_VFS_MOUNT_SOURCE_OVERLAY, 1U },
    { "fat16/", OS_VFS_MOUNT_SOURCE_FAT16, 1U },
    { "fat32/", OS_VFS_MOUNT_SOURCE_FAT32, 1U },
};
static uint32_t vfs_mount_count = VFS_BOOT_MOUNT_COUNT;

static uint32_t string_length(const char* text) {
    uint32_t i = 0U;
    while (text[i] != '\0') i++;
    return i;
}

static int mount_prefixes_overlap(const char* left, const char* right) {
    uint32_t i = 0U;
    while (left[i] != '\0' && right[i] != '\0' && left[i] == right[i]) i++;
    return left[i] == '\0' || right[i] == '\0';
}

static int vfs_mount_add(const char* prefix, uint32_t source) {
    uint32_t i;
    if (string_length(prefix) > VFS_DYNAMIC_MOUNT_MAX) return OS_VFS_STATUS_INVALID;
    for (i = 0U; i < vfs_mount_count; i++) {
        if (string_equal(prefix, vfs_mounts[i].prefix)) return OS_VFS_STATUS_MOUNT_EXISTS;
        if (mount_prefixes_overlap(prefix, vfs_mounts[i].prefix)) return OS_VFS_STATUS_INVALID;
    }
    if (vfs_mount_count >= VFS_MOUNT_MAX) return OS_VFS_STATUS_MOUNT_FULL;
    for (i = 0U; i < OS_VFS_PATH_MAX; i++) vfs_mounts[vfs_mount_count].prefix[i] = prefix[i];
    vfs_mounts[vfs_mount_count].source = source;
    vfs_mounts[vfs_mount_count].protected_mount = 0U;
    vfs_mount_count++;
    return OS_VFS_STATUS_OK;
}

static int vfs_mount_remove(const char* prefix) {
    uint32_t i;
    for (i = 0U; i < vfs_mount_count; i++) {
        if (string_equal(prefix, vfs_mounts[i].prefix)) {
            uint32_t j;
            if (vfs_mounts[i].protected_mount) return OS_VFS_STATUS_INVALID;
            for (j = i; j + 1U < vfs_mount_count; j++) vfs_mounts[j] = vfs_mounts[j + 1U];
            vfs_mount_count--;
            vfs_mounts[vfs_mount_count].prefix[0] = '\0';
            vfs_mounts[vfs_mount_count].source = 0U;
            vfs_mounts[vfs_mount_count].protected_mount = 0U;
            return OS_VFS_STATUS_OK;
        }
    }
    return OS_VFS_STATUS_NOT_MOUNTED;
}

/* Observabilité locale : compteurs volatils, remis à zéro au démarrage du
 * service. Toute requête VFS reconnue est comptée avant sa validation afin
 * que les refus de politique restent visibles. */
static uint32_t vfs_read_requests;
static uint32_t vfs_write_requests;
static uint32_t vfs_remove_requests;
static uint32_t vfs_rename_requests;
/* Génération volatile des contenus et de la table de montages. Elle n’est ni
 * persistante ni atomique : elle avertit seulement le client d’une mutation
 * visible entre deux pages. */
static uint32_t vfs_list_generation = 1U;

static uint32_t append_text(uint8_t* data, uint32_t offset, const char* text) {
    uint32_t i = 0U;
    while (text[i] != '\0') data[offset++] = (uint8_t)text[i++];
    return offset;
}

static uint32_t append_uint(uint8_t* data, uint32_t offset, uint32_t value) {
    char digits[10];
    uint32_t count = 0U;
    if (value == 0U) {
        data[offset++] = (uint8_t)'0';
        return offset;
    }
    while (value > 0U) {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (count > 0U) data[offset++] = (uint8_t)digits[--count];
    return offset;
}

static int read_virtual(const char* path, uint8_t* data, uint32_t* size) {
    static const char info[] = "vfsserver ring3 policy\n";
    const char* source = 0;
    uint32_t i;
    if (string_equal(path, "vfs-info")) source = info;
    else if (string_equal(path, "vfs-mounts")) {
        i = 0U;
        for (uint32_t mount_index = 0U; mount_index < vfs_mount_count; mount_index++) {
            uint32_t prefix_size = 0U;
            while (vfs_mounts[mount_index].prefix[prefix_size] != '\0') prefix_size++;
            /* suffixe fixe : espace, droits et saut de ligne. */
            if (i + prefix_size + 4U > OS_VFS_READ_MAX) break;
            i = append_text(data, i, vfs_mounts[mount_index].prefix);
            i = append_text(data, i, vfs_mounts[mount_index].source == OS_VFS_MOUNT_SOURCE_OVERLAY
                              ? " rw\n" : " ro\n");
        }
        *size = i;
        return 1;
    } else if (string_equal(path, "vfs-stats")) {
        i = append_text(data, 0U, "reads=");
        i = append_uint(data, i, vfs_read_requests);
        i = append_text(data, i, "\nwrites=");
        i = append_uint(data, i, vfs_write_requests);
        i = append_text(data, i, "\nremoves=");
        i = append_uint(data, i, vfs_remove_requests);
        i = append_text(data, i, "\nrenames=");
        i = append_uint(data, i, vfs_rename_requests);
        data[i++] = (uint8_t)'\n';
        *size = i;
        return 1;
    } else return 0;
    for (i = 0U; source[i] != '\0'; i++) data[i] = (uint8_t)source[i];
    *size = i;
    return 1;
}

/* Le backend ne reçoit jamais un chemin global : uniquement le suffixe d’un
 * montage déclaré par ce médiateur. */
static int read_mounted_backend(const char* path, uint8_t* data, uint32_t* size) {
    uint32_t i;
    for (i = 0U; i < vfs_mount_count; i++) {
        const char* relative = 0;
        if (os_vfs_match_mount(path, vfs_mounts[i].prefix, &relative)) {
            const vfs_backend_ops_t* ops = vfs_backend_ops_for(vfs_mounts[i].source);
            int read;
            if (!ops || !ops->read) return OS_VFS_STATUS_NOT_MOUNTED;
            read = ops->read(relative, (char*)data, OS_VFS_READ_MAX);
            if (read < 0) return read;
            *size = (uint32_t)read;
            return OS_VFS_STATUS_OK;
        }
    }
    return OS_VFS_STATUS_NOT_MOUNTED;
}

/* Les mutations restent limitées aux entrées de source overlay. */
static int stat_mounted_backend(const char* path, os_dirent_t* out) {
    uint32_t i;
    for (i = 0U; i < vfs_mount_count; i++) {
        const char* relative = 0;
        if (os_vfs_match_mount(path, vfs_mounts[i].prefix, &relative)) {
            const vfs_backend_ops_t* ops = vfs_backend_ops_for(vfs_mounts[i].source);
            return ops && ops->stat ? ops->stat(relative, out) : OS_VFS_STATUS_NOT_MOUNTED;
        }
    }
    return OS_VFS_STATUS_NOT_MOUNTED;
}

/* Le listage accepte la racine d’un montage ou un sous-répertoire qui lui
 * appartient. Chaque backend ne voit que son suffixe relatif ; la réponse
 * demeure une page texte bornée et la cinquième entrée détecte une page
 * incomplète. */
static int list_path_matches_mount(const char* path, const char* mount,
                                   const char** relative_out) {
    uint32_t i = 0U;
    if (!os_vfs_list_path_is_valid(path) || !os_vfs_mount_prefix_is_valid(mount)) return 0;
    while (mount[i] != '\0') {
        if (path[i] == '\0' || path[i] != mount[i]) return 0;
        i++;
    }
    if (path[i] == '\0') {
        if (relative_out) *relative_out = "/";
        return 1;
    }
    if (relative_out) *relative_out = path + i;
    return 1;
}

static int list_mounted_backend(const char* path, uint8_t* data, uint32_t* size,
                                uint32_t* count) {
    os_dirent_t entries[OS_VFS_LIST_ENTRY_MAX + 1U];
    uint32_t mount_index;
    uint32_t written = 0U;
    uint32_t emitted = 0U;
    int listed;
    int status = OS_VFS_STATUS_OK;
    if (!path || !data || !size || !count) return OS_VFS_STATUS_INVALID;
    for (mount_index = 0U; mount_index < vfs_mount_count; mount_index++) {
        const char* relative = 0;
        if (list_path_matches_mount(path, vfs_mounts[mount_index].prefix, &relative)) {
            const vfs_backend_ops_t* ops = vfs_backend_ops_for(vfs_mounts[mount_index].source);
            if (!ops || !ops->list) return OS_VFS_STATUS_NOT_MOUNTED;
            listed = ops->list(relative, entries, (int)(OS_VFS_LIST_ENTRY_MAX + 1U));
            if (listed < 0) return listed;
            for (uint32_t entry_index = 0U;
                 entry_index < (uint32_t)listed && entry_index < OS_VFS_LIST_ENTRY_MAX;
                 entry_index++) {
                uint32_t name_size = string_length(entries[entry_index].name);
                if (written + name_size + 1U > OS_VFS_LIST_DATA_MAX) {
                    status = OS_VFS_STATUS_TRUNCATED;
                    break;
                }
                for (uint32_t j = 0U; j < name_size; j++) {
                    data[written++] = (uint8_t)entries[entry_index].name[j];
                }
                data[written++] = (uint8_t)'\n';
                emitted++;
            }
            if ((uint32_t)listed > OS_VFS_LIST_ENTRY_MAX) status = OS_VFS_STATUS_TRUNCATED;
            *size = written;
            *count = emitted;
            return status;
        }
    }
    return OS_VFS_STATUS_NOT_MOUNTED;
}

static int list_virtual_mounts_page(uint32_t start, uint8_t* data, uint32_t data_max,
                                    uint32_t* size, uint32_t* count, uint32_t* next_start) {
    uint32_t index;
    uint32_t written = 0U;
    uint32_t emitted = 0U;
    if (!data || !size || !count || !next_start || start > vfs_mount_count) {
        return OS_VFS_STATUS_INVALID;
    }
    *next_start = OS_VFS_LIST_PAGE_END;
    for (index = start; index < vfs_mount_count && emitted < OS_VFS_LIST_ENTRY_MAX; index++) {
        uint32_t prefix_size = string_length(vfs_mounts[index].prefix);
        if (written + prefix_size + 4U > data_max) break;
        for (uint32_t j = 0U; j < prefix_size; j++) data[written++] = (uint8_t)vfs_mounts[index].prefix[j];
        data[written++] = (uint8_t)' ';
        data[written++] = (uint8_t)'r';
        data[written++] = (uint8_t)(vfs_mounts[index].source == OS_VFS_MOUNT_SOURCE_OVERLAY ? 'w' : 'o');
        data[written++] = (uint8_t)'\n';
        emitted++;
    }
    if (index < vfs_mount_count) {
        *next_start = index;
        *size = written;
        *count = emitted;
        return OS_VFS_STATUS_TRUNCATED;
    }
    *size = written;
    *count = emitted;
    return OS_VFS_STATUS_OK;
}

static int list_mounted_backend_page(const char* path, uint32_t start, uint8_t* data,
                                     uint32_t data_max, uint32_t* size, uint32_t* count,
                                     uint32_t* next_start) {
    os_dirent_t entries[OS_VFS_LIST_ENTRY_MAX + 1U];
    uint32_t mount_index;
    uint32_t written = 0U;
    uint32_t emitted = 0U;
    int listed;
    int status = OS_VFS_STATUS_OK;
    if (!path || !data || data_max == 0U || !size || !count || !next_start) return OS_VFS_STATUS_INVALID;
    *next_start = OS_VFS_LIST_PAGE_END;
    for (mount_index = 0U; mount_index < vfs_mount_count; mount_index++) {
        const char* relative = 0;
        if (!list_path_matches_mount(path, vfs_mounts[mount_index].prefix, &relative)) continue;
        {
            const vfs_backend_ops_t* ops = vfs_backend_ops_for(vfs_mounts[mount_index].source);
            if (!ops || !ops->list_page) return OS_VFS_STATUS_NOT_MOUNTED;
            listed = ops->list_page(relative, entries, start);
        }
        if (listed < 0) return listed;
        for (uint32_t entry_index = 0U;
             entry_index < (uint32_t)listed && entry_index < OS_VFS_LIST_ENTRY_MAX;
             entry_index++) {
            uint32_t name_size = string_length(entries[entry_index].name);
            if (written + name_size + 1U > data_max) {
                status = OS_VFS_STATUS_TRUNCATED;
                break;
            }
            for (uint32_t j = 0U; j < name_size; j++) data[written++] = (uint8_t)entries[entry_index].name[j];
            data[written++] = (uint8_t)'\n';
            emitted++;
        }
        if ((uint32_t)listed > emitted) {
            status = OS_VFS_STATUS_TRUNCATED;
            *next_start = start + (emitted == 0U ? 1U : emitted);
        }
        *size = written;
        *count = emitted;
        return status;
    }
    return OS_VFS_STATUS_NOT_MOUNTED;
}

static int write_mounted_backend(const char* path, const uint8_t* data, uint32_t size) {
    uint32_t i;
    for (i = 0U; i < vfs_mount_count; i++) {
        const char* relative = 0;
        if (os_vfs_match_mount(path, vfs_mounts[i].prefix, &relative)) {
            const vfs_backend_ops_t* ops = vfs_backend_ops_for(vfs_mounts[i].source);
            int written;
            if (!ops || !ops->write) return OS_VFS_STATUS_NOT_MOUNTED;
            written = ops->write(relative, data, size);
            return written < 0 ? written : OS_VFS_STATUS_OK;
        }
    }
    return OS_VFS_STATUS_NOT_MOUNTED;
}

static int mkdir_mounted_backend(const char* path) {
    uint32_t i;
    for (i = 0U; i < vfs_mount_count; i++) {
        const char* relative = 0;
        if (os_vfs_match_mount(path, vfs_mounts[i].prefix, &relative)) {
            const vfs_backend_ops_t* ops = vfs_backend_ops_for(vfs_mounts[i].source);
            return ops && ops->mkdir ? ops->mkdir(relative) : OS_VFS_STATUS_NOT_MOUNTED;
        }
    }
    return OS_VFS_STATUS_NOT_MOUNTED;
}

static int rmdir_mounted_backend(const char* path) {
    uint32_t i;
    for (i = 0U; i < vfs_mount_count; i++) {
        const char* relative = 0;
        if (os_vfs_match_mount(path, vfs_mounts[i].prefix, &relative)) {
            const vfs_backend_ops_t* ops = vfs_backend_ops_for(vfs_mounts[i].source);
            return ops && ops->rmdir ? ops->rmdir(relative) : OS_VFS_STATUS_NOT_MOUNTED;
        }
    }
    return OS_VFS_STATUS_NOT_MOUNTED;
}

static int remove_mounted_backend(const char* path) {
    uint32_t i;
    for (i = 0U; i < vfs_mount_count; i++) {
        const char* relative = 0;
        if (os_vfs_match_mount(path, vfs_mounts[i].prefix, &relative)) {
            const vfs_backend_ops_t* ops = vfs_backend_ops_for(vfs_mounts[i].source);
            return ops && ops->remove ? ops->remove(relative) : OS_VFS_STATUS_NOT_MOUNTED;
        }
    }
    return OS_VFS_STATUS_NOT_MOUNTED;
}

static int rename_mounted_backend(const char* oldpath, const char* newpath) {
    uint32_t i;
    for (i = 0U; i < vfs_mount_count; i++) {
        const char* old_relative = 0;
        const char* new_relative = 0;
        if (os_vfs_match_mount(oldpath, vfs_mounts[i].prefix, &old_relative) &&
            os_vfs_match_mount(newpath, vfs_mounts[i].prefix, &new_relative)) {
            const vfs_backend_ops_t* ops = vfs_backend_ops_for(vfs_mounts[i].source);
            return ops && ops->rename ? ops->rename(old_relative, new_relative)
                                      : OS_VFS_STATUS_NOT_MOUNTED;
        }
    }
    return OS_VFS_STATUS_NOT_MOUNTED;
}

void main(void) {
    os_ipc_message_t message;
    os_ipc_payload_t reply_payload;
    char path[OS_VFS_PATH_MAX];
    char new_path[OS_VFS_PATH_MAX];
    uint8_t data[OS_VFS_READ_MAX];
    uint8_t write_data[OS_VFS_WRITE_MAX];
    os_dirent_t metadata;
    if (service_register("vfs") != 0) {
        puts("vfsserver register failed\n");
        for (;;) yield();
    }
    puts("vfsserver ready vfs\n");
    puts("vfsserver mount initrd/ ro\n");
    puts("vfsserver mount overlay/ rw\n");
    for (;;) {
        int received = ipc_receive(&message);
        if (received == 0 && message.type == OS_IPC_VFS_LIST) {
            int status;
            uint32_t size = 0U;
            uint32_t count = 0U;
            puts("vfsserver list request\n");
            status = os_vfs_parse_list_request(&message, path);
            if (status == 0) {
                status = list_mounted_backend(path, data, &size, &count);
                if (status == OS_VFS_STATUS_NOT_MOUNTED) puts("vfsserver list outside mounts\n");
            }
            if (os_vfs_make_list_reply(&reply_payload, status, count, data, size,
                                       message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_LIST_PAGE) {
            int status;
            uint32_t size = 0U;
            uint32_t count = 0U;
            uint32_t start = 0U;
            uint32_t next_start = OS_VFS_LIST_PAGE_END;
            puts("vfsserver list page request\n");
            status = os_vfs_parse_list_page_request(&message, path, &start);
            if (status == 0) {
                status = string_equal(path, "vfs-mounts")
                    ? list_virtual_mounts_page(start, data, OS_VFS_LIST_PAGE_DATA_MAX,
                                               &size, &count, &next_start)
                    : list_mounted_backend_page(path, start, data, OS_VFS_LIST_PAGE_DATA_MAX,
                                                &size, &count, &next_start);
                if (status == OS_VFS_STATUS_NOT_MOUNTED) puts("vfsserver list page outside mounts\n");
            }
            if (os_vfs_make_list_page_reply(&reply_payload, status, count, next_start,
                                            data, size, message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_LIST_OBSERVE) {
            int status;
            uint32_t size = 0U;
            uint32_t count = 0U;
            uint32_t start = 0U;
            uint32_t expected_generation = 0U;
            uint32_t next_start = OS_VFS_LIST_PAGE_END;
            puts("vfsserver list observe request\n");
            status = os_vfs_parse_list_observe_request(&message, path, &start, &expected_generation);
            if (status == 0 && expected_generation != 0U && expected_generation != vfs_list_generation) {
                status = OS_VFS_STATUS_STALE;
            }
            if (status == 0) {
                status = string_equal(path, "vfs-mounts")
                    ? list_virtual_mounts_page(start, data, OS_VFS_LIST_OBSERVE_DATA_MAX,
                                               &size, &count, &next_start)
                    : list_mounted_backend_page(path, start, data, OS_VFS_LIST_OBSERVE_DATA_MAX,
                                                &size, &count, &next_start);
                if (status == OS_VFS_STATUS_NOT_MOUNTED) puts("vfsserver list observe outside mounts\n");
            }
            if (os_vfs_make_list_observe_reply(&reply_payload, status, count, next_start,
                                               vfs_list_generation, data, size,
                                               message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_STAT) {
            int status;
            puts("vfsserver stat request\n");
            status = os_vfs_parse_stat_request(&message, path);
            if (status == 0) {
                status = stat_mounted_backend(path, &metadata);
                if (status == OS_VFS_STATUS_NOT_MOUNTED) {
                    puts("vfsserver stat outside mounts\n");
                }
            }
            if (os_vfs_make_stat_reply(&reply_payload, status,
                                       status == 0 ? metadata.size : 0U,
                                       status == 0 ? metadata.flags : 0U,
                                       message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_READ) {
            int status;
            vfs_read_requests++;
            puts("vfsserver read request\n");
            status = os_vfs_parse_read_request(&message, path);
            uint32_t size = 0U;
            if (status == 0) {
                if (read_virtual(path, data, &size)) {
                    if (string_equal(path, "vfs-info")) puts("vfsserver virtual vfs-info\n");
                    else if (string_equal(path, "vfs-mounts")) puts("vfsserver virtual vfs-mounts\n");
                    else puts("vfsserver virtual vfs-stats\n");
                } else {
                    status = read_mounted_backend(path, data, &size);
                    if (status == OS_VFS_STATUS_NOT_MOUNTED) {
                        puts("vfsserver path outside mounts\n");
                    }
                }
            }
            if (os_vfs_make_read_reply(&reply_payload, status, data, size,
                                       message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_WRITE) {
            int status;
            uint32_t size = 0U;
            vfs_write_requests++;
            puts("vfsserver write request\n");
            status = os_vfs_parse_write_request(&message, path, write_data, &size);
            if (status == 0) {
                status = write_mounted_backend(path, write_data, size);
                if (status == OS_VFS_STATUS_NOT_MOUNTED) {
                    puts("vfsserver write outside mounts\n");
                }
            }
            if (status == OS_VFS_STATUS_OK) vfs_list_generation++;
            if (os_vfs_make_write_reply(&reply_payload, status, message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_MKDIR) {
            int status;
            puts("vfsserver mkdir request\n");
            status = os_vfs_parse_mkdir_request(&message, path);
            if (status == 0) {
                status = mkdir_mounted_backend(path);
                if (status == OS_VFS_STATUS_NOT_MOUNTED) puts("vfsserver mkdir outside mounts\n");
            }
            if (status == OS_VFS_STATUS_OK) vfs_list_generation++;
            if (os_vfs_make_mkdir_reply(&reply_payload, status, message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_RMDIR) {
            int status;
            puts("vfsserver rmdir request\n");
            status = os_vfs_parse_rmdir_request(&message, path);
            if (status == 0) {
                status = rmdir_mounted_backend(path);
                if (status == OS_VFS_STATUS_NOT_MOUNTED) puts("vfsserver rmdir outside mounts\n");
            }
            if (status == OS_VFS_STATUS_OK) vfs_list_generation++;
            if (os_vfs_make_rmdir_reply(&reply_payload, status, message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_REMOVE) {
            int status;
            vfs_remove_requests++;
            puts("vfsserver remove request\n");
            status = os_vfs_parse_remove_request(&message, path);
            if (status == 0) {
                status = remove_mounted_backend(path);
                if (status == OS_VFS_STATUS_NOT_MOUNTED) {
                    puts("vfsserver remove outside mounts\n");
                }
            }
            if (status == OS_VFS_STATUS_OK) vfs_list_generation++;
            if (os_vfs_make_remove_reply(&reply_payload, status, message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_RENAME) {
            int status;
            vfs_rename_requests++;
            puts("vfsserver rename request\n");
            status = os_vfs_parse_rename_request(&message, path, new_path);
            if (status == 0) {
                status = rename_mounted_backend(path, new_path);
                if (status == OS_VFS_STATUS_NOT_MOUNTED) {
                    puts("vfsserver rename outside mounts\n");
                }
            }
            if (status == OS_VFS_STATUS_OK) vfs_list_generation++;
            if (os_vfs_make_rename_reply(&reply_payload, status, message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_MOUNT_ADD) {
            uint32_t source;
            int status;
            puts("vfsserver mount add request\n");
            status = os_vfs_parse_mount_add_request(&message, path, &source);
            if (status == 0) status = vfs_mount_add(path, source);
            if (status == 0) {
                puts("vfsserver mount added ");
                puts(path);
                puts(source == OS_VFS_MOUNT_SOURCE_OVERLAY ? " overlay\n"
                     : (source == OS_VFS_MOUNT_SOURCE_FAT16 ? " fat16\n"
                        : (source == OS_VFS_MOUNT_SOURCE_FAT32 ? " fat32\n" : " initrd\n")));
            } else {
                puts("vfsserver mount add rc ");
                print_int(status);
                puts("\n");
            }
            if (status == OS_VFS_STATUS_OK) vfs_list_generation++;
            if (os_vfs_make_mount_reply(&reply_payload, OS_IPC_VFS_MOUNT_ADD_REPLY,
                                        status, message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_MOUNT_REMOVE) {
            int status;
            puts("vfsserver mount remove request\n");
            status = os_vfs_parse_mount_remove_request(&message, path);
            if (status == 0) status = vfs_mount_remove(path);
            if (status == 0) {
                puts("vfsserver mount removed ");
                puts(path);
                puts("\n");
            } else {
                puts("vfsserver mount remove rc ");
                print_int(status);
                puts("\n");
            }
            if (status == OS_VFS_STATUS_OK) vfs_list_generation++;
            if (os_vfs_make_mount_reply(&reply_payload, OS_IPC_VFS_MOUNT_REMOVE_REPLY,
                                        status, message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_BACKEND_GRANT) {
            int target_pid;
            int status;
            puts("vfsserver backend grant request\n");
            status = os_vfs_parse_backend_grant_request(&message, &target_pid);
            if (status == 0) status = service_backend_grant("vfs", target_pid);
            if (os_vfs_make_backend_grant_reply(&reply_payload, status, message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_BACKEND_GRANT_SCOPED) {
            int target_pid;
            int status;
            uint32_t rights;
            puts("vfsserver backend scoped grant request\n");
            status = os_vfs_parse_backend_grant_scoped_request(&message, &target_pid, &rights);
            if (status == 0) status = service_backend_grant_scoped("vfs", target_pid, rights);
            if (os_vfs_make_backend_grant_scoped_reply(&reply_payload, status, message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_BACKEND_REVOKE) {
            int target_pid;
            int status;
            puts("vfsserver backend revoke request\n");
            status = os_vfs_parse_backend_revoke_request(&message, &target_pid);
            if (status == 0) status = service_backend_revoke("vfs", target_pid);
            if (os_vfs_make_backend_revoke_reply(&reply_payload, status, message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_BACKEND_STATUS) {
            int target_pid;
            int status;
            uint32_t rights = 0U;
            puts("vfsserver backend status request\n");
            status = os_vfs_parse_backend_status_request(&message, &target_pid);
            if (status == 0) status = service_backend_status("vfs", target_pid, &rights);
            if (status != 0) rights = 0U;
            if (os_vfs_make_backend_status_reply(&reply_payload, status, rights, message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_BACKEND_OBSERVE) {
            os_service_backend_snapshot_t snapshot;
            uint32_t expected_generation;
            int status;
            uint32_t i;
            snapshot.generation = 0U;
            snapshot.list.count = 0U;
            for (i = 0U; i < OS_SERVICE_BACKEND_CAPACITY; i++) {
                snapshot.list.entries[i].pid = 0;
                snapshot.list.entries[i].rights = 0U;
            }
            puts("vfsserver backend observe request\n");
            status = os_vfs_parse_backend_observe_request(&message, &expected_generation);
            if (status == 0) status = service_backend_observe("vfs", expected_generation, &snapshot);
            if (os_vfs_make_backend_observe_reply(&reply_payload, status, &snapshot, message.request_id) == 0) {
                (void)ipc_send(message.sender_pid, &reply_payload);
            }
        } else if (received == 0 && message.type == OS_IPC_VFS_BACKEND_LIST) {
            os_service_backend_list_t list;
            int status;
            puts("vfsserver backend list request\n");
            status = os_vfs_parse_backend_list_request(&message);
            if (status == 0) status = service_backend_list("vfs", &list);
            if (os_vfs_make_backend_list_reply(&reply_payload, status,
                                               status == 0 ? &list : (const os_service_backend_list_t*)0,
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
