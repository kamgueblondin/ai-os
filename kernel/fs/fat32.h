#ifndef AIOS_FAT32_H
#define AIOS_FAT32_H

#include <stdint.h>
#include "fat16.h"

#define FAT32_EOC_MIN 0x0ffffff8U
#define FAT32_BAD_CLUSTER 0x0ffffff7U
#define FAT32_MAX_CLUSTER 0x0fffffffU

typedef struct {
    fat16_read_sector_fn read_sector;
    fat16_write_sector_fn write_sector;
    uint32_t base_lba;
    uint32_t total_sectors;
    uint32_t fat_lba;
    uint32_t fat_sectors;
    uint32_t data_lba;
    uint32_t cluster_count;
    uint32_t root_cluster;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint8_t fat_count;
    uint8_t mounted;
} fat32_volume_t;

int fat32_mount(fat32_volume_t* volume, fat16_read_sector_fn read_sector, uint32_t base_lba);
int fat32_is_mounted(const fat32_volume_t* volume);
int fat32_attach_writer(fat32_volume_t* volume, fat16_write_sector_fn write_sector);
int fat32_write_fat_entry(const fat32_volume_t* volume, uint32_t cluster, uint32_t next);
int fat32_allocate_cluster(const fat32_volume_t* volume, uint32_t* out_cluster);
int fat32_link_clusters(const fat32_volume_t* volume, uint32_t source, uint32_t target);
int fat32_read_fat_entry(const fat32_volume_t* volume, uint32_t cluster, uint32_t* out_next);
int fat32_cluster_lba(const fat32_volume_t* volume, uint32_t cluster, uint32_t* out_lba);
int fat32_read_cluster(const fat32_volume_t* volume, uint32_t cluster, uint8_t* buffer);
int fat32_write_cluster(const fat32_volume_t* volume, uint32_t cluster, const uint8_t* buffer);
int fat32_create_root_entry(const fat32_volume_t* volume, const char* name, uint8_t attributes, uint32_t first_cluster, uint32_t size);

#endif
