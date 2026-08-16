#include "gpt2_gguf_loader.h"

int gpt2_gguf_load_fat16(const fat16_volume_t* volume, const char* filename,
                         uint8_t* buffer, uint32_t capacity,
                         gpt2_gguf_loaded_model_t* out) {
    int bytes;
    int status;
    if (!volume || !filename || !buffer || capacity == 0U || !out) return -1;
    if (!fat16_is_mounted(volume)) return OS_FAT16_NOT_MOUNTED;
    bytes = fat16_read_file(volume, filename, (char*)buffer, capacity);
    if (bytes < 0) return bytes;
    if (bytes == 0) return -2;
    status = gpt2_gguf_build_index(buffer, (uint32_t)bytes, &out->index);
    if (status != 0) return -100 + status;
    out->bytes_loaded = (uint32_t)bytes;
    return 0;
}


int gpt2_gguf_read_tensor_fat16(const fat16_volume_t* volume, const char* filename,
                                const gpt2_gguf_loaded_model_t* model,
                                const gpt2_gguf_tensor_t* tensor,
                                uint32_t tensor_offset, uint8_t* buffer,
                                uint32_t capacity, uint32_t* out_read) {
    uint32_t absolute;
    if (out_read) *out_read = 0U;
    if (!volume || !filename || !model || !tensor || !buffer || capacity == 0U || !out_read) return -1;
    if (!model->index.info.is_valid || tensor_offset > tensor->byte_size) return -3;
    if (capacity > tensor->byte_size - tensor_offset) capacity = tensor->byte_size - tensor_offset;
    if (model->index.info.tensor_data_offset > 0xFFFFFFFFU - tensor->data_offset) return -3;
    absolute = model->index.info.tensor_data_offset + tensor->data_offset;
    if (absolute > 0xFFFFFFFFU - tensor_offset) return -3;
    absolute += tensor_offset;
    return fat16_read_file_range(volume, filename, absolute, buffer, capacity, out_read);
}


int gpt2_gguf_read_quant_block_fat16(const fat16_volume_t* volume, const char* filename,
                                     const gpt2_gguf_loaded_model_t* model,
                                     const gpt2_gguf_tensor_t* tensor,
                                     uint32_t block_index, uint8_t* buffer,
                                     uint32_t capacity, uint32_t* out_read) {
    uint32_t block_bytes;
    uint32_t block_offset;
    if (out_read) *out_read = 0U;
    if (!volume || !filename || !model || !tensor || !buffer || !out_read) return -1;
    if (tensor->type == GPT2_GGUF_TENSOR_Q3_K) block_bytes = GPT2_Q3_K_BLOCK_BYTES;
    else if (tensor->type == GPT2_GGUF_TENSOR_Q4_K) block_bytes = GPT2_Q4_K_BLOCK_BYTES;
    else if (tensor->type == GPT2_GGUF_TENSOR_Q6_K) block_bytes = GPT2_Q6_K_BLOCK_BYTES;
    else return -4;
    if (block_index > 0xFFFFFFFFU / block_bytes) return -5;
    block_offset = block_index * block_bytes;
    if (block_offset > tensor->byte_size || block_bytes > tensor->byte_size - block_offset) return -5;
    if (capacity < block_bytes) return -6;
    return gpt2_gguf_read_tensor_fat16(volume, filename, model, tensor,
                                       block_offset, buffer, block_bytes, out_read);
}


