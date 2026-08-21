#include "../../framework/unity.h"
#include "../../../kernel/fs/fat32.h"

static uint8_t disk[2048U * 512U];
static int read_sector(uint32_t lba, void* buffer) {
    if (lba >= 2048U || !buffer) return -1;
    for (uint32_t i = 0U; i < 512U; i++) ((uint8_t*)buffer)[i] = disk[lba * 512U + i];
    return 0;
}
static int write_sector(uint32_t lba, const void* buffer) {
    if (lba >= 2048U || !buffer) return -1;
    for (uint32_t i = 0U; i < 512U; i++) disk[lba * 512U + i] = ((const uint8_t*)buffer)[i];
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
    TEST_ASSERT_EQUAL(0, fat32_attach_writer(&volume, write_sector));
    TEST_ASSERT_EQUAL(0, fat32_allocate_cluster(&volume, &next)); TEST_ASSERT_EQUAL(3U, next);
    TEST_ASSERT_EQUAL(0, fat32_read_fat_entry(&volume, 3U, &next)); TEST_ASSERT_EQUAL(FAT32_EOC_MIN, next);
    TEST_ASSERT_EQUAL(0, fat32_link_clusters(&volume, 2U, 3U));
    TEST_ASSERT_EQUAL(0, fat32_read_fat_entry(&volume, 2U, &next)); TEST_ASSERT_EQUAL(3U, next);
    { uint8_t data[1024U]; for (uint32_t i = 0U; i < sizeof(data); i++) data[i] = (uint8_t)(i & 0xffU);
      TEST_ASSERT_EQUAL(0, fat32_write_cluster(&volume, 3U, data));
      TEST_ASSERT_EQUAL(0, fat32_create_root_entry(&volume, "SESSION.BIN", 0x20U, 3U, 1024U));
      TEST_ASSERT_EQUAL('S', disk[2032U * 512U]); TEST_ASSERT_EQUAL('E', disk[2032U * 512U + 1U]);
      TEST_ASSERT_EQUAL('B', disk[2032U * 512U + 8U]); TEST_ASSERT_EQUAL('N', disk[2032U * 512U + 10U]);
      TEST_ASSERT_EQUAL(0x20U, disk[2032U * 512U + 11U]); TEST_ASSERT_EQUAL(0x00U, disk[2032U * 512U + 20U]);
      TEST_ASSERT_EQUAL(3U, disk[2032U * 512U + 26U]); TEST_ASSERT_EQUAL(0U, disk[2032U * 512U + 28U]); TEST_ASSERT_EQUAL(4U, disk[2032U * 512U + 29U]);
      { uint8_t file_data[1024U]; uint32_t first; for (uint32_t i = 0U; i < sizeof(file_data); i++) file_data[i] = (uint8_t)(0xa0U + (i & 0x0fU));
        TEST_ASSERT_EQUAL(0, fat32_create_file(&volume, "CHAT.BIN", 0x20U, file_data, sizeof(file_data), &first));
        TEST_ASSERT_EQUAL(4U, first); TEST_ASSERT_EQUAL(file_data[0], disk[2036U * 512U]); TEST_ASSERT_EQUAL(file_data[511U], disk[2036U * 512U + 511U]);
        TEST_ASSERT_EQUAL(file_data[512U], disk[2037U * 512U]); TEST_ASSERT_EQUAL('C', disk[2032U * 512U + 32U]); }
    }
}

void test_fat32_extend_full_root(void) {
    fat32_volume_t volume; uint32_t next;
    for (uint32_t i = 0U; i < sizeof(disk); i++) disk[i] = 0U;
    disk[13] = 2U; put16(disk + 11U, 512U); put16(disk + 14U, 32U); disk[16] = 2U;
    put32(disk + 32U, 200000U); put32(disk + 36U, 1000U); put32(disk + 44U, 2U); put16(disk + 510U, 0xaa55U);
    for (uint32_t i = 0U; i < 1024U; i++) disk[2032U * 512U + i] = 0x41U;
    disk[32U * 512U + 8U] = 0xf8U; disk[32U * 512U + 9U] = 0xffU; disk[32U * 512U + 10U] = 0xffU; disk[32U * 512U + 11U] = 0x0fU;
    TEST_ASSERT_EQUAL(0, fat32_mount(&volume, read_sector, 0U)); TEST_ASSERT_EQUAL(0, fat32_attach_writer(&volume, write_sector));
    TEST_ASSERT_EQUAL(0, fat32_extend_root_directory(&volume, &next)); TEST_ASSERT_EQUAL(3U, next);
    TEST_ASSERT_EQUAL(3U, disk[32U * 512U + 8U]); TEST_ASSERT_EQUAL(0U, disk[2034U * 512U]);
}

