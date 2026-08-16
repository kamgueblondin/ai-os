#include "../../framework/unity.h"
#include "../../../kernel/fs/fat16.h"
#include "../../../kernel/llm/gpt2_gguf_loader.h"
#include "../../../kernel/llm/gpt2_quant.h"

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

static void put64_at(uint32_t off, uint64_t value) {
    put32(off, (uint32_t)value);
    put32(off + 4U, (uint32_t)(value >> 32));
}

static void put_text_at(uint32_t* cursor, const char* text) {
    uint32_t i = 0U;
    while (text[i]) i++;
    put64_at(*cursor, i);
    *cursor += 8U;
    while (*text) disk[(*cursor)++] = (uint8_t)*text++;
}

static void make_gguf_file(void) {
    uint32_t root = (1U + 2U * 17U) * 512U;
    uint32_t data = (root / 512U + 2U) * 512U;
    uint32_t fat;
    uint32_t p = data + 512U;
    uint32_t end;
    make_volume();
    for (fat = 1U; fat <= 2U; fat++) put16(fat * 17U * 512U + 6U, 0xFFFFU);
    disk[root + 32U] = 'G'; disk[root + 33U] = 'P'; disk[root + 34U] = 'T'; disk[root + 35U] = '2';
    disk[root + 36U] = ' '; disk[root + 37U] = ' '; disk[root + 38U] = ' '; disk[root + 39U] = ' ';
    disk[root + 40U] = 'G'; disk[root + 41U] = 'G'; disk[root + 42U] = 'U'; disk[root + 43U] = 0x20U;
    put16(root + 32U + 26U, 3U);
    put32(root + 32U + 28U, 320U);
    put32(p, GPT2_GGUF_MAGIC); p += 4U;
    put32(p, GPT2_GGUF_VERSION); p += 4U;
    put64_at(p, 1U); p += 8U;
    put64_at(p, 2U); p += 8U;
    put_text_at(&p, "general.architecture"); put32(p, GPT2_GGUF_VALUE_STRING); p += 4U; put_text_at(&p, "gpt2");
    put_text_at(&p, "general.alignment"); put32(p, GPT2_GGUF_VALUE_UINT32); p += 4U; put32(p, 32U); p += 4U;
    put_text_at(&p, "output.weight"); put32(p, 1U); p += 4U; put64_at(p, GPT2_QK_K); p += 8U;
    put32(p, GPT2_GGUF_TENSOR_Q4_K); p += 4U; put64_at(p, 0U); p += 8U;
    end = data + 512U + 320U;
    while (p < end) disk[p++] = 0U;
}