int gpt2_gguf_dot_quant_block_fat16(const fat16_volume_t* volume, const char* filename,
                                    const gpt2_gguf_loaded_model_t* model,
                                    const gpt2_gguf_tensor_t* tensor,
                                    uint32_t block_index, const float* input,
                                    uint32_t count, uint8_t* scratch,
                                    uint32_t scratch_capacity, float* out_dot) {
    uint32_t block_bytes;
    uint32_t read = 0U;
    int status;
    if (!input || !scratch || !out_dot) return -1;
    if (count != GPT2_QK_K) return -7;
    if (!tensor) return -1;
    if (tensor->type == GPT2_GGUF_TENSOR_Q3_K) block_bytes = GPT2_Q3_K_BLOCK_BYTES;
    else if (tensor->type == GPT2_GGUF_TENSOR_Q4_K) block_bytes = GPT2_Q4_K_BLOCK_BYTES;
    else if (tensor->type == GPT2_GGUF_TENSOR_Q6_K) block_bytes = GPT2_Q6_K_BLOCK_BYTES;
    else return -4;
    if (scratch_capacity < block_bytes) return -6;
    status = gpt2_gguf_read_quant_block_fat16(volume, filename, model, tensor,
                                               block_index, scratch, scratch_capacity, &read);
    if (status != 0) return status;
    if (read != block_bytes) return -8;
    if (tensor->type == GPT2_GGUF_TENSOR_Q3_K) *out_dot = gpt2_q3_k_dot_f32(input, scratch, count);
    else if (tensor->type == GPT2_GGUF_TENSOR_Q4_K) *out_dot = gpt2_q4_k_dot_f32(input, scratch, count);
    else *out_dot = gpt2_q6_k_dot_f32(input, scratch, count);
    return 0;
}


int gpt2_gguf_dot_quant_tensor_fat16(const fat16_volume_t* volume, const char* filename,
                                     const gpt2_gguf_loaded_model_t* model,
                                     const gpt2_gguf_tensor_t* tensor,
                                     const float* input, uint32_t count,
                                     uint8_t* scratch, uint32_t scratch_capacity,
                                     float* out_dot) {
    uint32_t blocks;
    uint32_t block;
    float total = 0.0f;
    int status;
    if (!input || !scratch || !out_dot || !tensor) return -1;
    if (count == 0U || (count % GPT2_QK_K) != 0U) return -7;
    blocks = count / GPT2_QK_K;
    for (block = 0U; block < blocks; block++) {
        float partial = 0.0f;
        status = gpt2_gguf_dot_quant_block_fat16(volume, filename, model, tensor,
                                                  block, input + block * GPT2_QK_K,
                                                  GPT2_QK_K, scratch, scratch_capacity,
                                                  &partial);
        if (status != 0) return status;
        total += partial;
    }
    *out_dot = total;
    return 0;
}


int gpt2_gguf_dot_quant_row_fat16(const fat16_volume_t* volume, const char* filename,
                                  const gpt2_gguf_loaded_model_t* model,
                                  const gpt2_gguf_tensor_t* tensor,
                                  uint32_t row_index, const float* input,
                                  uint32_t count, uint8_t* scratch,
                                  uint32_t scratch_capacity, float* out_dot) {
    uint64_t width;
    uint64_t rows = 1U;
    uint32_t block_bytes;
    uint64_t row_bytes;
    uint64_t row_offset;
    uint32_t first_block;
    uint32_t blocks;
    uint32_t block;
    float total = 0.0f;
    int status;
    if (!tensor || tensor->dimensions == 0U || tensor->dimensions > 2U) return -9;
    width = tensor->shape[0];
    if (tensor->dimensions == 2U) rows = tensor->shape[1];
    if (width == 0U || width > 0xFFFFFFFFU || rows == 0U || row_index >= rows) return -9;
    if (count != (uint32_t)width || (count % GPT2_QK_K) != 0U) return -7;
    if (tensor->type == GPT2_GGUF_TENSOR_Q3_K) block_bytes = GPT2_Q3_K_BLOCK_BYTES;
    else if (tensor->type == GPT2_GGUF_TENSOR_Q4_K) block_bytes = GPT2_Q4_K_BLOCK_BYTES;
    else if (tensor->type == GPT2_GGUF_TENSOR_Q6_K) block_bytes = GPT2_Q6_K_BLOCK_BYTES;
    else return -4;
    row_bytes = (width / GPT2_QK_K) * block_bytes;
    row_offset = row_bytes * row_index;
    if (row_offset > 0xFFFFFFFFU) return -9;
    first_block = (uint32_t)(row_offset / block_bytes);
    blocks = (uint32_t)(width / GPT2_QK_K);
    for (block = 0U; block < blocks; block++) {
        float partial = 0.0f;
        if (first_block > 0xFFFFFFFFU - block) return -9;
        status = gpt2_gguf_dot_quant_block_fat16(volume, filename, model, tensor,
                                                  first_block + block,
                                                  input + block * GPT2_QK_K,
                                                  GPT2_QK_K, scratch,
                                                  scratch_capacity, &partial);
        if (status != 0) return status;
        total += partial;
    }
    *out_dot = total;
    return 0;
}


