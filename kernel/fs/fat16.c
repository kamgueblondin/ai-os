#include "fat16.h"

#define FAT16_SECTOR_SIZE 512U
#define FAT16_ENTRY_SIZE 32U
#define FAT16_EOC_MIN 0xFFF8U
#define FAT16_BAD_CLUSTER 0xFFF7U
#define FAT16_MAX_ROOT_ENTRIES 512U
#define FAT16_MAX_CLUSTERS 65525U

static uint8_t sector[FAT16_SECTOR_SIZE];
static uint8_t sector2[FAT16_SECTOR_SIZE];
static const char* status_text = "FAT16: non monte";
static fat16_volume_t root_volume;

static uint16_t le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t le32(const uint8_t* p) {
    return (uint32_t)le16(p) | ((uint32_t)le16(p + 2) << 16);
}

static void copy_name(char* out, const uint8_t* entry) {
    uint32_t i;
    uint32_t pos = 0U;
    for (i = 0U; i < 8U && entry[i] != ' '; i++) out[pos++] = (char)entry[i];
    if (entry[8] != ' ' && entry[8] != 0U) {
        out[pos++] = '.';
        for (i = 8U; i < 11U && entry[i] != ' '; i++) out[pos++] = (char)entry[i];
    }
    out[pos] = '\0';
}

static char upper_ascii(char c) {
    if (c >= 'a' && c <= 'z') return (char)(c - 'a' + 'A');
    return c;
}

static int make_short_name(const char* name, uint8_t* out) {
    uint32_t i = 0U;
    uint32_t base = 0U;
    uint32_t ext = 0U;
    if (!name || !out || !name[0]) return OS_FAT16_BAD_PATH;
    for (i = 0U; i < 11U; i++) out[i] = ' ';
    i = 0U;
    while (name[i] && name[i] != '.' && base < 8U) {
        char c = upper_ascii(name[i++]);
        if (c < 0x21 || c == '/' || c == '\\') return OS_FAT16_BAD_PATH;
        out[base++] = (uint8_t)c;
    }
    if (!name[i]) return base ? 0 : OS_FAT16_BAD_PATH;
    if (name[i++] != '.') return OS_FAT16_BAD_PATH;
    while (name[i] && ext < 3U) {
        char c = upper_ascii(name[i++]);
        if (c < 0x21 || c == '.' || c == '/' || c == '\\') return OS_FAT16_BAD_PATH;
        out[8U + ext++] = (uint8_t)c;
    }
    return name[i] || ext == 0U ? OS_FAT16_BAD_PATH : 0;
}

static int entry_matches(const uint8_t* entry, const uint8_t* short_name) {
    uint32_t i;
    for (i = 0U; i < 11U; i++) {
        if (upper_ascii((char)entry[i]) != (char)short_name[i]) return 0;
    }
    return 1;
}

static int read_at(const fat16_volume_t* v, uint32_t lba, void* out) {
    if (!v || !v->read_sector || !out || lba < v->base_lba ||
        lba - v->base_lba >= v->total_sectors) return OS_FAT16_CORRUPT;
    return v->read_sector(lba, out) == 0 ? 0 : OS_FAT16_CORRUPT;
}

static int read_fat_entry(const fat16_volume_t* v, uint16_t cluster, uint16_t* next) {
    uint32_t byte_offset = (uint32_t)cluster * 2U;
    uint32_t lba = v->fat_lba + (byte_offset >> 9U);
    uint32_t offset = byte_offset & 511U;
    if (!next || offset > 510U || (lba - v->base_lba) >= v->total_sectors) return OS_FAT16_CORRUPT;
    if (read_at(v, lba, sector) != 0) return OS_FAT16_CORRUPT;
    *next = le16(sector + offset);
    return 0;
}

static int read_root_entry(const fat16_volume_t* v, uint32_t index, uint8_t* entry) {
    uint32_t lba;
    uint32_t offset;
    if (!v || !entry || index >= v->root_entries) return OS_FAT16_NOT_FOUND;
    lba = v->root_lba + ((index * FAT16_ENTRY_SIZE) >> 9U);
    offset = (index * FAT16_ENTRY_SIZE) & 511U;
    if (read_at(v, lba, sector) != 0) return OS_FAT16_CORRUPT;
    for (uint32_t i = 0U; i < FAT16_ENTRY_SIZE; i++) entry[i] = sector[offset + i];
    return 0;
}