static void test_loads_gpt2_from_fat16(void) {
    fat16_volume_t volume;
    gpt2_gguf_loaded_model_t model;
    gpt2_gguf_tensor_t tensor;
    uint8_t buffer[512];
    make_gguf_file();
    TEST_ASSERT_EQUAL(0, fat16_mount(&volume, read_sector, 0U));
    TEST_ASSERT_EQUAL(0, gpt2_gguf_load_fat16(&volume, "gpt2.ggu", buffer, sizeof(buffer), &model));
    TEST_ASSERT_EQUAL(320, (int)model.bytes_loaded);
    TEST_ASSERT_EQUAL(1, model.index.tensor_count);
    TEST_ASSERT_EQUAL(0, gpt2_gguf_map_role(&model.index, GPT2_GGUF_ROLE_OUTPUT_WEIGHT, &tensor));
    TEST_ASSERT_EQUAL(GPT2_GGUF_TENSOR_Q4_K, tensor.type);
    {
        uint8_t tensor_bytes[4] = {0xFFU, 0xFFU, 0xFFU, 0xFFU};
        uint32_t tensor_read = 0U;
        TEST_ASSERT_EQUAL(0, gpt2_gguf_read_tensor_fat16(&volume, "gpt2.ggu", &model, &tensor, 0U, tensor_bytes, sizeof(tensor_bytes), &tensor_read));
        TEST_ASSERT_EQUAL(4, (int)tensor_read);
        TEST_ASSERT_EQUAL(0, tensor_bytes[0]);
        TEST_ASSERT_EQUAL(0, tensor_bytes[1]);
        TEST_ASSERT_EQUAL(0, tensor_bytes[2]);
        TEST_ASSERT_EQUAL(0, tensor_bytes[3]);
    }
    {
        uint8_t block[GPT2_Q4_K_BLOCK_BYTES];
        uint32_t block_read = 0U;
        TEST_ASSERT_EQUAL(0, gpt2_gguf_read_quant_block_fat16(&volume, "gpt2.ggu", &model, &tensor, 0U, block, sizeof(block), &block_read));
        TEST_ASSERT_EQUAL(GPT2_Q4_K_BLOCK_BYTES, (int)block_read);
        TEST_ASSERT_EQUAL(-6, gpt2_gguf_read_quant_block_fat16(&volume, "gpt2.ggu", &model, &tensor, 0U, block, 1U, &block_read));
        {
            float input[GPT2_QK_K] = {0.0f};
            float dot = 1.0f;
            TEST_ASSERT_EQUAL(0, gpt2_gguf_dot_quant_block_fat16(&volume, "gpt2.ggu", &model, &tensor, 0U, input, GPT2_QK_K, block, sizeof(block), &dot));
            TEST_ASSERT_EQUAL(0, (int)dot);
            TEST_ASSERT_EQUAL(-7, gpt2_gguf_dot_quant_block_fat16(&volume, "gpt2.ggu", &model, &tensor, 0U, input, 32U, block, sizeof(block), &dot));
        }
    }
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

static void test_reads_bounded_file_range(void) {
    fat16_volume_t volume;
    uint8_t content[4] = {0U, 0U, 0U, 0U};
    uint32_t read = 0U;
    make_volume();
    TEST_ASSERT_EQUAL(0, fat16_mount(&volume, read_sector, 0U));
    TEST_ASSERT_EQUAL(0, fat16_read_file_range(&volume, "fatok.txt", 1U, content, 3U, &read));
    TEST_ASSERT_EQUAL(3, (int)read);
    TEST_ASSERT_EQUAL('e', content[0]);
    TEST_ASSERT_EQUAL('l', content[1]);
    TEST_ASSERT_EQUAL('l', content[2]);
    TEST_ASSERT_EQUAL(OS_FAT16_BAD_PATH, fat16_read_file_range(&volume, "fatok.txt", 6U, content, 1U, &read));
    TEST_ASSERT_EQUAL(0, (int)read);
}

static void test_cursor_reads_successive_windows(void) {
    fat16_volume_t volume;
    fat16_file_t file;
    uint8_t first[3] = {0U, 0U, 0U};
    uint8_t second[3] = {0U, 0U, 0U};
    uint32_t read = 0U;
    make_volume();
    TEST_ASSERT_EQUAL(0, fat16_mount(&volume, read_sector, 0U));
    TEST_ASSERT_EQUAL(0, fat16_open_file(&volume, "fatok.txt", &file));
    TEST_ASSERT_EQUAL(0, fat16_file_read(&file, first, sizeof(first), &read));
    TEST_ASSERT_EQUAL(3, (int)read);
    TEST_ASSERT_EQUAL('h', first[0]);
    TEST_ASSERT_EQUAL('e', first[1]);
    TEST_ASSERT_EQUAL('l', first[2]);
    TEST_ASSERT_EQUAL(0, fat16_file_read(&file, second, sizeof(second), &read));
    TEST_ASSERT_EQUAL(2, (int)read);
    TEST_ASSERT_EQUAL('l', second[0]);
    TEST_ASSERT_EQUAL('o', second[1]);
    TEST_ASSERT_EQUAL(0, fat16_file_read(&file, second, sizeof(second), &read));
    TEST_ASSERT_EQUAL(0, (int)read);
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
    RUN_TEST(test_loads_gpt2_from_fat16);
    RUN_TEST(test_reads_bounded_file_range);
    RUN_TEST(test_cursor_reads_successive_windows);
    RUN_TEST(test_rejects_bad_bpb);
    RUN_TEST(test_rejects_bad_name_and_small_buffer);
    unity_print_results();
    unity_cleanup();
    return 0;
}
