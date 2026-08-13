#include "ata.h"

extern unsigned char inb(unsigned short port);
extern void outb(unsigned short port, unsigned char data);

#define ATA_DATA     0x1F0
#define ATA_ERROR    0x1F1
#define ATA_SECCOUNT 0x1F2
#define ATA_LBA0     0x1F3
#define ATA_LBA1     0x1F4
#define ATA_LBA2     0x1F5
#define ATA_DRIVE    0x1F6
#define ATA_CMD      0x1F7
#define ATA_STATUS   0x1F7
#define ATA_ALTSTAT  0x3F6

#define ATA_SR_ERR  0x01
#define ATA_SR_DRQ  0x08
#define ATA_SR_DF   0x20
#define ATA_SR_BSY  0x80

#define ATA_CMD_READ_PIO  0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_IDENTIFY  0xEC

#define ATA_TIMEOUT 500000u

static int g_ata_present;

static unsigned short ata_inw(unsigned short port) {
    unsigned short ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "dN"(port));
    return ret;
}

static void ata_outw(unsigned short port, unsigned short data) {
    asm volatile ("outw %0, %1" : : "a"(data), "dN"(port));
}

static void ata_io_delay(void) {
    (void)inb(ATA_ALTSTAT);
    (void)inb(ATA_ALTSTAT);
    (void)inb(ATA_ALTSTAT);
    (void)inb(ATA_ALTSTAT);
}

static int ata_wait_not_busy(void) {
    uint32_t i;
    for (i = 0; i < ATA_TIMEOUT; i++) {
        uint8_t st = inb(ATA_STATUS);
        if (st == 0xFF) return -1;
        if ((st & ATA_SR_BSY) == 0) return (int)st;
    }
    return -1;
}

static int ata_wait_drq(void) {
    uint32_t i;
    for (i = 0; i < ATA_TIMEOUT; i++) {
        uint8_t st = inb(ATA_STATUS);
        if (st == 0xFF) return -1;
        if (st & (ATA_SR_ERR | ATA_SR_DF)) return -1;
        if ((st & ATA_SR_BSY) == 0 && (st & ATA_SR_DRQ)) return 0;
    }
    return -1;
}

static void ata_select_lba(uint32_t lba, uint8_t count) {
    outb(ATA_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    ata_io_delay();
    outb(ATA_SECCOUNT, count);
    outb(ATA_LBA0, (uint8_t)lba);
    outb(ATA_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_LBA2, (uint8_t)(lba >> 16));
}

int ata_present(void) {
    return g_ata_present;
}

int ata_init(void) {
    int st;
    uint32_t i;

    g_ata_present = 0;

    outb(ATA_DRIVE, 0xA0);
    ata_io_delay();
    st = inb(ATA_STATUS);
    /* No controller / no drive: fail immediately so QEMU without -hda does not hang. */
    if (st == 0x00 || st == 0xFF) return -1;

    outb(ATA_SECCOUNT, 0);
    outb(ATA_LBA0, 0);
    outb(ATA_LBA1, 0);
    outb(ATA_LBA2, 0);
    outb(ATA_CMD, ATA_CMD_IDENTIFY);
    ata_io_delay();

    st = inb(ATA_STATUS);
    if (st == 0x00 || st == 0xFF) return -1;
    if (ata_wait_not_busy() < 0) return -1;
    if (ata_wait_drq() < 0) return -1;

    for (i = 0; i < 256; i++) {
        (void)ata_inw(ATA_DATA);
    }

    g_ata_present = 1;
    return 0;
}

int ata_read_sectors(uint32_t lba, uint32_t count, void* buf) {
    uint8_t* out = (uint8_t*)buf;
    uint32_t s;
    uint32_t w;

    if (!g_ata_present || !buf || count == 0 || count > 256) return -1;
    if (lba + count < lba) return -1;

    ata_select_lba(lba, (uint8_t)count);
    outb(ATA_CMD, ATA_CMD_READ_PIO);

    for (s = 0; s < count; s++) {
        if (ata_wait_drq() < 0) return -1;
        for (w = 0; w < 256; w++) {
            uint16_t word = ata_inw(ATA_DATA);
            out[0] = (uint8_t)word;
            out[1] = (uint8_t)(word >> 8);
            out += 2;
        }
        if (ata_wait_not_busy() < 0) return -1;
    }
    return 0;
}

int ata_write_sectors(uint32_t lba, uint32_t count, const void* buf) {
    const uint8_t* in = (const uint8_t*)buf;
    uint32_t s;
    uint32_t w;

    if (!g_ata_present || !buf || count == 0 || count > 256) return -1;
    if (lba + count < lba) return -1;

    ata_select_lba(lba, (uint8_t)count);
    outb(ATA_CMD, ATA_CMD_WRITE_PIO);

    for (s = 0; s < count; s++) {
        if (ata_wait_drq() < 0) return -1;
        for (w = 0; w < 256; w++) {
            uint16_t word = (uint16_t)in[0] | ((uint16_t)in[1] << 8);
            ata_outw(ATA_DATA, word);
            in += 2;
        }
        if (ata_wait_not_busy() < 0) return -1;
    }
    return 0;
}