fat16_volume_t* fat16_root(void) {
    return &root_volume;
}

int fat16_mount(fat16_volume_t* v, fat16_read_sector_fn read_sector, uint32_t base_lba) {
    uint32_t total;
    uint32_t fat_lba;
    uint32_t root_lba;
    uint32_t data_lba;
    uint32_t data_sectors;
    uint32_t clusters;
    uint16_t reserved;
    uint16_t fat_sectors;
    uint16_t root_entries;
    uint8_t fats;
    uint8_t spc;
    if (!v || !read_sector) return OS_FAT16_CORRUPT;
    v->mounted = 0U;
    v->read_sector = read_sector;
    v->base_lba = base_lba;
    if (read_sector(base_lba, sector) != 0) {
        status_text = "FAT16: secteur boot illisible";
        return OS_FAT16_CORRUPT;
    }
    if (le16(sector + 11U) != FAT16_SECTOR_SIZE || sector[13] == 0U ||
        le16(sector + 14U) == 0U || sector[16] == 0U || sector[16] > 2U) {
        status_text = "FAT16: BPB invalide";
        return OS_FAT16_CORRUPT;
    }
    spc = sector[13];
    if ((spc & (uint8_t)(spc - 1U)) != 0U || spc > 128U) {
        status_text = "FAT16: clusters invalides";
        return OS_FAT16_CORRUPT;
    }
    reserved = le16(sector + 14U);
    fats = sector[16];
    root_entries = le16(sector + 17U);
    fat_sectors = le16(sector + 22U);
    total = le16(sector + 19U);
    if (total == 0U) total = le32(sector + 32U);
    if (total == 0U || fat_sectors == 0U || root_entries == 0U ||
        root_entries > FAT16_MAX_ROOT_ENTRIES) {
        status_text = "FAT16: tailles BPB invalides";
        return OS_FAT16_CORRUPT;
    }
    root_lba = base_lba + reserved + (uint32_t)fats * fat_sectors;
    if ((uint32_t)root_entries * FAT16_ENTRY_SIZE > 0xffffffffU - 511U) return OS_FAT16_CORRUPT;
    v->root_sectors = ((uint32_t)root_entries * FAT16_ENTRY_SIZE + 511U) >> 9U;
    data_lba = root_lba + v->root_sectors;
    if (data_lba < base_lba || data_lba - base_lba >= total) return OS_FAT16_CORRUPT;
    data_sectors = total - (data_lba - base_lba);
    clusters = data_sectors / spc;
    if (clusters < 4085U || clusters > FAT16_MAX_CLUSTERS) {
        status_text = "FAT16: nombre de clusters hors plage";
        return OS_FAT16_CORRUPT;
    }
    fat_lba = base_lba + reserved;
    v->total_sectors = total;
    v->fat_lba = fat_lba;
    v->fat_sectors = fat_sectors;
    v->root_lba = root_lba;
    v->data_lba = data_lba;
    v->cluster_count = clusters;
    v->bytes_per_sector = FAT16_SECTOR_SIZE;
    v->sectors_per_cluster = spc;
    v->fat_count = fats;
    v->root_entries = root_entries;
    v->mounted = 1U;
    status_text = "FAT16: volume lecture seule monte";
    return 0;
}

int fat16_is_mounted(const fat16_volume_t* v) {
    return v && v->mounted != 0U;
}

int fat16_list_root(const fat16_volume_t* v, os_fat16_dirent_t* out, uint32_t capacity) {
    uint32_t i;
    uint32_t count = 0U;
    uint8_t entry[FAT16_ENTRY_SIZE];
    if (!fat16_is_mounted(v)) return OS_FAT16_NOT_MOUNTED;
    if (!out || capacity == 0U) return OS_FAT16_BAD_PATH;
    for (i = 0U; i < v->root_entries; i++) {
        if (read_root_entry(v, i, entry) != 0) return OS_FAT16_CORRUPT;
        if (entry[0] == 0x00U) break;
        if (entry[0] == 0xE5U || entry[11] == 0x0FU || (entry[11] & 0x08U)) continue;
        if (count >= capacity) return (int)count;
        copy_name(out[count].name, entry);
        out[count].size = le32(entry + 28U);
        out[count].flags = (entry[11] & 0x10U) ? OS_DIRENT_DIR : OS_DIRENT_FILE;
        count++;
    }
    return (int)count;
}