int gpt2_gguf_forward_context_init(const gpt2_gguf_loaded_model_t* model,
                                   uint32_t layer_index, uint32_t channels,
                                   uint32_t position, char* name, uint32_t name_capacity,
                                   uint8_t* scratch, uint32_t scratch_capacity,
                                   gpt2_gguf_forward_context_t* out) {
    int status;
    if (!model || !model->index.info.is_valid || channels == 0U || !name ||
        name_capacity == 0U || !scratch || scratch_capacity == 0U || !out) return -1;
    status = gpt2_gguf_map_layer(&model->index, layer_index, name, name_capacity, &out->layer);
    if (status != 0) return status;
    status = gpt2_gguf_validate_gpt2_layer_storage(&out->layer, channels);
    if (status != 0) return status;
    out->model = model;
    out->scratch = scratch;
    out->scratch_capacity = scratch_capacity;
    out->channels = channels;
    out->position = position;
    return 0;
}


int gpt2_gguf_read_quant_row_fat16(const fat16_volume_t* volume, const char* filename,
                                   const gpt2_gguf_loaded_model_t* model,
                                   const gpt2_gguf_tensor_t* tensor,
                                   uint32_t row_index, uint8_t* buffer,
                                   uint32_t capacity, uint32_t* out_read) {
    uint64_t width;
    uint64_t rows = 1U;
    uint32_t block_bytes;
    uint64_t row_bytes;
    uint64_t row_offset;
    int status;
    if (out_read) *out_read = 0U;
    if (!volume || !filename || !model || !tensor || !buffer || !out_read) return -1;
    if (!model->index.info.is_valid || tensor->dimensions == 0U || tensor->dimensions > 2U) return -9;
    width = tensor->shape[0];
    if (tensor->dimensions == 2U) rows = tensor->shape[1];
    if (width == 0U || width > 0xFFFFFFFFULL || rows == 0U || row_index >= rows) return -9;
    if (tensor->type == GPT2_GGUF_TENSOR_Q3_K) block_bytes = GPT2_Q3_K_BLOCK_BYTES;
    else if (tensor->type == GPT2_GGUF_TENSOR_Q4_K) block_bytes = GPT2_Q4_K_BLOCK_BYTES;
    else if (tensor->type == GPT2_GGUF_TENSOR_Q6_K) block_bytes = GPT2_Q6_K_BLOCK_BYTES;
    else return -4;
    if ((width % GPT2_QK_K) != 0U) return -7;
    row_bytes = (width / GPT2_QK_K) * block_bytes;
    row_offset = row_bytes * row_index;
    if (row_bytes > 0xFFFFFFFFULL || row_offset > 0xFFFFFFFFULL) return -9;
    if (capacity < (uint32_t)row_bytes) return -6;
    status = gpt2_gguf_read_tensor_fat16(volume, filename, model, tensor,
                                          (uint32_t)row_offset, buffer,
                                          (uint32_t)row_bytes, out_read);
    if (status != 0) return status;
    if (*out_read != (uint32_t)row_bytes) return -8;
    return 0;
}


