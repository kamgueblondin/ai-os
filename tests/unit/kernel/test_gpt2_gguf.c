/* test_gpt2_gguf.c - bounded structural parsing for GGUF v3. */

#include "../../framework/unity.h"
#include "../../../kernel/llm/gpt2_gguf.h"

#define BUF_SIZE 512U

static uint8_t blob[BUF_SIZE];
static uint32_t cursor;

static void reset_blob(void) {
    uint32_t i;
    for (i = 0U; i < BUF_SIZE; i++) blob[i] = 0U;
    cursor = 0U;
}

static void put_u32(uint32_t value) {
    blob[cursor++] = (uint8_t)(value & 0xffU);
    blob[cursor++] = (uint8_t)((value >> 8) & 0xffU);
    blob[cursor++] = (uint8_t)((value >> 16) & 0xffU);
    blob[cursor++] = (uint8_t)((value >> 24) & 0xffU);
}

static void put_u64(uint64_t value) {
    put_u32((uint32_t)value);
    put_u32((uint32_t)(value >> 32));
}

static void put_text(const char* text) {
    uint32_t count = 0U;
    while (text[count]) count++;
    put_u64(count);
    while (*text) blob[cursor++] = (uint8_t)*text++;
}

static void put_metadata_string(const char* key, const char* value) {
    put_text(key);
    put_u32(GPT2_GGUF_VALUE_STRING);
    put_text(value);
}

static void put_metadata_u32(const char* key, uint32_t value) {
    put_text(key);
    put_u32(GPT2_GGUF_VALUE_UINT32);
    put_u32(value);
}

static void put_tensor(const char* name, uint32_t dimensions, uint64_t first,
                       uint64_t second, uint32_t type, uint64_t offset) {
    put_text(name);
    put_u32(dimensions);
    put_u64(first);
    if (dimensions > 1U) put_u64(second);
    put_u32(type);
    put_u64(offset);
}

static uint32_t make_valid_gpt2(uint32_t type, uint64_t data_offset) {
    uint32_t padded;
    reset_blob();
    put_u32(GPT2_GGUF_MAGIC);
    put_u32(GPT2_GGUF_VERSION);
    put_u64(1U);
    put_u64(2U);
    put_metadata_string("general.architecture", "gpt2");
    put_metadata_u32("general.alignment", 32U);
    put_tensor("token_embd.weight", 2U, 768U, 50257U, type, data_offset);
    padded = (cursor + 31U) & ~31U;
    while (cursor < padded) blob[cursor++] = 0U;
    return cursor;
}

static void test_accepts_gpt2_q8_0_envelope(void) {
    gpt2_gguf_info_t info;
    uint32_t size = make_valid_gpt2(GPT2_GGUF_TENSOR_Q8_0, 0U);
    TEST_ASSERT_EQUAL(0, gpt2_gguf_probe_blob(blob, size, &info));
    TEST_ASSERT_EQUAL(1, info.is_valid);
    TEST_ASSERT_EQUAL(1, info.is_gpt2);
    TEST_ASSERT_EQUAL(3, info.version);
    TEST_ASSERT_EQUAL(1, info.tensor_count);
    TEST_ASSERT_EQUAL(1, info.q8_0_tensors);
    TEST_ASSERT_EQUAL(0, info.unsupported_quantized_tensors);
    TEST_ASSERT_EQUAL(32, info.alignment);
}

static void test_reports_supported_k_quantized_tensors(void) {
    gpt2_gguf_info_t info;
    uint32_t size;
    size = make_valid_gpt2(GPT2_GGUF_TENSOR_Q3_K, 0U);
    TEST_ASSERT_EQUAL(0, gpt2_gguf_probe_blob(blob, size, &info));
    TEST_ASSERT_EQUAL(1, info.q3_k_tensors);
    TEST_ASSERT_EQUAL(0, info.unsupported_quantized_tensors);
    size = make_valid_gpt2(GPT2_GGUF_TENSOR_Q4_K, 0U);
    TEST_ASSERT_EQUAL(0, gpt2_gguf_probe_blob(blob, size, &info));
    TEST_ASSERT_EQUAL(1, info.q4_k_tensors);
    TEST_ASSERT_EQUAL(0, info.unsupported_quantized_tensors);
    size = make_valid_gpt2(GPT2_GGUF_TENSOR_Q6_K, 0U);
    TEST_ASSERT_EQUAL(0, gpt2_gguf_probe_blob(blob, size, &info));
    TEST_ASSERT_EQUAL(1, info.q6_k_tensors);
    TEST_ASSERT_EQUAL(0, info.unsupported_quantized_tensors);
}

static void test_rejects_non_gpt2_architecture(void) {
    gpt2_gguf_info_t info;
    uint32_t size;
    reset_blob();
    put_u32(GPT2_GGUF_MAGIC);
    put_u32(GPT2_GGUF_VERSION);
    put_u64(0U);
    put_u64(1U);
    put_metadata_string("general.architecture", "llama");
    size = cursor;
    TEST_ASSERT_EQUAL(-4, gpt2_gguf_probe_blob(blob, size, &info));
}

static void test_rejects_unaligned_tensor_offset(void) {
    gpt2_gguf_info_t info;
    uint32_t size = make_valid_gpt2(GPT2_GGUF_TENSOR_Q8_0, 1U);
    TEST_ASSERT_EQUAL(-9, gpt2_gguf_probe_blob(blob, size, &info));
}

static void test_rejects_bad_magic_and_truncation(void) {
    gpt2_gguf_info_t info;
    uint32_t size = make_valid_gpt2(GPT2_GGUF_TENSOR_Q8_0, 0U);
    blob[0] = 0U;
    TEST_ASSERT_EQUAL(-2, gpt2_gguf_probe_blob(blob, size, &info));
    TEST_ASSERT_TRUE(gpt2_gguf_probe_blob(blob, 3U, &info) != 0);
}

int main(void) {
    unity_init();
    RUN_TEST(test_accepts_gpt2_q8_0_envelope);
    RUN_TEST(test_reports_supported_k_quantized_tensors);
    RUN_TEST(test_rejects_non_gpt2_architecture);
    RUN_TEST(test_rejects_unaligned_tensor_offset);
    RUN_TEST(test_rejects_bad_magic_and_truncation);
    unity_print_results();
    unity_cleanup();
    return unity_stats.tests_failed == 0 ? 0 : 1;
}
