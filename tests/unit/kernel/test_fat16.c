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
    uint32_t tensor_data;
    make_volume();
    for (fat = 1U; fat <= 2U; fat++) put16(fat * 17U * 512U + 6U, 0xFFFFU);
    disk[root + 32U] = 'G'; disk[root + 33U] = 'P'; disk[root + 34U] = 'T'; disk[root + 35U] = '2';
    disk[root + 36U] = ' '; disk[root + 37U] = ' '; disk[root + 38U] = ' '; disk[root + 39U] = ' ';
    disk[root + 40U] = 'G'; disk[root + 41U] = 'G'; disk[root + 42U] = 'U'; disk[root + 43U] = 0x20U;
    put16(root + 32U + 26U, 3U);
    put32(root + 32U + 28U, 480U);
    put32(p, GPT2_GGUF_MAGIC); p += 4U;
    put32(p, GPT2_GGUF_VERSION); p += 4U;
    put64_at(p, 1U); p += 8U;
    put64_at(p, 2U); p += 8U;
    put_text_at(&p, "general.architecture"); put32(p, GPT2_GGUF_VALUE_STRING); p += 4U; put_text_at(&p, "gpt2");
    put_text_at(&p, "general.alignment"); put32(p, GPT2_GGUF_VALUE_UINT32); p += 4U; put32(p, 32U); p += 4U;
    put_text_at(&p, "output.weight"); put32(p, 2U); p += 4U; put64_at(p, GPT2_QK_K); p += 8U;
    put64_at(p, 2U); p += 8U;
    put32(p, GPT2_GGUF_TENSOR_Q4_K); p += 4U; put64_at(p, 0U); p += 8U;
    end = data + 512U + 480U;
    tensor_data = p;
    while ((tensor_data & 31U) != 0U) tensor_data++;
    while (p < end) disk[p++] = 0U;
    disk[tensor_data] = 0x11U;
    disk[tensor_data + GPT2_Q4_K_BLOCK_BYTES] = 0x77U;
}