int gpt2_gguf_dot_quant_row_buffer(const gpt2_gguf_tensor_t* tensor,
                                   const uint8_t* row_buffer, uint32_t row_capacity,
                                   const float* input, uint32_t count, float* out_dot) {
    uint32_t block_bytes;
    uint32_t blocks;
    uint32_t block;
    float total = 0.0f;
    if (!tensor || !row_buffer || !input || !out_dot || tensor->dimensions == 0U || tensor->dimensions > 2U) return -1;
    if (tensor->shape[0] == 0U || tensor->shape[0] > 0xFFFFFFFFULL) return -9;
    if (count != (uint32_t)tensor->shape[0] || (count % GPT2_QK_K) != 0U) return -7;
    if (tensor->type == GPT2_GGUF_TENSOR_Q3_K) block_bytes = GPT2_Q3_K_BLOCK_BYTES;
    else if (tensor->type == GPT2_GGUF_TENSOR_Q4_K) block_bytes = GPT2_Q4_K_BLOCK_BYTES;
    else if (tensor->type == GPT2_GGUF_TENSOR_Q6_K) block_bytes = GPT2_Q6_K_BLOCK_BYTES;
    else return -4;
    blocks = count / GPT2_QK_K;
    if (blocks > 0xFFFFFFFFU / block_bytes || row_capacity < blocks * block_bytes) return -6;
    for (block = 0U; block < blocks; block++) {
        const float* values = input + block * GPT2_QK_K;
        const uint8_t* encoded = row_buffer + block * block_bytes;
        if (tensor->type == GPT2_GGUF_TENSOR_Q3_K) total += gpt2_q3_k_dot_f32(values, encoded, GPT2_QK_K);
        else if (tensor->type == GPT2_GGUF_TENSOR_Q4_K) total += gpt2_q4_k_dot_f32(values, encoded, GPT2_QK_K);
        else total += gpt2_q6_k_dot_f32(values, encoded, GPT2_QK_K);
    }
    *out_dot = total;
    return 0;
}


int gpt2_gguf_project_qkv_row_fat16(const fat16_volume_t* volume, const char* filename,
                                    const gpt2_gguf_loaded_model_t* model,
                                    const gpt2_gguf_tensor_t* tensor, uint32_t channels,
                                    uint32_t output_index, const float* input,
                                    uint8_t* row_buffer, uint32_t row_capacity,
                                    float* out_value) {
    uint32_t read = 0U;
    int status;
    if (!tensor || channels == 0U || !input || !row_buffer || !out_value) return -1;
    if (tensor->dimensions != 2U || tensor->shape[0] != channels ||
        tensor->shape[1] != 3ULL * channels || output_index >= 3U * channels) return -9;
    status = gpt2_gguf_read_quant_row_fat16(volume, filename, model, tensor,
                                             output_index, row_buffer, row_capacity, &read);
    if (status != 0) return status;
    return gpt2_gguf_dot_quant_row_buffer(tensor, row_buffer, read, input, channels, out_value);
}


int gpt2_gguf_project_qkv_fat16(const fat16_volume_t* volume, const char* filename,
                                const gpt2_gguf_loaded_model_t* model,
                                const gpt2_gguf_tensor_t* tensor, uint32_t channels,
                                const float* input, uint8_t* row_buffer,
                                uint32_t row_capacity, float* query, uint32_t query_capacity,
                                float* key, uint32_t key_capacity,
                                float* value, uint32_t value_capacity) {
    uint32_t output_index;
    int status;
    if (!query || !key || !value || channels == 0U ||
        query_capacity < channels * sizeof(float) ||
        key_capacity < channels * sizeof(float) ||
        value_capacity < channels * sizeof(float)) return -6;
    for (output_index = 0U; output_index < 3U * channels; output_index++) {
        float result = 0.0f;
        status = gpt2_gguf_project_qkv_row_fat16(volume, filename, model, tensor, channels,
                                                  output_index, input, row_buffer,
                                                  row_capacity, &result);
        if (status != 0) return status;
        if (output_index < channels) query[output_index] = result;
        else if (output_index < 2U * channels) key[output_index - channels] = result;
        else value[output_index - 2U * channels] = result;
    }
    return 0;
}


int gpt2_gguf_project_matrix_fat16(const fat16_volume_t* volume, const char* filename,
                                   const gpt2_gguf_loaded_model_t* model,
                                   const gpt2_gguf_tensor_t* tensor,
                                   uint32_t input_channels, uint32_t output_channels,
                                   const float* input, uint8_t* row_buffer,
                                   uint32_t row_capacity, float* output,
                                   uint32_t output_capacity) {
    uint32_t row;
    int status;
    if (!volume || !filename || !model || !tensor || !input || !row_buffer || !output) return -1;
    if (input_channels == 0U || output_channels == 0U ||
        tensor->dimensions != 2U || tensor->shape[0] != input_channels ||
        tensor->shape[1] != output_channels) return -9;
    if (output_capacity < output_channels * sizeof(float)) return -6;
    for (row = 0U; row < output_channels; row++) {
        uint32_t read = 0U;
        status = gpt2_gguf_read_quant_row_fat16(volume, filename, model, tensor,
                                                row, row_buffer, row_capacity, &read);
        if (status != 0) return status;
        status = gpt2_gguf_dot_quant_row_buffer(tensor, row_buffer, read,
                                                input, input_channels, &output[row]);
        if (status != 0) return status;
    }
    return 0;
}