int fat16_read_file(const fat16_volume_t* v, const char* name, char* buffer, uint32_t max) {
    uint8_t short_name[11];
    uint8_t entry[FAT16_ENTRY_SIZE];
    uint32_t i;
    uint32_t size;
    uint32_t copied = 0U;
    uint16_t cluster;
    uint32_t guard = 0U;
    if (!fat16_is_mounted(v)) return OS_FAT16_NOT_MOUNTED;
    if (!buffer || max == 0U) return OS_FAT16_BUFFER_SMALL;
    if (make_short_name(name, short_name) != 0) return OS_FAT16_BAD_PATH;
    for (i = 0U; i < v->root_entries; i++) {
        if (read_root_entry(v, i, entry) != 0) return OS_FAT16_CORRUPT;
        if (entry[0] == 0x00U) break;
        if (entry[0] == 0xE5U || entry[11] == 0x0FU || (entry[11] & 0x08U)) continue;
        if (!entry_matches(entry, short_name)) continue;
        if (entry[11] & 0x10U) return OS_FAT16_BAD_PATH;
        size = le32(entry + 28U);
        if (size > max) return OS_FAT16_BUFFER_SMALL;
        cluster = le16(entry + 26U);
        while (copied < size) {
            uint32_t s;
            uint32_t take;
            if (cluster < 2U || cluster >= FAT16_EOC_MIN || cluster - 2U >= v->cluster_count || guard++ > v->cluster_count) return OS_FAT16_CORRUPT;
            for (s = 0U; s < v->sectors_per_cluster && copied < size; s++) {
                uint32_t lba = v->data_lba + (uint32_t)(cluster - 2U) * v->sectors_per_cluster + s;
                if (read_at(v, lba, sector2) != 0) return OS_FAT16_CORRUPT;
                take = size - copied;
                if (take > FAT16_SECTOR_SIZE) take = FAT16_SECTOR_SIZE;
                for (i = 0U; i < take; i++) buffer[copied + i] = (char)sector2[i];
                copied += take;
            }
            if (copied < size && read_fat_entry(v, cluster, &cluster) != 0) return OS_FAT16_CORRUPT;
        }
        return (int)copied;
    }
    return OS_FAT16_NOT_FOUND;
}

const char* fat16_status(void) {
    return status_text;
}


int fat16_read_file_range(const fat16_volume_t* v, const char* name,
                          uint32_t offset, uint8_t* buffer, uint32_t max,
                          uint32_t* out_read) {
    uint8_t short_name[11];
    uint8_t entry[FAT16_ENTRY_SIZE];
    uint32_t i;
    uint32_t size;
    uint32_t cluster_bytes;
    uint32_t skip_clusters;
    uint32_t intra;
    uint32_t copied = 0U;
    uint16_t cluster;
    uint32_t guard = 0U;
    if (out_read) *out_read = 0U;
    if (!fat16_is_mounted(v)) return OS_FAT16_NOT_MOUNTED;
    if (!buffer || max == 0U || !out_read) return OS_FAT16_BUFFER_SMALL;
    if (make_short_name(name, short_name) != 0) return OS_FAT16_BAD_PATH;
    for (i = 0U; i < v->root_entries; i++) {
        if (read_root_entry(v, i, entry) != 0) return OS_FAT16_CORRUPT;
        if (entry[0] == 0x00U) break;
        if (entry[0] == 0xE5U || entry[11] == 0x0FU || (entry[11] & 0x08U)) continue;
        if (!entry_matches(entry, short_name)) continue;
        if (entry[11] & 0x10U) return OS_FAT16_BAD_PATH;
        size = le32(entry + 28U);
        if (offset > size) return OS_FAT16_BAD_PATH;
        cluster = le16(entry + 26U);
        cluster_bytes = (uint32_t)v->sectors_per_cluster * FAT16_SECTOR_SIZE;
        if (cluster_bytes == 0U) return OS_FAT16_CORRUPT;
        skip_clusters = offset / cluster_bytes;
        intra = offset % cluster_bytes;
        while (skip_clusters-- > 0U) {
            if (cluster < 2U || cluster >= FAT16_EOC_MIN ||
                cluster - 2U >= v->cluster_count || guard++ > v->cluster_count ||
                read_fat_entry(v, cluster, &cluster) != 0) return OS_FAT16_CORRUPT;
        }
        while (copied < max && offset + copied < size) {
            uint32_t sector_in_cluster = intra / FAT16_SECTOR_SIZE;
            uint32_t sector_offset = intra % FAT16_SECTOR_SIZE;
            uint32_t lba;
            uint32_t take = FAT16_SECTOR_SIZE - sector_offset;
            if (cluster < 2U || cluster >= FAT16_EOC_MIN ||
                cluster - 2U >= v->cluster_count || guard++ > v->cluster_count) return OS_FAT16_CORRUPT;
            lba = v->data_lba + (uint32_t)(cluster - 2U) * v->sectors_per_cluster + sector_in_cluster;
            if (read_at(v, lba, sector2) != 0) return OS_FAT16_CORRUPT;
            if (take > max - copied) take = max - copied;
            if (take > size - offset - copied) take = size - offset - copied;
            for (i = 0U; i < take; i++) buffer[copied + i] = sector2[sector_offset + i];
            copied += take;
            intra += take;
            if (intra >= cluster_bytes && copied < max && offset + copied < size) {
                if (read_fat_entry(v, cluster, &cluster) != 0) return OS_FAT16_CORRUPT;
                intra = 0U;
            }
        }
        *out_read = copied;
        return 0;
    }
    return OS_FAT16_NOT_FOUND;
}