static void test_loads_gpt2_from_fat16(void) {
    fat16_volume_t volume;
    gpt2_gguf_loaded_model_t model;
    gpt2_gguf_tensor_t tensor;
    uint8_t buffer[512];
    make_gguf_file();
    TEST_ASSERT_EQUAL(0, fat16_mount(&volume, read_sector, 0U));
    TEST_ASSERT_EQUAL(0, gpt2_gguf_load_fat16(&volume, "gpt2.ggu", buffer, sizeof(buffer), &model));
    {
        gpt2_gguf_forward_context_t context;
        uint8_t context_scratch[GPT2_Q4_K_BLOCK_BYTES];
        char context_name[40];
        TEST_ASSERT_EQUAL(-1, gpt2_gguf_forward_context_init(&model, 0U, 0U, 0U,
                                                               context_name, sizeof(context_name),
                                                               context_scratch, sizeof(context_scratch), &context));
        TEST_ASSERT_EQUAL(-1, gpt2_gguf_forward_context_init(&model, 0U, 256U, 0U,
                                                               context_name, sizeof(context_name),
                                                               0, sizeof(context_scratch), &context));
    }
    {
        float storage[2U * 3U * 2U * 4U] = {0.0f};
        float key[4] = {1.0f, 2.0f, 3.0f, 4.0f};
        float value[4] = {5.0f, 6.0f, 7.0f, 8.0f};
        float key_out[4] = {0.0f};
        float value_out[4] = {0.0f};
        gpt2_gguf_kv_cache_t cache;
        TEST_ASSERT_EQUAL(0, gpt2_gguf_kv_cache_init(storage, 48U, 2U, 3U, 4U, &cache));
        TEST_ASSERT_EQUAL(0, gpt2_gguf_kv_cache_put(&cache, 1U, 0U, key, value));
        key[0] = 9.0f; value[0] = 19.0f;
        TEST_ASSERT_EQUAL(0, gpt2_gguf_kv_cache_put(&cache, 1U, 1U, key, value));
        key[0] = 1.0f; value[0] = 5.0f;
        TEST_ASSERT_EQUAL(0, gpt2_gguf_kv_cache_put(&cache, 1U, 2U, key, value));
        TEST_ASSERT_EQUAL(3, (int)cache.count);
        TEST_ASSERT_EQUAL(0, gpt2_gguf_kv_cache_get(&cache, 1U, 2U, key_out, value_out));
        TEST_ASSERT_EQUAL(1, (int)key_out[0]);
        TEST_ASSERT_EQUAL(8, (int)value_out[3]);
        TEST_ASSERT_EQUAL(-9, gpt2_gguf_kv_cache_get(&cache, 2U, 0U, key_out, value_out));
        TEST_ASSERT_EQUAL(-6, gpt2_gguf_kv_cache_init(storage, 47U, 2U, 3U, 4U, &cache));
        TEST_ASSERT_EQUAL(-1, gpt2_gguf_kv_cache_put(&cache, 0U, 0U, 0, value));
        {
            float history_key[12] = {0.0f};
            float history_value[12] = {0.0f};
            uint32_t history_count = 0U;
            TEST_ASSERT_EQUAL(0, gpt2_gguf_kv_cache_copy_history(&cache, 1U, 0U, 3U,
                                                                  history_key, 12U, history_value, 12U,
                                                                  &history_count));
            TEST_ASSERT_EQUAL(3, (int)history_count);
            TEST_ASSERT_EQUAL(1, (int)history_key[0]);
            TEST_ASSERT_EQUAL(9, (int)history_key[4]);
            TEST_ASSERT_EQUAL(19, (int)history_value[4]);
            TEST_ASSERT_EQUAL(-9, gpt2_gguf_kv_cache_copy_history(&cache, 0U, 2U, 2U,
                                                                   history_key, 12U, history_value, 12U,
                                                                   &history_count));
            TEST_ASSERT_EQUAL(-6, gpt2_gguf_kv_cache_copy_history(&cache, 1U, 0U, 3U,
                                                                   history_key, 8U, history_value, 12U,
                                                                   &history_count));
            {
                float query[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                float key_scratch[4] = {0.0f};
                float scores[3] = {0.0f};
                TEST_ASSERT_EQUAL(0, gpt2_gguf_kv_cache_query_scores(&cache, 1U, 0U, 3U,
                                                                      query, key_scratch, 4U,
                                                                      scores, 3U, &history_count));
                TEST_ASSERT_EQUAL(10, (int)scores[0]);
                TEST_ASSERT_EQUAL(18, (int)scores[1]);
                TEST_ASSERT_EQUAL(10, (int)scores[2]);
                TEST_ASSERT_EQUAL(-6, gpt2_gguf_kv_cache_query_scores(&cache, 1U, 0U, 3U,
                                                                       query, key_scratch, 3U,
                                                                       scores, 3U, &history_count));
                TEST_ASSERT_EQUAL(-9, gpt2_gguf_kv_cache_query_scores(&cache, 1U, 2U, 2U,
                                                                       query, key_scratch, 4U,
                                                                       scores, 3U, &history_count));
                {
                    float weights[3] = {0.2f, 0.3f, 0.5f};
                    float output[4] = {99.0f, 99.0f, 99.0f, 99.0f};
                    TEST_ASSERT_EQUAL(0, gpt2_gguf_kv_cache_accumulate_values(&cache, 1U, 0U, 3U,
                                                                                weights, 3U, output, 4U,
                                                                                &history_count));
                    TEST_ASSERT_EQUAL(4, (int)history_count);
                    TEST_ASSERT_EQUAL(92, (int)(output[0] * 10.0f));
                    TEST_ASSERT_EQUAL(60, (int)(output[1] * 10.0f));
                    TEST_ASSERT_EQUAL(70, (int)(output[2] * 10.0f));
                    TEST_ASSERT_EQUAL(80, (int)(output[3] * 10.0f));
                    TEST_ASSERT_EQUAL(-6, gpt2_gguf_kv_cache_accumulate_values(&cache, 1U, 0U, 3U,
                                                                                 weights, 2U, output, 4U,
                                                                                 &history_count));
                    TEST_ASSERT_EQUAL(-6, gpt2_gguf_kv_cache_accumulate_values(&cache, 1U, 0U, 3U,
                                                                                 weights, 3U, output, 3U,
                                                                                 &history_count));
                    TEST_ASSERT_EQUAL(-9, gpt2_gguf_kv_cache_accumulate_values(&cache, 1U, 2U, 2U,
                                                                                 weights, 3U, output, 4U,
                                                                                 &history_count));
                }
                {
                    float scaled[2] = {4.0f, 0.0f};
                    float probabilities[3] = {0.0f, 1.0f, 2.0f};
                    uint32_t attention_count = 0U;
                    TEST_ASSERT_EQUAL(0, gpt2_gguf_attention_scale_scores(scaled, 2U, 4U));
                    TEST_ASSERT_EQUAL(199, (int)(scaled[0] * 100.0f));
                    TEST_ASSERT_EQUAL(0, (int)(scaled[1] * 100.0f));
                    TEST_ASSERT_EQUAL(-1, gpt2_gguf_attention_scale_scores(scaled, 2U, 0U));
                    TEST_ASSERT_EQUAL(0, gpt2_gguf_attention_softmax(probabilities, 3U, &attention_count));
                    TEST_ASSERT_EQUAL(3, (int)attention_count);
                    TEST_ASSERT_TRUE(probabilities[0] < probabilities[1]);
                    TEST_ASSERT_TRUE(probabilities[1] < probabilities[2]);
                    TEST_ASSERT_EQUAL(100, (int)((probabilities[0] + probabilities[1] + probabilities[2]) * 100.0f));
                    TEST_ASSERT_EQUAL(0, gpt2_gguf_attention_softmax(probabilities, 0U, &attention_count));
                    TEST_ASSERT_EQUAL(0, (int)attention_count);
                    TEST_ASSERT_EQUAL(-1, gpt2_gguf_attention_softmax(0, 3U, &attention_count));
                }
                {
                    float head_query[2] = {1.0f, 1.0f};
                    float head_key_scratch[2] = {0.0f};
                    float head_scores[3] = {0.0f};
                    float head_output[2] = {0.0f};
                    uint32_t attention_count = 0U;
                    TEST_ASSERT_EQUAL(0, gpt2_gguf_kv_cache_attention_head(&cache, 1U, 0U, 3U,
                                                                            head_query, 2U, 0U,
                                                                            head_key_scratch, 2U,
                                                                            head_scores, 3U,
                                                                            head_output, 2U,
                                                                            &attention_count));
                    TEST_ASSERT_EQUAL(2, (int)attention_count);
                    TEST_ASSERT_TRUE(head_output[0] > 15.0f);
                    TEST_ASSERT_TRUE(head_output[0] < 19.5f);
                    TEST_ASSERT_TRUE(head_output[1] > 5.9f);
                    TEST_ASSERT_TRUE(head_output[1] < 6.1f);
                    TEST_ASSERT_EQUAL(0, gpt2_gguf_kv_cache_attention_head(&cache, 1U, 0U, 3U,
                                                                            head_query, 2U, 1U,
                                                                            head_key_scratch, 2U,
                                                                            head_scores, 3U,
                                                                            head_output, 2U,
                                                                            &attention_count));
                    TEST_ASSERT_TRUE(head_output[0] > 6.9f);
                    TEST_ASSERT_TRUE(head_output[0] < 7.1f);
                    TEST_ASSERT_TRUE(head_output[1] > 7.9f);
                    TEST_ASSERT_TRUE(head_output[1] < 8.1f);
                    TEST_ASSERT_EQUAL(-9, gpt2_gguf_kv_cache_attention_head(&cache, 1U, 0U, 3U,
                                                                            head_query, 2U, 2U,
                                                                            head_key_scratch, 2U,
                                                                            head_scores, 3U,
                                                                            head_output, 2U,
                                                                            &attention_count));
                    TEST_ASSERT_EQUAL(-6, gpt2_gguf_kv_cache_attention_head(&cache, 1U, 0U, 3U,
                                                                            head_query, 2U, 0U,
                                                                            head_key_scratch, 1U,
                                                                            head_scores, 3U,
                                                                            head_output, 2U,
                                                                            &attention_count));
                }
                {
                    float heads[4] = {1.0f, 2.0f, 3.0f, 4.0f};
                    float concatenated[4] = {0.0f};
                    uint32_t concat_count = 0U;
                    TEST_ASSERT_EQUAL(0, gpt2_gguf_attention_concat_heads(heads, 2U, 2U,
                                                                          concatenated, 4U,
                                                                          &concat_count));
                    TEST_ASSERT_EQUAL(4, (int)concat_count);
                    TEST_ASSERT_EQUAL(1, (int)concatenated[0]);
                    TEST_ASSERT_EQUAL(4, (int)concatenated[3]);
                    TEST_ASSERT_EQUAL(-6, gpt2_gguf_attention_concat_heads(heads, 2U, 2U,
                                                                            concatenated, 3U,
                                                                            &concat_count));
                    TEST_ASSERT_EQUAL(-9, gpt2_gguf_attention_concat_heads(heads, 0U, 2U,
                                                                            concatenated, 4U,
                                                                            &concat_count));
                    {
                        float multi_query[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                        float multi_heads[4] = {0.0f};
                        float multi_output[4] = {0.0f};
                        float multi_key[2] = {0.0f};
                        float multi_scores[3] = {0.0f};
                        TEST_ASSERT_EQUAL(0, gpt2_gguf_kv_cache_attention_multi_head(
                            &cache, 1U, 0U, 3U, multi_query, 2U,
                            multi_heads, 4U, multi_key, 2U, multi_scores, 3U,
                            multi_output, 4U, &concat_count));
                        TEST_ASSERT_EQUAL(4, (int)concat_count);
                        TEST_ASSERT_TRUE(multi_output[0] > 15.0f);
                        TEST_ASSERT_TRUE(multi_output[2] > 6.9f);
                        TEST_ASSERT_EQUAL(-9, gpt2_gguf_kv_cache_attention_multi_head(
                            &cache, 1U, 0U, 3U, multi_query, 3U,
                            multi_heads, 4U, multi_key, 2U, multi_scores, 3U,
                            multi_output, 4U, &concat_count));
                        TEST_ASSERT_EQUAL(-6, gpt2_gguf_kv_cache_attention_multi_head(
                            &cache, 1U, 0U, 3U, multi_query, 2U,
                            multi_heads, 3U, multi_key, 2U, multi_scores, 3U,
                            multi_output, 4U, &concat_count));
                    }
                    {
                        float residual[4] = {1.0f, 2.0f, 3.0f, 4.0f};
                        float attention[4] = {0.5f, 1.5f, 2.5f, 3.5f};
                        TEST_ASSERT_EQUAL(0, gpt2_gguf_add_residual(residual, 4U, attention, 4U));
                        TEST_ASSERT_EQUAL(1, (int)residual[0]);
                        TEST_ASSERT_EQUAL(3, (int)residual[1]);
                        TEST_ASSERT_EQUAL(5, (int)residual[2]);
                        TEST_ASSERT_EQUAL(7, (int)residual[3]);
                        TEST_ASSERT_EQUAL(-6, gpt2_gguf_add_residual(residual, 3U, attention, 4U));
                        TEST_ASSERT_EQUAL(-1, gpt2_gguf_add_residual(0, 4U, attention, 4U));
                    }
                    {
                        float norm_input[4] = {1.0f, 2.0f, 3.0f, 4.0f};
                        float norm_gamma[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                        float norm_beta[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                        float norm_output[4] = {0.0f};
                        float gelu_output[3] = {0.0f};
                        TEST_ASSERT_EQUAL(0, gpt2_gguf_layernorm(norm_input, 4U, norm_gamma,
                                                                  norm_beta, 0.0001f,
                                                                  norm_output, 4U));
                        TEST_ASSERT_TRUE(norm_output[0] < -1.2f);
                        TEST_ASSERT_TRUE(norm_output[3] > 1.2f);
                        TEST_ASSERT_EQUAL(-6, gpt2_gguf_layernorm(norm_input, 4U, norm_gamma,
                                                                  norm_beta, 0.0f,
                                                                  norm_output, 4U));
                        TEST_ASSERT_EQUAL(0, gpt2_gguf_gelu((float[]){-1.0f, 0.0f, 1.0f}, 3U,
                                                            gelu_output, 3U));
                        TEST_ASSERT_TRUE(gelu_output[0] < 0.0f);
                        TEST_ASSERT_EQUAL(0, (int)gelu_output[1]);
                        TEST_ASSERT_TRUE(gelu_output[2] > 0.5f);
                        TEST_ASSERT_EQUAL(-6, gpt2_gguf_gelu(norm_input, 4U, gelu_output, 3U));
                    }
                    {
                        float mlp_input[256] = {0.0f};
                        float mlp_hidden[2] = {0.0f};
                        float mlp_output[256] = {0.0f};
                        uint8_t mlp_row[2U * GPT2_Q6_K_BLOCK_BYTES];
                        float block_gamma[256] = {0.0f};
                        float block_beta[256] = {0.0f};
                        float block_norm[256] = {0.0f};
                        TEST_ASSERT_EQUAL(-9, gpt2_gguf_mlp_forward_fat16(
                            &volume, "gpt2.ggu", &model, &tensor, &tensor,
                            GPT2_QK_K, 2U, mlp_input, mlp_row, sizeof(mlp_row), 0, 0,
                            mlp_hidden, 2U, mlp_output, GPT2_QK_K));
                        TEST_ASSERT_EQUAL(-6, gpt2_gguf_mlp_forward_fat16(
                            &volume, "gpt2.ggu", &model, &tensor, &tensor,
                            GPT2_QK_K, 2U, mlp_input, mlp_row, sizeof(mlp_row), 0, 0,
                            mlp_hidden, 1U, mlp_output, GPT2_QK_K));
                        TEST_ASSERT_EQUAL(-6, gpt2_gguf_mlp_forward_add_residual_fat16(
                            &volume, "gpt2.ggu", &model, &tensor, &tensor,
                            GPT2_QK_K, 2U, mlp_input, mlp_row, sizeof(mlp_row), 0, 0,
                            mlp_hidden, 2U, mlp_output, GPT2_QK_K - 1U));
                        TEST_ASSERT_EQUAL(-1, gpt2_gguf_mlp_forward_add_residual_fat16(
                            &volume, "gpt2.ggu", &model, &tensor, &tensor,
                            GPT2_QK_K, 2U, mlp_input, mlp_row, sizeof(mlp_row), 0, 0,
                            mlp_hidden, 2U, 0, GPT2_QK_K));
                        TEST_ASSERT_EQUAL(-6, gpt2_gguf_block_mlp_forward_fat16(
                            &volume, "gpt2.ggu", &model, &tensor, &tensor,
                            GPT2_QK_K, 2U, mlp_input, block_gamma, block_beta, 0.0001f,
                            block_norm, GPT2_QK_K - 1U, mlp_row, sizeof(mlp_row),
                            0, 0, mlp_hidden, 2U, mlp_output, GPT2_QK_K));
                        TEST_ASSERT_EQUAL(-1, gpt2_gguf_block_mlp_forward_fat16(
                            &volume, "gpt2.ggu", &model, &tensor, &tensor,
                            GPT2_QK_K, 2U, mlp_input, block_gamma, block_beta, 0.0001f,
                            block_norm, GPT2_QK_K, mlp_row, sizeof(mlp_row),
                            0, 0, mlp_hidden, 2U, 0, GPT2_QK_K));
                        TEST_ASSERT_EQUAL(-9, gpt2_gguf_attention_output_add_residual_fat16(
                            &volume, "gpt2.ggu", &model, &tensor, GPT2_QK_K,
                            mlp_input, mlp_row, sizeof(mlp_row), mlp_output,
                            GPT2_QK_K, 0, mlp_output, GPT2_QK_K));
                        TEST_ASSERT_EQUAL(-6, gpt2_gguf_attention_output_add_residual_fat16(
                            &volume, "gpt2.ggu", &model, &tensor, 2U,
                            mlp_input, mlp_row, sizeof(mlp_row), mlp_output,
                            2U, 0, mlp_output, 1U));
                        TEST_ASSERT_EQUAL(-1, gpt2_gguf_block_attention_forward_fat16(
                            0, 0U, 0U, 0U, 0, 0, 0, 0.0001f,
                            0, 0U, 0U, 0U, 0, 0U, 0, 0U,
                            0, 0U, 0, 0U, 0, 0, 0, 0, 0, 0U,
                            0, 0U, 0, 0, 0U));
                    }
                }
            }
        }
    }
    TEST_ASSERT_EQUAL(480, (int)model.bytes_loaded);
    TEST_ASSERT_EQUAL(1, model.index.tensor_count);
    TEST_ASSERT_EQUAL(0, gpt2_gguf_map_role(&model.index, GPT2_GGUF_ROLE_OUTPUT_WEIGHT, &tensor));
    TEST_ASSERT_EQUAL(2, (int)tensor.dimensions);
    TEST_ASSERT_EQUAL(GPT2_GGUF_TENSOR_Q4_K, tensor.type);
    {
        uint8_t tensor_bytes[4] = {0xFFU, 0xFFU, 0xFFU, 0xFFU};
        uint32_t tensor_read = 0U;
        TEST_ASSERT_EQUAL(0, gpt2_gguf_read_tensor_fat16(&volume, "gpt2.ggu", &model, &tensor, 0U, tensor_bytes, sizeof(tensor_bytes), &tensor_read));
        TEST_ASSERT_EQUAL(4, (int)tensor_read);
        TEST_ASSERT_EQUAL(0x11, tensor_bytes[0]);
        TEST_ASSERT_EQUAL(0, tensor_bytes[1]);
        TEST_ASSERT_EQUAL(0, tensor_bytes[2]);
        TEST_ASSERT_EQUAL(0, tensor_bytes[3]);
    }
    {
        uint8_t block[GPT2_Q4_K_BLOCK_BYTES];
        uint32_t block_read = 0U;
        TEST_ASSERT_EQUAL(0, gpt2_gguf_read_quant_block_fat16(&volume, "gpt2.ggu", &model, &tensor, 0U, block, sizeof(block), &block_read));
        TEST_ASSERT_EQUAL(GPT2_Q4_K_BLOCK_BYTES, (int)block_read);
        TEST_ASSERT_EQUAL(0x11, block[0]);
        TEST_ASSERT_EQUAL(0, gpt2_gguf_read_quant_block_fat16(&volume, "gpt2.ggu", &model, &tensor, 1U, block, sizeof(block), &block_read));
        TEST_ASSERT_EQUAL(GPT2_Q4_K_BLOCK_BYTES, (int)block_read);
        TEST_ASSERT_EQUAL(0x77, block[0]);
        TEST_ASSERT_EQUAL(-6, gpt2_gguf_read_quant_block_fat16(&volume, "gpt2.ggu", &model, &tensor, 0U, block, 1U, &block_read));
        {
            uint8_t row[2U * GPT2_Q6_K_BLOCK_BYTES];
            uint32_t row_read = 0U;
            gpt2_gguf_tensor_t variant = tensor;
            TEST_ASSERT_EQUAL(0, gpt2_gguf_read_quant_row_fat16(&volume, "gpt2.ggu", &model, &tensor, 0U, row, sizeof(row), &row_read));
            TEST_ASSERT_EQUAL(GPT2_Q4_K_BLOCK_BYTES, (int)row_read);
            TEST_ASSERT_EQUAL(0x11, row[0]);
            TEST_ASSERT_EQUAL(0, gpt2_gguf_read_quant_row_fat16(&volume, "gpt2.ggu", &model, &tensor, 1U, row, sizeof(row), &row_read));
            TEST_ASSERT_EQUAL(0x77, row[0]);
            variant.type = GPT2_GGUF_TENSOR_Q3_K;
            variant.byte_size = 2U * GPT2_Q3_K_BLOCK_BYTES;
            TEST_ASSERT_EQUAL(0, gpt2_gguf_read_quant_row_fat16(&volume, "gpt2.ggu", &model, &variant, 0U, row, sizeof(row), &row_read));
            TEST_ASSERT_EQUAL(GPT2_Q3_K_BLOCK_BYTES, (int)row_read);
            variant.type = GPT2_GGUF_TENSOR_Q6_K;
            variant.byte_size = 2U * GPT2_Q6_K_BLOCK_BYTES;
            TEST_ASSERT_EQUAL(0, gpt2_gguf_read_quant_row_fat16(&volume, "gpt2.ggu", &model, &variant, 0U, row, sizeof(row), &row_read));
            TEST_ASSERT_EQUAL(GPT2_Q6_K_BLOCK_BYTES, (int)row_read);
            TEST_ASSERT_EQUAL(-6, gpt2_gguf_read_quant_row_fat16(&volume, "gpt2.ggu", &model, &tensor, 0U, row, 1U, &row_read));
            {
                float input[GPT2_QK_K] = {0.0f};
                float dot = 1.0f;
                TEST_ASSERT_EQUAL(0, gpt2_gguf_dot_quant_row_buffer(&tensor, row, sizeof(row), input, GPT2_QK_K, &dot));
                TEST_ASSERT_EQUAL(0, (int)dot);
                variant.type = GPT2_GGUF_TENSOR_Q3_K;
                TEST_ASSERT_EQUAL(0, gpt2_gguf_dot_quant_row_buffer(&variant, row, sizeof(row), input, GPT2_QK_K, &dot));
                variant.type = GPT2_GGUF_TENSOR_Q6_K;
                TEST_ASSERT_EQUAL(0, gpt2_gguf_dot_quant_row_buffer(&variant, row, sizeof(row), input, GPT2_QK_K, &dot));
                TEST_ASSERT_EQUAL(-6, gpt2_gguf_dot_quant_row_buffer(&tensor, row, 1U, input, GPT2_QK_K, &dot));
                TEST_ASSERT_EQUAL(-7, gpt2_gguf_dot_quant_row_buffer(&tensor, row, sizeof(row), input, 32U, &dot));
                {
                    float projected[2] = {99.0f, 99.0f};
                    TEST_ASSERT_EQUAL(0, gpt2_gguf_project_matrix_fat16(&volume, "gpt2.ggu", &model,
                                                                        &tensor, GPT2_QK_K, 2U,
                                                                        input, row, sizeof(row),
                                                                        projected, 2U * sizeof(float)));
                    TEST_ASSERT_EQUAL(0, (int)projected[0]);
                    TEST_ASSERT_EQUAL(0, (int)projected[1]);
                    TEST_ASSERT_EQUAL(-6, gpt2_gguf_project_matrix_fat16(&volume, "gpt2.ggu", &model,
                                                                         &tensor, GPT2_QK_K, 2U,
                                                                         input, row, sizeof(row),
                                                                         projected, sizeof(float)));
                    TEST_ASSERT_EQUAL(-9, gpt2_gguf_project_matrix_fat16(&volume, "gpt2.ggu", &model,
                                                                         &tensor, 32U, 2U,
                                                                         input, row, sizeof(row),
                                                                         projected, 2U * sizeof(float)));
                }
            {
                gpt2_gguf_tensor_t qkv = tensor;
                qkv.shape[1] = 3U * GPT2_QK_K;
                qkv.byte_size = 3U * GPT2_QK_K * GPT2_Q4_K_BLOCK_BYTES;
                TEST_ASSERT_EQUAL(0, gpt2_gguf_project_qkv_row_fat16(&volume, "gpt2.ggu", &model, &qkv,
                                                                       GPT2_QK_K, 0U, input, row, sizeof(row), &dot));
                TEST_ASSERT_EQUAL(0, (int)dot);
                TEST_ASSERT_EQUAL(-9, gpt2_gguf_project_qkv_row_fat16(&volume, "gpt2.ggu", &model, &qkv,
                                                                        GPT2_QK_K, 3U * GPT2_QK_K, input, row, sizeof(row), &dot));
                qkv.shape[1] = 2U * GPT2_QK_K;
                TEST_ASSERT_EQUAL(-9, gpt2_gguf_project_qkv_row_fat16(&volume, "gpt2.ggu", &model, &qkv,
                                                                        GPT2_QK_K, 0U, input, row, sizeof(row), &dot));
                {
                    float query[GPT2_QK_K] = {0.0f};
                    float key[GPT2_QK_K] = {0.0f};
                    float value[GPT2_QK_K] = {0.0f};
                    TEST_ASSERT_EQUAL(-6, gpt2_gguf_project_qkv_fat16(&volume, "gpt2.ggu", &model, &qkv,
                                                                       GPT2_QK_K, input, row, sizeof(row),
                                                                       query, sizeof(query) - 1U, key, sizeof(key), value, sizeof(value)));
                    TEST_ASSERT_EQUAL(-6, gpt2_gguf_project_qkv_fat16(&volume, "gpt2.ggu", &model, &qkv,
                                                                       GPT2_QK_K, input, row, sizeof(row),
                                                                       query, sizeof(query), key, sizeof(key), 0, sizeof(value)));
                }
            }
            }
        }
        {
            float input[GPT2_QK_K] = {0.0f};
            float dot = 1.0f;
            TEST_ASSERT_EQUAL(0, gpt2_gguf_dot_quant_block_fat16(&volume, "gpt2.ggu", &model, &tensor, 0U, input, GPT2_QK_K, block, sizeof(block), &dot));
            TEST_ASSERT_EQUAL(0, (int)dot);
            TEST_ASSERT_EQUAL(-7, gpt2_gguf_dot_quant_block_fat16(&volume, "gpt2.ggu", &model, &tensor, 0U, input, 32U, block, sizeof(block), &dot));
            {
                float row[GPT2_QK_K] = {0.0f};
                float tensor_input[GPT2_QK_K * 2U] = {0.0f};
                TEST_ASSERT_EQUAL(0, gpt2_gguf_dot_quant_tensor_fat16(&volume, "gpt2.ggu", &model, &tensor, tensor_input, GPT2_QK_K * 2U, block, sizeof(block), &dot));
                TEST_ASSERT_EQUAL(0, (int)dot);
                TEST_ASSERT_EQUAL(0, gpt2_gguf_dot_quant_row_fat16(&volume, "gpt2.ggu", &model, &tensor, 0U, row, GPT2_QK_K, block, sizeof(block), &dot));
                TEST_ASSERT_EQUAL(0, (int)dot);
                TEST_ASSERT_EQUAL(0, gpt2_gguf_dot_quant_row_fat16(&volume, "gpt2.ggu", &model, &tensor, 1U, row, GPT2_QK_K, block, sizeof(block), &dot));
                TEST_ASSERT_EQUAL(0, (int)dot);
                TEST_ASSERT_EQUAL(-9, gpt2_gguf_dot_quant_row_fat16(&volume, "gpt2.ggu", &model, &tensor, 2U, row, GPT2_QK_K, block, sizeof(block), &dot));
            }
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