static int gpt2_gguf_kv_cache_offset(const gpt2_gguf_kv_cache_t* cache,
                                     uint32_t layer, uint32_t position,
                                     uint32_t* out_offset) {
    uint64_t slots;
    uint64_t offset;
    if (!cache || !out_offset || !cache->storage || cache->layers == 0U ||
        cache->max_positions == 0U || cache->channels == 0U ||
        layer >= cache->layers || position >= cache->max_positions) return -9;
    slots = ((uint64_t)layer * cache->max_positions) + position;
    offset = slots * 2ULL * cache->channels;
    if (offset > 0xFFFFFFFFULL || offset + 2ULL * cache->channels > 0xFFFFFFFFULL) return -9;
    *out_offset = (uint32_t)offset;
    return 0;
}

int gpt2_gguf_kv_cache_init(float* storage, uint32_t storage_floats,
                            uint32_t layers, uint32_t max_positions,
                            uint32_t channels, gpt2_gguf_kv_cache_t* out) {
    uint64_t required;
    if (!storage || !out || layers == 0U || max_positions == 0U || channels == 0U) return -1;
    required = (uint64_t)layers * max_positions * 2ULL * channels;
    if (required > 0xFFFFFFFFULL || storage_floats < (uint32_t)required) return -6;
    out->storage = storage;
    out->layers = layers;
    out->max_positions = max_positions;
    out->channels = channels;
    out->count = 0U;
    return 0;
}

int gpt2_gguf_kv_cache_put(gpt2_gguf_kv_cache_t* cache, uint32_t layer,
                           uint32_t position, const float* key, const float* value) {
    uint32_t offset;
    uint32_t i;
    int status;
    if (!key || !value) return -1;
    status = gpt2_gguf_kv_cache_offset(cache, layer, position, &offset);
    if (status != 0) return status;
    for (i = 0U; i < cache->channels; i++) {
        cache->storage[offset + i] = key[i];
        cache->storage[offset + cache->channels + i] = value[i];
    }
    if (position + 1U > cache->count) cache->count = position + 1U;
    return 0;
}

int gpt2_gguf_kv_cache_get(const gpt2_gguf_kv_cache_t* cache, uint32_t layer,
                           uint32_t position, float* key, float* value) {
    uint32_t offset;
    uint32_t i;
    int status;
    if (!key || !value) return -1;
    status = gpt2_gguf_kv_cache_offset(cache, layer, position, &offset);
    if (status != 0) return status;
    for (i = 0U; i < cache->channels; i++) {
        key[i] = cache->storage[offset + i];
        value[i] = cache->storage[offset + cache->channels + i];
    }
    return 0;
}


int gpt2_gguf_kv_cache_copy_history(const gpt2_gguf_kv_cache_t* cache, uint32_t layer,
                                    uint32_t start_position, uint32_t position_count,
                                    float* key_out, uint32_t key_capacity,
                                    float* value_out, uint32_t value_capacity,
                                    uint32_t* out_count) {
    uint32_t position;
    uint32_t copied = 0U;
    int status;
    if (out_count) *out_count = 0U;
    if (!cache || !key_out || !value_out || !out_count) return -1;
    if (position_count == 0U) return 0;
    if (position_count > 0xFFFFFFFFU / cache->channels ||
        key_capacity < position_count * cache->channels ||
        value_capacity < position_count * cache->channels) return -6;
    if (start_position > cache->count || position_count > cache->count - start_position) return -9;
    for (position = 0U; position < position_count; position++) {
        status = gpt2_gguf_kv_cache_get(cache, layer, start_position + position,
                                        key_out + position * cache->channels,
                                        value_out + position * cache->channels);
        if (status != 0) return status;
        copied++;
    }
    *out_count = copied;
    return 0;
}


