#include "overlay.h"
#include "initrd.h"

#define OV_MAX_NODES 32
#define OV_PATH_MAX  64
#define OV_DATA_MAX  256

typedef struct {
    int used;
    int is_dir;
    char path[OV_PATH_MAX];
    char data[OV_DATA_MAX];
    uint32_t size;
} ov_node_t;

static ov_node_t g_ov[OV_MAX_NODES];

static int ov_len(const char* s) {
    int n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

static void ov_copy(char* dest, const char* src, int max) {
    int i = 0;
    if (!src) src = "";
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int ov_eq(const char* a, const char* b) {
    int i = 0;
    if (!a) a = "";
    if (!b) b = "";
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static void ov_normalize(const char* in, char* out, int max) {
    if (!in) {
        out[0] = '\0';
        return;
    }
    if (in[0] == '.' && in[1] == '/') in += 2;
    while (*in == '/') in++;
    ov_copy(out, in, max);
    {
        int n = ov_len(out);
        while (n > 0 && out[n - 1] == '/') {
            out[n - 1] = '\0';
            n--;
        }
    }
}

static void ov_parent(const char* path, char* out, int max) {
    int n = ov_len(path);
    int slash = -1;
    int i;
    for (i = 0; i < n; i++) {
        if (path[i] == '/') slash = i;
    }
    if (slash < 0) {
        out[0] = '\0';
        return;
    }
    if (slash >= max) slash = max - 1;
    for (i = 0; i < slash; i++) out[i] = path[i];
    out[slash] = '\0';
}

static ov_node_t* ov_find(const char* path) {
    char want[OV_PATH_MAX];
    int i;
    ov_normalize(path, want, OV_PATH_MAX);
    if (!want[0]) return 0;
    for (i = 0; i < OV_MAX_NODES; i++) {
        if (g_ov[i].used && ov_eq(g_ov[i].path, want)) {
            return &g_ov[i];
        }
    }
    return 0;
}

static ov_node_t* ov_alloc(void) {
    int i;
    for (i = 0; i < OV_MAX_NODES; i++) {
        if (!g_ov[i].used) return &g_ov[i];
    }
    return 0;
}

static int ov_has_children(const char* path) {
    char want[OV_PATH_MAX];
    int plen;
    int i;
    ov_normalize(path, want, OV_PATH_MAX);
    plen = ov_len(want);
    for (i = 0; i < OV_MAX_NODES; i++) {
        if (!g_ov[i].used) continue;
        if (plen == 0) {
            if (g_ov[i].path[0]) return 1;
        } else {
            int j = 0;
            while (j < plen && g_ov[i].path[j] == want[j]) j++;
            if (j == plen && g_ov[i].path[j] == '/') return 1;
        }
    }
    return 0;
}

static int ov_parent_is_dir(const char* path) {
    char parent[OV_PATH_MAX];
    ov_parent(path, parent, OV_PATH_MAX);
    if (!parent[0]) return 1;
    if (overlay_is_dir(parent)) return 1;
    if (initrd_is_dir(parent)) return 1;
    return 0;
}

static int ov_direct_child(const char* prefix, const char* path, char* name, int nmax) {
    int plen = ov_len(prefix);
    const char* rest;
    int i;
    if (plen == 0) {
        rest = path;
    } else {
        int j = 0;
        while (j < plen && path[j] == prefix[j]) j++;
        if (j != plen || path[j] != '/') return 0;
        rest = path + plen + 1;
    }
    if (!rest[0]) return 0;
    for (i = 0; rest[i]; i++) {
        if (rest[i] == '/') return 0;
    }
    ov_copy(name, rest, nmax);
    return 1;
}

void overlay_init(void) {
    int i;
    for (i = 0; i < OV_MAX_NODES; i++) {
        g_ov[i].used = 0;
        g_ov[i].path[0] = '\0';
        g_ov[i].size = 0;
        g_ov[i].is_dir = 0;
    }
}

int overlay_is_dir(const char* path) {
    ov_node_t* n;
    char want[OV_PATH_MAX];
    ov_normalize(path, want, OV_PATH_MAX);
    if (!want[0]) return 1;
    n = ov_find(want);
    return n && n->is_dir;
}

int overlay_stat(const char* path, os_dirent_t* out) {
    ov_node_t* n;
    char want[OV_PATH_MAX];
    const char* base;
    int i;
    ov_normalize(path, want, OV_PATH_MAX);
    if (!want[0]) {
        if (out) {
            out->name[0] = '/';
            out->name[1] = '\0';
            out->size = 0;
            out->flags = OS_DIRENT_DIR;
        }
        return OV_OK;
    }
    n = ov_find(want);
    if (!n) return OV_ERR_NOTFOUND;
    if (out) {
        base = want;
        for (i = 0; want[i]; i++) {
            if (want[i] == '/') base = want + i + 1;
        }
        ov_copy(out->name, base, OS_NAME_MAX);
        out->size = n->is_dir ? 0 : n->size;
        out->flags = n->is_dir ? OS_DIRENT_DIR : OS_DIRENT_FILE;
    }
    return OV_OK;
}

int overlay_mkdir(const char* path) {
    ov_node_t* n;
    char want[OV_PATH_MAX];
    ov_normalize(path, want, OV_PATH_MAX);
    if (!want[0]) return OV_ERR_EXISTS;
    if (ov_find(want)) return OV_ERR_EXISTS;
    if (initrd_is_file(want)) return OV_ERR_EXISTS;
    if (!ov_parent_is_dir(want)) return OV_ERR_NOTDIR;
    n = ov_alloc();
    if (!n) return OV_ERR_NOSPACE;
    n->used = 1;
    n->is_dir = 1;
    n->size = 0;
    n->data[0] = '\0';
    ov_copy(n->path, want, OV_PATH_MAX);
    return OV_OK;
}

int overlay_write(const char* path, const char* data, uint32_t n) {
    ov_node_t* node;
    char want[OV_PATH_MAX];
    uint32_t i;
    ov_normalize(path, want, OV_PATH_MAX);
    if (!want[0] || (n > 0 && !data)) return OV_ERR_INVAL;
    node = ov_find(want);
    if (node && node->is_dir) return OV_ERR_ISDIR;
    if (initrd_is_dir(want)) return OV_ERR_ISDIR;
    if (!node) {
        if (!ov_parent_is_dir(want)) return OV_ERR_NOTDIR;
        node = ov_alloc();
        if (!node) return OV_ERR_NOSPACE;
        node->used = 1;
        node->is_dir = 0;
        ov_copy(node->path, want, OV_PATH_MAX);
    }
    if (n > OV_DATA_MAX) n = OV_DATA_MAX;
    for (i = 0; i < n; i++) node->data[i] = data[i];
    node->size = n;
    return (int)n;
}

int overlay_append(const char* path, const char* data, uint32_t n) {
    ov_node_t* node;
    char want[OV_PATH_MAX];
    uint32_t i;
    ov_normalize(path, want, OV_PATH_MAX);
    if (!want[0] || (n > 0 && !data)) return OV_ERR_INVAL;
    if (initrd_is_dir(want)) return OV_ERR_ISDIR;
    node = ov_find(want);
    if (node && node->is_dir) return OV_ERR_ISDIR;
    if (!node) {
        uint32_t have = 0;
        char tmp[OV_DATA_MAX];
        if (initrd_is_file(want)) {
            int r = initrd_read_into(want, tmp, OV_DATA_MAX);
            if (r < 0) return r;
            have = (uint32_t)r;
        } else if (!ov_parent_is_dir(want)) {
            return OV_ERR_NOTDIR;
        }
        if (have + n > OV_DATA_MAX) return OV_ERR_NOSPACE;
        node = ov_alloc();
        if (!node) return OV_ERR_NOSPACE;
        node->used = 1;
        node->is_dir = 0;
        node->size = have;
        ov_copy(node->path, want, OV_PATH_MAX);
        for (i = 0; i < have; i++) node->data[i] = tmp[i];
    }
    if (node->size + n > OV_DATA_MAX) return OV_ERR_NOSPACE;
    for (i = 0; i < n; i++) node->data[node->size + i] = data[i];
    node->size += n;
    return (int)n;
}

int overlay_read(const char* path, char* buf, uint32_t max) {
    ov_node_t* n;
    uint32_t i;
    uint32_t copy;
    n = ov_find(path);
    if (!n) return OV_ERR_NOTFOUND;
    if (n->is_dir) return OV_ERR_ISDIR;
    if (!buf || max == 0) return OV_ERR_INVAL;
    copy = n->size;
    if (copy > max) copy = max;
    for (i = 0; i < copy; i++) buf[i] = n->data[i];
    return (int)copy;
}

int overlay_unlink(const char* path) {
    ov_node_t* n;
    char want[OV_PATH_MAX];
    ov_normalize(path, want, OV_PATH_MAX);
    if (!want[0]) return OV_ERR_INVAL;
    n = ov_find(want);
    if (!n) {
        if (initrd_is_file(want) || initrd_is_dir(want)) return OV_ERR_PROTECTED;
        return OV_ERR_NOTFOUND;
    }
    if (n->is_dir && ov_has_children(want)) return OV_ERR_NOTEMPTY;
    n->used = 0;
    n->path[0] = '\0';
    n->size = 0;
    return OV_OK;
}

static int ov_under(const char* path, const char* prefix, int plen) {
    int i = 0;
    while (i < plen && path[i] == prefix[i]) i++;
    if (i != plen) return 0;
    return path[plen] == '\0' || path[plen] == '/';
}

int overlay_rename(const char* oldpath, const char* newpath) {
    char oldp[OV_PATH_MAX];
    char newp[OV_PATH_MAX];
    int oldn;
    int newn;
    int i;

    ov_normalize(oldpath, oldp, OV_PATH_MAX);
    ov_normalize(newpath, newp, OV_PATH_MAX);
    if (!oldp[0] || !newp[0]) return OV_ERR_INVAL;
    if (ov_eq(oldp, newp)) return OV_OK;

    if (!ov_find(oldp)) {
        if (initrd_is_file(oldp) || initrd_is_dir(oldp)) return OV_ERR_PROTECTED;
        return OV_ERR_NOTFOUND;
    }
    if (ov_find(newp)) return OV_ERR_EXISTS;
    if (initrd_is_file(newp) || initrd_is_dir(newp)) return OV_ERR_EXISTS;
    if (!ov_parent_is_dir(newp)) return OV_ERR_NOTDIR;

    oldn = ov_len(oldp);
    newn = ov_len(newp);
    if (ov_under(newp, oldp, oldn) && newp[oldn] == '/') return OV_ERR_INVAL;

    for (i = 0; i < OV_MAX_NODES; i++) {
        int rest;
        if (!g_ov[i].used) continue;
        if (!ov_under(g_ov[i].path, oldp, oldn)) continue;
        rest = ov_len(g_ov[i].path) - oldn;
        if (newn + rest >= OV_PATH_MAX) return OV_ERR_INVAL;
    }

    for (i = 0; i < OV_MAX_NODES; i++) {
        char rest[OV_PATH_MAX];
        int r = 0;
        if (!g_ov[i].used) continue;
        if (!ov_under(g_ov[i].path, oldp, oldn)) continue;
        while (g_ov[i].path[oldn + r]) {
            rest[r] = g_ov[i].path[oldn + r];
            r++;
        }
        rest[r] = '\0';
        ov_copy(g_ov[i].path, newp, OV_PATH_MAX);
        {
            int k;
            for (k = 0; rest[k] && newn + k < OV_PATH_MAX - 1; k++) {
                g_ov[i].path[newn + k] = rest[k];
            }
            g_ov[i].path[newn + k] = '\0';
        }
    }
    return OV_OK;
}

static int ov_used_count(void) {
    int n = 0;
    int i;
    for (i = 0; i < OV_MAX_NODES; i++) {
        if (g_ov[i].used) n++;
    }
    return n;
}

int overlay_copy(const char* src, const char* dst) {
    ov_node_t* srcnode;
    char oldp[OV_PATH_MAX];
    char newp[OV_PATH_MAX];
    int oldn;
    int newn;
    int i;
    int need = 0;
    int free_n;

    ov_normalize(src, oldp, OV_PATH_MAX);
    ov_normalize(dst, newp, OV_PATH_MAX);
    if (!oldp[0] || !newp[0]) return OV_ERR_INVAL;
    if (ov_eq(oldp, newp)) return OV_ERR_EXISTS;

    srcnode = ov_find(oldp);
    if (!srcnode) {
        if (initrd_is_file(oldp) || initrd_is_dir(oldp)) return OV_ERR_PROTECTED;
        return OV_ERR_NOTFOUND;
    }
    if (ov_find(newp)) return OV_ERR_EXISTS;
    if (initrd_is_file(newp) || initrd_is_dir(newp)) return OV_ERR_EXISTS;
    if (!ov_parent_is_dir(newp)) return OV_ERR_NOTDIR;

    oldn = ov_len(oldp);
    newn = ov_len(newp);
    if (ov_under(newp, oldp, oldn) && newp[oldn] == '/') return OV_ERR_INVAL;

    for (i = 0; i < OV_MAX_NODES; i++) {
        int rest;
        if (!g_ov[i].used) continue;
        if (!ov_under(g_ov[i].path, oldp, oldn)) continue;
        rest = ov_len(g_ov[i].path) - oldn;
        if (newn + rest >= OV_PATH_MAX) return OV_ERR_INVAL;
        need++;
    }
    free_n = OV_MAX_NODES - ov_used_count();
    if (need > free_n) return OV_ERR_NOSPACE;

    for (i = 0; i < OV_MAX_NODES; i++) {
        ov_node_t* n;
        char rest[OV_PATH_MAX];
        int r = 0;
        int k;
        uint32_t b;
        if (!g_ov[i].used) continue;
        if (!ov_under(g_ov[i].path, oldp, oldn)) continue;
        n = ov_alloc();
        if (!n) return OV_ERR_NOSPACE;
        n->used = 1;
        while (g_ov[i].path[oldn + r]) {
            rest[r] = g_ov[i].path[oldn + r];
            r++;
        }
        rest[r] = '\0';
        ov_copy(n->path, newp, OV_PATH_MAX);
        for (k = 0; rest[k] && newn + k < OV_PATH_MAX - 1; k++) {
            n->path[newn + k] = rest[k];
        }
        n->path[newn + k] = '\0';
        n->used = 1;
        n->is_dir = g_ov[i].is_dir;
        n->size = g_ov[i].size;
        for (b = 0; b < n->size && b < OV_DATA_MAX; b++) {
            n->data[b] = g_ov[i].data[b];
        }
    }
    return OV_OK;
}

int overlay_listdir(const char* path, os_dirent_t* out, int start, int max_n) {
    char prefix[OV_PATH_MAX];
    int count = start;
    int i;
    if (!out || max_n <= 0) return start < 0 ? 0 : start;
    if (start < 0) start = 0;
    count = start;
    ov_normalize(path ? path : "/", prefix, OV_PATH_MAX);
    for (i = 0; i < OV_MAX_NODES && count < max_n; i++) {
        char name[OS_NAME_MAX];
        int e;
        int dup = 0;
        if (!g_ov[i].used) continue;
        if (!ov_direct_child(prefix, g_ov[i].path, name, OS_NAME_MAX)) continue;
        for (e = 0; e < count; e++) {
            if (ov_eq(out[e].name, name)) {
                out[e].flags = g_ov[i].is_dir ? OS_DIRENT_DIR : OS_DIRENT_FILE;
                out[e].size = g_ov[i].is_dir ? 0 : g_ov[i].size;
                dup = 1;
                break;
            }
        }
        if (dup) continue;
        ov_copy(out[count].name, name, OS_NAME_MAX);
        out[count].flags = g_ov[i].is_dir ? OS_DIRENT_DIR : OS_DIRENT_FILE;
        out[count].size = g_ov[i].is_dir ? 0 : g_ov[i].size;
        count++;
    }
    return count;
}
