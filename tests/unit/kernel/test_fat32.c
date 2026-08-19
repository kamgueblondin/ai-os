#include "../../framework/unity.h"
#include "../../../kernel/fs/fat32.h"

static uint8_t disk[2048U * 512U];
static int read_sector(uint32_t lba, void* buffer) {
    if (lba >= 2048U || !buffer) return -1;
    for (uint32_t i = 0U; i < 512U; i++) ((uint8_t*)buffer)[i] = disk[lba * 512U + i];
    return 0;
}
static void put16(uint8_t* p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8U); }
static void put32(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8U); p[2] = (uint8_t)(v >> 16U); p[3] = (uint8_t)(v >> 24U); }
void setUp(void) { for (uint32_t i = 0U; i < sizeof(disk); i++) disk[i] = 0U; }
void tearDown(void) {}

void test_fat32_mount_and_read_cluster(void) {
    fat32_volume_t volume; uint8_t cluster[1024]; uint32_t next, lba;
    disk[13] = 2U; put16(disk + 11U, 512U); put16(disk + 14U, 32U); disk[16] = 2U;
    put16(disk + 19U, 0U); put16(disk + 22U, 0U); put32(disk + 32U, 200000U); put32(disk + 36U, 1000U); put32(disk + 44U, 2U); put16(disk + 510U, 0xaa55U);
    TEST_ASSERT_EQUAL(0, fat32_mount(&volume, read_sector, 0U));
    TEST_ASSERT_TRUE(fat32_is_mounted(&volume)); TEST_ASSERT_EQUAL(2U, volume.root_cluster);
    TEST_ASSERT_EQUAL(0, fat32_cluster_lba(&volume, 2U, &lba)); TEST_ASSERT_EQUAL(2032U, lba);
    disk[32U * 512U + 8U] = 0xf8U; disk[32U * 512U + 9U] = 0xffU; disk[32U * 512U + 10U] = 0xffU; disk[32U * 512U + 11U] = 0x0fU;
    TEST_ASSERT_EQUAL(0, fat32_read_fat_entry(&volume, 2U, &next)); TEST_ASSERT_EQUAL(FAT32_EOC_MIN, next);
}

int main(void) { unity_init(); RUN_TEST(test_fat32_mount_and_read_cluster); unity_print_results(); unity_cleanup(); return 0; }