int gpt2_gguf_kv_cache_query_scores(const gpt2_gguf_kv_cache_t* cache, uint32_t layer,
                                    uint32_t start_position, uint32_t position_count,
                                    const float* query, float* key_scratch,
                                    uint32_t key_scratch_capacity, float* scores,
                                    uint32_t score_capacity, uint32_t* out_count) {
    uint32_t position;
    int status;
    if (out_count) *out_count = 0U;
    if (!cache || !query || !key_scratch || !scores || !out_count) return -1;
    if (key_scratch_capacity < cache->channels || score_capacity < position_count) return -6;
    if (position_count == 0U) return 0;
    if (start_position > cache->count || position_count > cache->count - start_position) return -9;
    for (position = 0U; position < position_count; position++) {
        uint32_t i;
        uint32_t offset;
        float dot = 0.0f;
        status = gpt2_gguf_kv_cache_offset(cache, layer, start_position + position, &offset);
        if (status != 0) return status;
        for (i = 0U; i < cache->channels; i++) {
            key_scratch[i] = cache->storage[offset + i];
            dot += query[i] * key_scratch[i];
        }
        scores[position] = dot;
    }
    *out_count = position_count;
    return 0;
}


int gpt2_gguf_kv_cache_accumulate_values(const gpt2_gguf_kv_cache_t* cache, uint32_t layer,
                                         uint32_t start_position, uint32_t position_count,
                                         const float* weights, uint32_t weight_capacity,
                                         float* output, uint32_t output_capacity,
                                         uint32_t* out_count) {
    uint32_t position;
    uint32_t channel;
    int status;
    if (out_count) *out_count = 0U;
    if (!cache || !weights || !output || !out_count) return -1;
    if (weight_capacity < position_count || output_capacity < cache->channels) return -6;
    if (position_count == 0U) return 0;
    if (start_position > cache->count || position_count > cache->count - start_position) return -9;
    for (channel = 0U; channel < cache->channels; channel++) output[channel] = 0.0f;
    for (position = 0U; position < position_count; position++) {
        uint32_t offset;
        float weight = weights[position];
        status = gpt2_gguf_kv_cache_offset(cache, layer, start_position + position, &offset);
        if (status != 0) return status;
        for (channel = 0U; channel < cache->channels; channel++) {
            output[channel] += weight * cache->storage[offset + cache->channels + channel];
        }
    }
    *out_count = cache->channels;
    return 0;
}


static float gpt2_gguf_attention_inv_sqrt(float value) {
    union { float f; uint32_t u; } convert;
    float half;
    convert.f = value;
    half = 0.5f * value;
    convert.u = 0x5f3759dfU - (convert.u >> 1);
    convert.f = convert.f * (1.5f - half * convert.f * convert.f);
    return convert.f;
}


static float gpt2_gguf_attention_fast_exp(float value) {
    union { float f; uint32_t u; } convert;
    if (value < -80.0f) return 0.0f;
    if (value > 80.0f) value = 80.0f;
    convert.u = (uint32_t)(12102203.0f * value + 1064866805.0f);
    return convert.f;
}


int gpt2_gguf_attention_scale_scores(float* scores, uint32_t score_count, uint32_t head_size) {
    uint32_t i;
    float scale;
    if (!scores || head_size == 0U) return -1;
    scale = gpt2_gguf_attention_inv_sqrt((float)head_size);
    for (i = 0U; i < score_count; i++) scores[i] *= scale;
    return 0;
}


int gpt2_gguf_attention_softmax(float* scores, uint32_t score_count, uint32_t* out_count) {
    uint32_t i;
    float maximum;
    float total = 0.0f;
    if (out_count) *out_count = 0U;
    if (!scores || !out_count) return -1;
    if (score_count == 0U) return 0;
    maximum = scores[0];
    for (i = 1U; i < score_count; i++) {
        if (scores[i] > maximum) maximum = scores[i];
    }
    for (i = 0U; i < score_count; i++) {
        scores[i] = gpt2_gguf_attention_fast_exp(scores[i] - maximum);
        total += scores[i];
    }
    if (total <= 0.0f) return -7;
    for (i = 0U; i < score_count; i++) scores[i] /= total;
    *out_count = score_count;
    return 0;
}