void test_fat32_lfn_encoding(void) {
    uint8_t alias[11] = {'C','H','A','T',' ',' ',' ',' ','B','I','N'}, entry[32];
    TEST_ASSERT_EQUAL(0, fat32_encode_lfn_entry("Session-2026", 0x41U, fat32_lfn_checksum(alias), entry));
    TEST_ASSERT_EQUAL(0x41U, entry[0]); TEST_ASSERT_EQUAL(0x0fU, entry[11]); TEST_ASSERT_EQUAL(fat32_lfn_checksum(alias), entry[13]);
    TEST_ASSERT_EQUAL('S', entry[1]); TEST_ASSERT_EQUAL(0U, entry[2]); TEST_ASSERT_EQUAL('n', entry[16]); TEST_ASSERT_EQUAL(0U, entry[17]);
    TEST_ASSERT_EQUAL(0, fat32_encode_lfn_entry("abcdefghijklm", 1U, 0U, entry));
    TEST_ASSERT_EQUAL(OS_FAT16_BAD_PATH, fat32_encode_lfn_entry("abcdefghijklmn", 2U, 0U, entry));
}

void test_fat32_lfn_file_and_list(void) {
    fat32_volume_t volume; os_fat16_dirent_t entries[4]; uint8_t data[16]; uint32_t first;
    for (uint32_t i = 0U; i < sizeof(disk); i++) disk[i] = 0U;
    disk[13] = 2U; put16(disk + 11U, 512U); put16(disk + 14U, 32U); disk[16] = 2U;
    put32(disk + 32U, 200000U); put32(disk + 36U, 1000U); put32(disk + 44U, 2U); put16(disk + 510U, 0xaa55U);
    disk[32U * 512U + 8U] = 0xf8U; disk[32U * 512U + 9U] = 0xffU; disk[32U * 512U + 10U] = 0xffU; disk[32U * 512U + 11U] = 0x0fU;
    for (uint32_t i = 0U; i < sizeof(data); i++) data[i] = (uint8_t)i;
    TEST_ASSERT_EQUAL(0, fat32_mount(&volume, read_sector, 0U)); TEST_ASSERT_EQUAL(0, fat32_attach_writer(&volume, write_sector));
    { int rc = fat32_create_lfn_file(&volume, "Persistent-LLM-Session", "SESSION.BIN", 0x20U, data, sizeof(data), &first); TEST_ASSERT_EQUAL(0, rc); }
    TEST_ASSERT_EQUAL(3U, first); TEST_ASSERT_EQUAL(1, fat32_list_root(&volume, entries, 4U));
    TEST_ASSERT_EQUAL_STRING("Persistent-LLM-Session", entries[0].name); TEST_ASSERT_EQUAL(sizeof(data), entries[0].size);
    TEST_ASSERT_EQUAL(0, fat32_unlink_file(&volume, "persistent-llm-session"));
    TEST_ASSERT_EQUAL(0xe5U, disk[2032U * 512U]);
    TEST_ASSERT_EQUAL(0xe5U, disk[2032U * 512U + 32U]);
    TEST_ASSERT_EQUAL(0xe5U, disk[2032U * 512U + 64U]);
    TEST_ASSERT_EQUAL(0xe5U, disk[2032U * 512U + 96U]);
    TEST_ASSERT_EQUAL(0U, disk[32U * 512U + 12U]);
    TEST_ASSERT_EQUAL(0, fat32_list_root(&volume, entries, 4U));
    TEST_ASSERT_EQUAL(OS_FAT16_NOT_FOUND, fat32_unlink_file(&volume, "Persistent-LLM-Session"));
}

int main(void) { unity_init(); RUN_TEST(test_fat32_mount_and_read_cluster); RUN_TEST(test_fat32_extend_full_root); RUN_TEST(test_fat32_lfn_encoding); RUN_TEST(test_fat32_lfn_file_and_list); unity_print_results(); unity_cleanup(); return 0; }