int fat16_open_file(const fat16_volume_t* v, const char* name, fat16_file_t* out) {
    uint8_t short_name[11];
    uint8_t entry[FAT16_ENTRY_SIZE];
    uint32_t i;
    if (!fat16_is_mounted(v) || !out) return OS_FAT16_NOT_MOUNTED;
    if (make_short_name(name, short_name) != 0) return OS_FAT16_BAD_PATH;
    out->open = 0U;
    for (i = 0U; i < v->root_entries; i++) {
        if (read_root_entry(v, i, entry) != 0) return OS_FAT16_CORRUPT;
        if (entry[0] == 0x00U) break;
        if (entry[0] == 0xE5U || entry[11] == 0x0FU || (entry[11] & 0x08U)) continue;
        if (!entry_matches(entry, short_name)) continue;
        if (entry[11] & 0x10U) return OS_FAT16_BAD_PATH;
        out->volume = v;
        out->cluster = le16(entry + 26U);
        out->size = le32(entry + 28U);
        out->position = 0U;
        out->cluster_offset = 0U;
        out->guard = 0U;
        out->open = 1U;
        return 0;
    }
    return OS_FAT16_NOT_FOUND;
}

int fat16_file_read(fat16_file_t* file, uint8_t* buffer, uint32_t max,
                    uint32_t* out_read) {
    const fat16_volume_t* v;
    uint32_t cluster_bytes;
    uint32_t copied = 0U;
    if (out_read) *out_read = 0U;
    if (!file || !file->open || !file->volume || !buffer || max == 0U || !out_read) return OS_FAT16_BUFFER_SMALL;
    v = file->volume;
    cluster_bytes = (uint32_t)v->sectors_per_cluster * FAT16_SECTOR_SIZE;
    if (cluster_bytes == 0U || file->position > file->size) return OS_FAT16_CORRUPT;
    while (copied < max && file->position < file->size) {
        uint32_t sector_in_cluster = file->cluster_offset / FAT16_SECTOR_SIZE;
        uint32_t sector_offset = file->cluster_offset % FAT16_SECTOR_SIZE;
        uint32_t take = FAT16_SECTOR_SIZE - sector_offset;
        uint32_t lba;
        if (file->cluster < 2U || file->cluster >= FAT16_EOC_MIN ||
            file->cluster - 2U >= v->cluster_count || file->guard++ > v->cluster_count) return OS_FAT16_CORRUPT;
        lba = v->data_lba + (uint32_t)(file->cluster - 2U) * v->sectors_per_cluster + sector_in_cluster;
        if (read_at(v, lba, sector2) != 0) return OS_FAT16_CORRUPT;
        if (take > max - copied) take = max - copied;
        if (take > file->size - file->position) take = file->size - file->position;
        for (uint32_t i = 0U; i < take; i++) buffer[copied + i] = sector2[sector_offset + i];
        copied += take;
        file->position += take;
        file->cluster_offset += take;
        if (file->cluster_offset >= cluster_bytes && file->position < file->size) {
            if (read_fat_entry(v, file->cluster, &file->cluster) != 0) return OS_FAT16_CORRUPT;
            file->cluster_offset = 0U;
        }
    }
    *out_read = copied;
    return 0;
}
