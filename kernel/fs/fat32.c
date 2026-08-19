#include "fat32.h"

static uint8_t fat32_sector[512];

static uint16_t le16(const uint8_t* p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8U); }
static uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) | ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U); }
static void put32(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8U); p[2] = (uint8_t)(v >> 16U); p[3] = (uint8_t)(v >> 24U); }
static int power_of_two(uint8_t value) { return value != 0U && (value & (uint8_t)(value - 1U)) == 0U; }

int fat32_mount(fat32_volume_t* volume, fat16_read_sector_fn read_sector, uint32_t base_lba) {
    uint32_t total, fat_sectors, data_sectors, clusters;
    uint16_t reserved, root_entries, fat16_sectors;
    uint8_t spc, fats;
    if (!volume || !read_sector) return OS_FAT16_CORRUPT;
    volume->mounted = 0U;
    volume->read_sector = read_sector;
    volume->write_sector = 0;

    volume->base_lba = base_lba;
    if (read_sector(base_lba, fat32_sector) != 0) return OS_FAT16_CORRUPT;
    if (le16(fat32_sector + 11U) != 512U || !power_of_two(fat32_sector[13]) || fat32_sector[13] > 128U) return OS_FAT16_CORRUPT;
    reserved = le16(fat32_sector + 14U); fats = fat32_sector[16]; root_entries = le16(fat32_sector + 17U);
    fat16_sectors = le16(fat32_sector + 22U); total = le16(fat32_sector + 19U);
    if (total == 0U) total = le32(fat32_sector + 32U);
    fat_sectors = le32(fat32_sector + 36U);
    if (reserved == 0U || fats == 0U || fats > 2U || root_entries != 0U || fat16_sectors != 0U || total == 0U || fat_sectors == 0U) return OS_FAT16_CORRUPT;
    if (le16(fat32_sector + 510U) != 0xaa55U || le32(fat32_sector + 44U) < 2U) return OS_FAT16_CORRUPT;
    data_sectors = total - (uint32_t)reserved - (uint32_t)fats * fat_sectors;
    spc = fat32_sector[13];
    if (data_sectors == 0U || data_sectors < spc) return OS_FAT16_CORRUPT;
    clusters = data_sectors / spc;
    if (clusters < 65525U || clusters > FAT32_MAX_CLUSTER - 1U) return OS_FAT16_CORRUPT;
    volume->total_sectors = total; volume->fat_lba = base_lba + reserved; volume->fat_sectors = fat_sectors;
    volume->data_lba = volume->fat_lba + (uint32_t)fats * fat_sectors; volume->cluster_count = clusters;
    volume->root_cluster = le32(fat32_sector + 44U) & FAT32_MAX_CLUSTER; volume->bytes_per_sector = 512U;
    volume->sectors_per_cluster = spc; volume->fat_count = fats; volume->mounted = 1U;
    return 0;
}

int fat32_is_mounted(const fat32_volume_t* volume) { return volume && volume->mounted && volume->read_sector; }

int fat32_attach_writer(fat32_volume_t* volume, fat16_write_sector_fn write_sector) {
    if (!volume || !fat32_is_mounted(volume) || !write_sector) return OS_FAT16_NOT_MOUNTED;
    volume->write_sector = write_sector;
    return 0;
}

int fat32_write_fat_entry(const fat32_volume_t* volume, uint32_t cluster, uint32_t next) {
    uint32_t byte_offset, fat, lba, offset, current, updated;
    if (!volume || !fat32_is_mounted(volume) || !volume->write_sector || cluster < 2U || cluster > volume->cluster_count + 1U || next > FAT32_MAX_CLUSTER) return OS_FAT16_CORRUPT;
    byte_offset = cluster * 4U; offset = byte_offset & 511U;
    if (offset > 508U) return OS_FAT16_CORRUPT;
    for (fat = 0U; fat < volume->fat_count; fat++) {
        lba = volume->fat_lba + fat * volume->fat_sectors + (byte_offset >> 9U);
        if (volume->read_sector(lba, fat32_sector) != 0) return OS_FAT16_CORRUPT;
        current = le32(fat32_sector + offset); updated = (current & 0xf0000000U) | next;
        put32(fat32_sector + offset, updated);
        if (volume->write_sector(lba, fat32_sector) != 0) return OS_FAT16_CORRUPT;
    }
    return 0;
}

int fat32_allocate_cluster(const fat32_volume_t* volume, uint32_t* out_cluster) {
    uint32_t cluster, value;
    if (!volume || !out_cluster || !fat32_is_mounted(volume) || !volume->write_sector) return OS_FAT16_NOT_MOUNTED;
    for (cluster = 2U; cluster <= volume->cluster_count + 1U; cluster++) {
        if (fat32_read_fat_entry(volume, cluster, &value) != 0) return OS_FAT16_CORRUPT;
        if (value == 0U && fat32_write_fat_entry(volume, cluster, FAT32_EOC_MIN) == 0) { *out_cluster = cluster; return 0; }
    }
    return OS_FAT16_NOT_FOUND;
}

int fat32_link_clusters(const fat32_volume_t* volume, uint32_t source, uint32_t target) {
    uint32_t source_value, target_value;
    if (!volume || source < 2U || target < 2U || source == target) return OS_FAT16_CORRUPT;
    if (fat32_read_fat_entry(volume, source, &source_value) != 0 || fat32_read_fat_entry(volume, target, &target_value) != 0) return OS_FAT16_CORRUPT;
    if (source_value < FAT32_EOC_MIN || source_value == FAT32_BAD_CLUSTER || target_value == 0U || target_value == FAT32_BAD_CLUSTER) return OS_FAT16_CORRUPT;
    return fat32_write_fat_entry(volume, source, target);
}