int gpt2_gguf_kv_cache_attention_head(const gpt2_gguf_kv_cache_t* cache, uint32_t layer,
                                      uint32_t start_position, uint32_t position_count,
                                      const float* query, uint32_t head_count,
                                      uint32_t head_index, float* key_scratch,
                                      uint32_t key_scratch_capacity, float* scores,
                                      uint32_t score_capacity, float* output,
                                      uint32_t output_capacity, uint32_t* out_count) {
    uint32_t head_size;
    uint32_t head_base;
    uint32_t position;
    uint32_t channel;
    int status;
    if (out_count) *out_count = 0U;
    if (!cache || !query || !key_scratch || !scores || !output || !out_count) return -1;
    if (head_count == 0U || head_index >= head_count ||
        cache->channels == 0U || (cache->channels % head_count) != 0U) return -9;
    head_size = cache->channels / head_count;
    head_base = head_index * head_size;
    if (key_scratch_capacity < head_size || score_capacity < position_count ||
        output_capacity < head_size) return -6;
    if (position_count == 0U) return 0;
    if (start_position > cache->count || position_count > cache->count - start_position) return -9;
    for (position = 0U; position < position_count; position++) {
        uint32_t offset;
        float dot = 0.0f;
        status = gpt2_gguf_kv_cache_offset(cache, layer, start_position + position, &offset);
        if (status != 0) return status;
        for (channel = 0U; channel < head_size; channel++) {
            key_scratch[channel] = cache->storage[offset + head_base + channel];
            dot += query[channel] * key_scratch[channel];
        }
        scores[position] = dot;
    }
    status = gpt2_gguf_attention_scale_scores(scores, position_count, head_size);
    if (status != 0) return status;
    status = gpt2_gguf_attention_softmax(scores, position_count, out_count);
    if (status != 0) return status;
    for (channel = 0U; channel < head_size; channel++) output[channel] = 0.0f;
    for (position = 0U; position < position_count; position++) {
        uint32_t offset;
        status = gpt2_gguf_kv_cache_offset(cache, layer, start_position + position, &offset);
        if (status != 0) return status;
        for (channel = 0U; channel < head_size; channel++) {
            output[channel] += scores[position] *
                               cache->storage[offset + cache->channels + head_base + channel];
        }
    }
    *out_count = head_size;
    return 0;
}


int gpt2_gguf_attention_concat_heads(const float* head_outputs, uint32_t head_count,
                                      uint32_t head_size, float* output,
                                      uint32_t output_capacity, uint32_t* out_count) {
    uint32_t total;
    uint32_t i;
    if (out_count) *out_count = 0U;
    if (!head_outputs || !output || !out_count) return -1;
    if (head_count == 0U || head_size == 0U) return -9;
    if (head_count > 0xFFFFFFFFU / head_size) return -9;
    total = head_count * head_size;
    if (output_capacity < total) return -6;
    for (i = 0U; i < total; i++) output[i] = head_outputs[i];
    *out_count = total;
    return 0;
}


int gpt2_gguf_kv_cache_attention_multi_head(const gpt2_gguf_kv_cache_t* cache, uint32_t layer,
                                            uint32_t start_position, uint32_t position_count,
                                            const float* query, uint32_t head_count,
                                            float* head_outputs, uint32_t head_output_capacity,
                                            float* key_scratch, uint32_t key_scratch_capacity,
                                            float* scores, uint32_t score_capacity,
                                            float* output, uint32_t output_capacity,
                                            uint32_t* out_count) {
    uint32_t head_size;
    uint32_t head;
    uint32_t channels;
    uint32_t produced;
    int status;
    if (out_count) *out_count = 0U;
    if (!cache || !query || !head_outputs || !key_scratch || !scores || !output || !out_count) return -1;
    if (head_count == 0U || cache->channels == 0U || (cache->channels % head_count) != 0U) return -9;
    channels = cache->channels;
    head_size = channels / head_count;
    if (head_output_capacity < channels || output_capacity < channels ||
        key_scratch_capacity < head_size || score_capacity < position_count) return -6;
    for (head = 0U; head < head_count; head++) {
        status = gpt2_gguf_kv_cache_attention_head(cache, layer, start_position,
                                                    position_count, query, head_count, head,
                                                    key_scratch, key_scratch_capacity, scores,
                                                    score_capacity, head_outputs + head * head_size,
                                                    head_size, &produced);
        if (status != 0) return status;
        if (produced != head_size) return -7;
    }
    status = gpt2_gguf_attention_concat_heads(head_outputs, head_count, head_size,
                                               output, output_capacity, out_count);
    return status;
}


