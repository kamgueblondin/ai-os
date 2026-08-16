#include "../../framework/unity.h"
#include "../../../kernel/fs/fat16.h"

#define TEST_SECTORS 4224U
static uint8_t disk[TEST_SECTORS * 512U];

static uint16_t le16(uint32_t off) {
    return (uint16_t)disk[off] | ((uint16_t)disk[off + 1U] << 8);
}

static void put16(uint32_t off, uint16_t value) {
    disk[off] = (uint8_t)value;
    disk[off + 1U] = (uint8_t)(value >> 8);
}

static void put32(uint32_t off, uint32_t value) {
    put16(off, (uint16_t)value);
    put16(off + 2U, (uint16_t)(value >> 16));
}

static int read_sector(uint32_t lba, void* out) {
    uint32_t i;
    if (!out || lba >= TEST_SECTORS) return -1;
    for (i = 0U; i < 512U; i++) ((uint8_t*)out)[i] = disk[lba * 512U + i];
    return 0;
}

static void make_volume(void) {
    uint32_t fat;
    uint32_t root = (1U + 2U * 17U) * 512U;
    uint32_t data = (root / 512U + 2U) * 512U;
    uint32_t i;
    for (i = 0U; i < sizeof(disk); i++) disk[i] = 0U;
    put16(11U, 512U);
    disk[13] = 1U;
    put16(14U, 1U);
    disk[16] = 2U;
    put16(17U, 32U);
    put16(19U, TEST_SECTORS);
    disk[21] = 0xF8U;
    put16(22U, 17U);
    put16(510U, 0xAA55U);
    for (fat = 1U; fat <= 2U; fat++) {
        uint32_t base = fat * 17U * 512U;
        put16(base, 0xFFF8U);
        put16(base + 2U, 0xFFFFU);
        put16(base + 4U, 0xFFFFU);
    }
    disk[root + 0U] = 'F'; disk[root + 1U] = 'A'; disk[root + 2U] = 'T';
    disk[root + 3U] = 'O'; disk[root + 4U] = 'K'; disk[root + 5U] = ' '; disk[root + 6U] = ' '; disk[root + 7U] = ' ';
    disk[root + 8U] = 'T'; disk[root + 9U] = 'X'; disk[root + 10U] = 'T';
    disk[root + 11U] = 0x20U;
    put16(root + 26U, 2U);
    put32(root + 28U, 5U);
    disk[data + 0U] = 'h'; disk[data + 1U] = 'e'; disk[data + 2U] = 'l'; disk[data + 3U] = 'l'; disk[data + 4U] = 'o';
}

static void test_mount_list_and_read(void) {
    fat16_volume_t volume;
    os_fat16_dirent_t entries[4];
    char content[8] = {0};
    make_volume();
    TEST_ASSERT_EQUAL(0, fat16_mount(&volume, read_sector, 0U));
    TEST_ASSERT_TRUE(fat16_is_mounted(&volume));
    TEST_ASSERT_EQUAL(1, fat16_list_root(&volume, entries, 4U));
    TEST_ASSERT_EQUAL_STRING("FATOK.TXT", entries[0].name);
    TEST_ASSERT_EQUAL(5, (int)entries[0].size);
    TEST_ASSERT_EQUAL(5, fat16_read_file(&volume, "fatok.txt", content, sizeof(content)));
    TEST_ASSERT_EQUAL_STRING("hello", content);
}

static void test_rejects_bad_bpb(void) {
    fat16_volume_t volume;
    make_volume();
    put16(11U, 256U);
    TEST_ASSERT_EQUAL(OS_FAT16_CORRUPT, fat16_mount(&volume, read_sector, 0U));
    TEST_ASSERT_FALSE(fat16_is_mounted(&volume));
}

static void test_rejects_bad_name_and_small_buffer(void) {
    fat16_volume_t volume;
    char content[4];
    make_volume();
    TEST_ASSERT_EQUAL(0, fat16_mount(&volume, read_sector, 0U));
    TEST_ASSERT_EQUAL(OS_FAT16_BAD_PATH, fat16_read_file(&volume, "../x", content, sizeof(content)));
    TEST_ASSERT_EQUAL(OS_FAT16_BUFFER_SMALL, fat16_read_file(&volume, "FATOK.TXT", content, sizeof(content)));
}

int main(void) {
    unity_init();
    RUN_TEST(test_mount_list_and_read);
    RUN_TEST(test_rejects_bad_bpb);
    RUN_TEST(test_rejects_bad_name_and_small_buffer);
    unity_print_results();
    unity_cleanup();
    return 0;
}
