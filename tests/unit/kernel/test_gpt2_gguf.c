/* test_gpt2_gguf.c - bounded structural parsing for GGUF v3. */

#include "../../framework/unity.h"
#include "../../../kernel/llm/gpt2_gguf.h"
#include "../../../kernel/llm/gpt2_quant.h"

#define BUF_SIZE 4096U

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

static uint32_t make_small_tensor(uint32_t type, uint64_t data_offset) {
    uint32_t padded;
    uint32_t i;
    reset_blob();
    put_u32(GPT2_GGUF_MAGIC);
    put_u32(GPT2_GGUF_VERSION);
    put_u64(1U);
    put_u64(2U);
    put_metadata_string("general.architecture", "gpt2");
    put_metadata_u32("general.alignment", 32U);
    put_tensor("blk.0.weight", 1U, GPT2_QK_K, 0U, type, data_offset);
    padded = (cursor + 31U) & ~31U;
    while (cursor < padded) blob[cursor++] = 0U;
    for (i = 0U; i < GPT2_Q4_K_BLOCK_BYTES; i++) blob[cursor++] = 0U;
    return cursor;
}

static uint32_t make_role_tensors(void) {
    static const char* const names[] = {
        "token_embd.weight", "position_embd.weight", "output_norm.weight",
        "output_norm.bias", "output.weight",
        "blk.0.attn_norm.weight", "blk.0.attn_norm.bias",
        "blk.0.attn_qkv.weight", "blk.0.attn_qkv.bias",
        "blk.0.attn_output.weight", "blk.0.attn_output.bias",
        "blk.0.ffn_norm.weight", "blk.0.ffn_norm.bias",
        "blk.0.ffn_up.weight", "blk.0.ffn_down.weight"
    };
    uint32_t i;
    uint32_t padded;
    reset_blob();
    put_u32(GPT2_GGUF_MAGIC);
    put_u32(GPT2_GGUF_VERSION);
    put_u64(15U);
    put_u64(2U);
    put_metadata_string("general.architecture", "gpt2");
    put_metadata_u32("general.alignment", 32U);
    for (i = 0U; i < 15U; i++) {
        put_tensor(names[i], 1U, GPT2_QK_K, 0U, GPT2_GGUF_TENSOR_Q4_K, (uint64_t)(i * 160U));
    }
    padded = 4096U;
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

static void test_finds_bounded_q4_tensor(void) {
    gpt2_gguf_tensor_t tensor;
    uint32_t size = make_small_tensor(GPT2_GGUF_TENSOR_Q4_K, 0U);
    TEST_ASSERT_EQUAL(0, gpt2_gguf_find_tensor(blob, size, "blk.0.weight", &tensor));
    TEST_ASSERT_EQUAL(GPT2_GGUF_TENSOR_Q4_K, tensor.type);
    TEST_ASSERT_EQUAL(1, tensor.dimensions);
    TEST_ASSERT_EQUAL(GPT2_QK_K, (int)tensor.shape[0]);
    TEST_ASSERT_EQUAL(GPT2_Q4_K_BLOCK_BYTES, tensor.byte_size);
    TEST_ASSERT_EQUAL(0, tensor.data_offset);
    TEST_ASSERT_EQUAL(-8, gpt2_gguf_find_tensor(blob, size, "missing", &tensor));
    size = make_small_tensor(GPT2_GGUF_TENSOR_Q4_K, 32U);
    TEST_ASSERT_EQUAL(-7, gpt2_gguf_find_tensor(blob, size, "blk.0.weight", &tensor));
}

static void test_builds_index_and_maps_gpt2_roles(void) {
    gpt2_gguf_index_t index;
    gpt2_gguf_tensor_t tensor;
    uint32_t size = make_role_tensors();
    TEST_ASSERT_EQUAL(0, gpt2_gguf_build_index(blob, size, &index));
    char name[40];
    TEST_ASSERT_EQUAL(15, index.tensor_count);
    TEST_ASSERT_EQUAL(0, gpt2_gguf_index_find(&index, "output.weight", &tensor));
    TEST_ASSERT_EQUAL(GPT2_GGUF_TENSOR_Q4_K, tensor.type);
    TEST_ASSERT_EQUAL(4 * 160, (int)tensor.data_offset);
    TEST_ASSERT_EQUAL(0, gpt2_gguf_validate_tensor_size(&tensor));
    tensor.byte_size--;
    TEST_ASSERT_EQUAL(-9, gpt2_gguf_validate_tensor_size(&tensor));
    tensor.byte_size++;
    tensor.shape[0] = GPT2_QK_K - 1U;
    TEST_ASSERT_EQUAL(-9, gpt2_gguf_validate_tensor_size(&tensor));
    TEST_ASSERT_EQUAL(0, gpt2_gguf_map_role(&index, GPT2_GGUF_ROLE_TOKEN_EMBEDDING, &tensor));
    TEST_ASSERT_EQUAL(0, gpt2_gguf_map_role(&index, GPT2_GGUF_ROLE_POSITION_EMBEDDING, &tensor));
    TEST_ASSERT_EQUAL(0, gpt2_gguf_map_role(&index, GPT2_GGUF_ROLE_OUTPUT_NORM_WEIGHT, &tensor));
    TEST_ASSERT_EQUAL(0, gpt2_gguf_map_role(&index, GPT2_GGUF_ROLE_OUTPUT_NORM_BIAS, &tensor));
    TEST_ASSERT_EQUAL(0, gpt2_gguf_map_role(&index, GPT2_GGUF_ROLE_OUTPUT_WEIGHT, &tensor));
    TEST_ASSERT_EQUAL(-1, gpt2_gguf_map_role(&index, (gpt2_gguf_role_t)99, &tensor));
    TEST_ASSERT_EQUAL(0, gpt2_gguf_map_layer_role(&index, 0U, GPT2_GGUF_ROLE_LAYER_ATTN_NORM_WEIGHT, name, sizeof(name), &tensor));
    TEST_ASSERT_EQUAL_STRING("blk.0.attn_norm.weight", name);
    TEST_ASSERT_EQUAL(0, gpt2_gguf_map_layer_role(&index, 0U, GPT2_GGUF_ROLE_LAYER_ATTN_QKV_WEIGHT, name, sizeof(name), &tensor));
    TEST_ASSERT_EQUAL_STRING("blk.0.attn_qkv.weight", name);
    TEST_ASSERT_EQUAL(-8, gpt2_gguf_map_layer_role(&index, 1U, GPT2_GGUF_ROLE_LAYER_ATTN_NORM_WEIGHT, name, sizeof(name), &tensor));
    TEST_ASSERT_EQUAL(-2, gpt2_gguf_map_layer_role(&index, 0U, GPT2_GGUF_ROLE_LAYER_ATTN_QKV_WEIGHT, name, 8U, &tensor));
    {
        gpt2_gguf_layer_t layer;
        TEST_ASSERT_EQUAL(0, gpt2_gguf_map_layer(&index, 0U, name, sizeof(name), &layer));
        TEST_ASSERT_EQUAL(0x3FF, (int)layer.present_mask);
        TEST_ASSERT_EQUAL(0, (int)layer.layer_index);
        TEST_ASSERT_EQUAL(0, gpt2_gguf_layer_get(&layer, GPT2_GGUF_ROLE_LAYER_FFN_DOWN_WEIGHT, &tensor));
        TEST_ASSERT_EQUAL(-1, gpt2_gguf_layer_get(&layer, GPT2_GGUF_ROLE_OUTPUT_WEIGHT, &tensor));
        layer.tensors[0].dimensions = 1U; layer.tensors[0].shape[0] = GPT2_QK_K;
        layer.tensors[1].dimensions = 1U; layer.tensors[1].shape[0] = GPT2_QK_K;
        layer.tensors[2].dimensions = 2U; layer.tensors[2].shape[0] = GPT2_QK_K; layer.tensors[2].shape[1] = 3U * GPT2_QK_K;
        layer.tensors[3].dimensions = 1U; layer.tensors[3].shape[0] = 3U * GPT2_QK_K;
        layer.tensors[4].dimensions = 2U; layer.tensors[4].shape[0] = GPT2_QK_K; layer.tensors[4].shape[1] = GPT2_QK_K;
        layer.tensors[5].dimensions = 1U; layer.tensors[5].shape[0] = GPT2_QK_K;
        layer.tensors[6].dimensions = 1U; layer.tensors[6].shape[0] = GPT2_QK_K;
        layer.tensors[7].dimensions = 1U; layer.tensors[7].shape[0] = GPT2_QK_K;
        layer.tensors[8].dimensions = 2U; layer.tensors[8].shape[0] = GPT2_QK_K; layer.tensors[8].shape[1] = 4U * GPT2_QK_K;
        layer.tensors[9].dimensions = 2U; layer.tensors[9].shape[0] = 4U * GPT2_QK_K; layer.tensors[9].shape[1] = GPT2_QK_K;
        TEST_ASSERT_EQUAL(0, gpt2_gguf_validate_gpt2_layer(&layer, GPT2_QK_K));
        layer.tensors[2].shape[1] = 2U * GPT2_QK_K;
        TEST_ASSERT_EQUAL(-9, gpt2_gguf_validate_gpt2_layer(&layer, GPT2_QK_K));
        layer.tensors[2].shape[1] = 3U * GPT2_QK_K;
        TEST_ASSERT_EQUAL(0, gpt2_gguf_validate_layer(&layer, GPT2_QK_K));
        TEST_ASSERT_EQUAL(-9, gpt2_gguf_validate_layer(&layer, GPT2_QK_K - 1U));
        layer.tensors[0].shape[0] = GPT2_QK_K - 1U;
        TEST_ASSERT_EQUAL(-9, gpt2_gguf_validate_layer(&layer, GPT2_QK_K));
        layer.tensors[0].shape[0] = GPT2_QK_K;
        layer.present_mask = 0U;
        TEST_ASSERT_EQUAL(-8, gpt2_gguf_layer_get(&layer, GPT2_GGUF_ROLE_LAYER_FFN_DOWN_WEIGHT, &tensor));
        TEST_ASSERT_EQUAL(-8, gpt2_gguf_validate_layer(&layer, GPT2_QK_K));
        TEST_ASSERT_EQUAL(-8, gpt2_gguf_map_layer(&index, 1U, name, sizeof(name), &layer));
    }
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
    RUN_TEST(test_finds_bounded_q4_tensor);
    RUN_TEST(test_builds_index_and_maps_gpt2_roles);
    RUN_TEST(test_rejects_non_gpt2_architecture);
    RUN_TEST(test_rejects_unaligned_tensor_offset);
    RUN_TEST(test_rejects_bad_magic_and_truncation);
    unity_print_results();
    unity_cleanup();
    return unity_stats.tests_failed == 0 ? 0 : 1;
}