int fat32_cluster_lba(const fat32_volume_t* volume, uint32_t cluster, uint32_t* out_lba) {
    if (!fat32_is_mounted(volume) || !out_lba || cluster < 2U || cluster > volume->cluster_count + 1U) return OS_FAT16_CORRUPT;
    *out_lba = volume->data_lba + (cluster - 2U) * volume->sectors_per_cluster;
    return 0;
}

int fat32_read_fat_entry(const fat32_volume_t* volume, uint32_t cluster, uint32_t* out_next) {
    uint32_t byte_offset, lba, offset, value;
    if (!fat32_is_mounted(volume) || !out_next || cluster < 2U || cluster > volume->cluster_count + 1U) return OS_FAT16_CORRUPT;
    byte_offset = cluster * 4U; lba = volume->fat_lba + (byte_offset >> 9U); offset = byte_offset & 511U;
    if (offset > 508U || volume->read_sector(lba, fat32_sector) != 0) return OS_FAT16_CORRUPT;
    value = le32(fat32_sector + offset) & FAT32_MAX_CLUSTER;
    *out_next = value;
    return 0;
}

int fat32_read_cluster(const fat32_volume_t* volume, uint32_t cluster, uint8_t* buffer) {
    uint32_t lba, i;
    if (!buffer || fat32_cluster_lba(volume, cluster, &lba) != 0) return OS_FAT16_CORRUPT;
    for (i = 0U; i < volume->sectors_per_cluster; i++) if (volume->read_sector(lba + i, buffer + i * 512U) != 0) return OS_FAT16_CORRUPT;
    return 0;
}

int fat32_write_cluster(const fat32_volume_t* volume, uint32_t cluster, const uint8_t* buffer) {
    uint32_t lba, i;
    if (!volume || !buffer || !volume->write_sector || fat32_cluster_lba(volume, cluster, &lba) != 0) return OS_FAT16_NOT_MOUNTED;
    for (i = 0U; i < volume->sectors_per_cluster; i++) if (volume->write_sector(lba + i, buffer + i * 512U) != 0) return OS_FAT16_CORRUPT;
    return 0;
}

static int fat32_short_name(const char* input, uint8_t out[11]) {
    uint32_t i = 0U, base = 0U, ext = 0U, dot = 0U;
    if (!input || !input[0]) return OS_FAT16_BAD_PATH;
    for (i = 0U; input[i]; i++) {
        char c = input[i];
        if (c == '/') return OS_FAT16_BAD_PATH;
        if (c == '.') { if (dot || base == 0U) return OS_FAT16_BAD_PATH; dot = 1U; continue; }
        if (c <= ' ' || c == '"' || c == '*' || c == '+' || c == ',' || c == ':' || c == ';' || c == '<' || c == '=' || c == '>' || c == '?' || c == '[' || c == '\\' || c == ']' || c == '|') return OS_FAT16_BAD_PATH;
        if (!dot) { if (base >= 8U) return OS_FAT16_BAD_PATH; out[base++] = (uint8_t)(c >= 'a' && c <= 'z' ? c - 32 : c); }
        else { if (ext >= 3U) return OS_FAT16_BAD_PATH; out[8U + ext++] = (uint8_t)(c >= 'a' && c <= 'z' ? c - 32 : c); }
    }
    if (base == 0U) return OS_FAT16_BAD_PATH;
    while (base < 8U) out[base++] = ' ';
    while (ext < 3U) out[8U + ext++] = ' ';
    return 0;
}

int fat32_create_root_entry(const fat32_volume_t* volume, const char* name, uint8_t attributes, uint32_t first_cluster, uint32_t size) {
    uint8_t short_name[11]; uint32_t cluster, next, lba, sector_index, entry_index, i;
    if (!volume || !fat32_is_mounted(volume) || !volume->write_sector) return OS_FAT16_NOT_MOUNTED;
    if (first_cluster < 2U || first_cluster > volume->cluster_count + 1U) return OS_FAT16_CORRUPT;
    if (fat32_short_name(name, short_name) != 0) return OS_FAT16_BAD_PATH;
    cluster = volume->root_cluster;
    for (i = 0U; i <= volume->cluster_count; i++) {
        if (fat32_cluster_lba(volume, cluster, &lba) != 0) return OS_FAT16_CORRUPT;
        for (sector_index = 0U; sector_index < volume->sectors_per_cluster; sector_index++) {
            if (volume->read_sector(lba + sector_index, fat32_sector) != 0) return OS_FAT16_CORRUPT;
            for (entry_index = 0U; entry_index < 16U; entry_index++) {
                uint8_t* entry = fat32_sector + entry_index * 32U;
                if (entry[0] != 0U && entry[0] != 0xe5U) continue;
                for (uint32_t j = 0U; j < 32U; j++) entry[j] = 0U;
                for (uint32_t j = 0U; j < 11U; j++) entry[j] = short_name[j];
                entry[11] = attributes; entry[20] = (uint8_t)(first_cluster >> 24U); entry[21] = (uint8_t)(first_cluster >> 16U);
                entry[26] = (uint8_t)first_cluster; entry[27] = (uint8_t)(first_cluster >> 8U);
                entry[28] = (uint8_t)size; entry[29] = (uint8_t)(size >> 8U); entry[30] = (uint8_t)(size >> 16U); entry[31] = (uint8_t)(size >> 24U);
                return volume->write_sector(lba + sector_index, fat32_sector) == 0 ? 0 : OS_FAT16_CORRUPT;
            }
        }
        if (fat32_read_fat_entry(volume, cluster, &next) != 0 || next >= FAT32_EOC_MIN) return OS_FAT16_NOT_FOUND;
        cluster = next;
    }
    return OS_FAT16_NOT_FOUND;
}