int gpt2_gguf_add_residual(float* residual, uint32_t residual_capacity,
                           const float* attention, uint32_t attention_count) {
    uint32_t i;
    if (!residual || !attention) return -1;
    if (residual_capacity < attention_count) return -6;
    for (i = 0U; i < attention_count; i++) residual[i] += attention[i];
    return 0;
}


int gpt2_gguf_layernorm(const float* input, uint32_t input_count,
                        const float* gamma, const float* beta,
                        float epsilon, float* output, uint32_t output_capacity) {
    uint32_t i;
    float mean = 0.0f;
    float variance = 0.0f;
    float inverse;
    if (!input || !gamma || !beta || !output) return -1;
    if (input_count == 0U || output_capacity < input_count || epsilon <= 0.0f) return -6;
    for (i = 0U; i < input_count; i++) mean += input[i];
    mean /= (float)input_count;
    for (i = 0U; i < input_count; i++) {
        float delta = input[i] - mean;
        variance += delta * delta;
    }
    variance /= (float)input_count;
    inverse = gpt2_gguf_attention_inv_sqrt(variance + epsilon);
    for (i = 0U; i < input_count; i++)
        output[i] = (input[i] - mean) * inverse * gamma[i] + beta[i];
    return 0;
}


int gpt2_gguf_gelu(const float* input, uint32_t input_count,
                   float* output, uint32_t output_capacity) {
    uint32_t i;
    if (!input || !output) return -1;
    if (output_capacity < input_count) return -6;
    for (i = 0U; i < input_count; i++) {
        float value = input[i];
        float exponent = gpt2_gguf_attention_fast_exp(-1.702f * value);
        float sigmoid = 1.0f / (1.0f + exponent);
        output[i] = value * sigmoid;
    }
    return 0;
}


int gpt2_gguf_mlp_forward_fat16(const fat16_volume_t* volume, const char* filename,
                                const gpt2_gguf_loaded_model_t* model,
                                const gpt2_gguf_tensor_t* up_tensor,
                                const gpt2_gguf_tensor_t* down_tensor,
                                uint32_t channels, uint32_t hidden_channels,
                                const float* input, uint8_t* row_buffer,
                                uint32_t row_capacity, const float* up_bias,
                                const float* down_bias, float* hidden,
                                uint32_t hidden_capacity, float* output,
                                uint32_t output_capacity) {
    uint32_t i;
    int status;
    if (!volume || !filename || !model || !up_tensor || !down_tensor ||
        !input || !row_buffer || !hidden || !output) return -1;
    if (channels == 0U || hidden_channels == 0U ||
        hidden_capacity < hidden_channels || output_capacity < channels) return -6;
    if (up_tensor->dimensions != 2U || down_tensor->dimensions != 2U ||
        up_tensor->shape[0] != channels || up_tensor->shape[1] != hidden_channels ||
        down_tensor->shape[0] != hidden_channels || down_tensor->shape[1] != channels) return -9;
    status = gpt2_gguf_project_matrix_fat16(volume, filename, model, up_tensor,
                                             channels, hidden_channels, input,
                                             row_buffer, row_capacity, hidden,
                                             hidden_capacity);
    if (status != 0) return status;
    if (up_bias) for (i = 0U; i < hidden_channels; i++) hidden[i] += up_bias[i];
    status = gpt2_gguf_gelu(hidden, hidden_channels, hidden, hidden_capacity);
    if (status != 0) return status;
    status = gpt2_gguf_project_matrix_fat16(volume, filename, model, down_tensor,
                                             hidden_channels, channels, hidden,
                                             row_buffer, row_capacity, output,
                                             output_capacity);
    if (status != 0) return status;
    if (down_bias) for (i = 0U; i < channels; i++) output[i] += down_bias[i];
    return 0;
}
