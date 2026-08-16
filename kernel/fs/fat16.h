#ifndef AIOS_FAT16_H
#define AIOS_FAT16_H

#include <stdint.h>
#include "../../include/os_syscalls.h"

typedef int (*fat16_read_sector_fn)(uint32_t lba, void* buffer);

typedef struct {
    fat16_read_sector_fn read_sector;
    uint32_t base_lba;
    uint32_t total_sectors;
    uint32_t fat_lba;
    uint32_t fat_sectors;
    uint32_t root_lba;
    uint32_t root_sectors;
    uint32_t data_lba;
    uint32_t cluster_count;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint8_t fat_count;
    uint16_t root_entries;
    uint8_t mounted;
} fat16_volume_t;

fat16_volume_t* fat16_root(void);
int fat16_mount(fat16_volume_t* volume, fat16_read_sector_fn read_sector,
                uint32_t base_lba);
int fat16_is_mounted(const fat16_volume_t* volume);
int fat16_list_root(const fat16_volume_t* volume, os_fat16_dirent_t* out,
                    uint32_t capacity);
int fat16_read_file(const fat16_volume_t* volume, const char* name,
                    char* buffer, uint32_t max);
/* Lit au plus max octets à partir d’un offset sans charger tout le fichier. */
int fat16_read_file_range(const fat16_volume_t* volume, const char* name,
                          uint32_t offset, uint8_t* buffer, uint32_t max,
                          uint32_t* out_read);
const char* fat16_status(void);

#endif
