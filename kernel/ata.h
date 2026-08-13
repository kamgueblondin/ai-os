#ifndef ATA_H
#define ATA_H

#include <stdint.h>

/* ATA PIO LBA28 on the primary master (0x1F0). No IRQ14. */

int ata_init(void);
int ata_present(void);
int ata_read_sectors(uint32_t lba, uint32_t count, void* buf);
int ata_write_sectors(uint32_t lba, uint32_t count, const void* buf);

#endif
