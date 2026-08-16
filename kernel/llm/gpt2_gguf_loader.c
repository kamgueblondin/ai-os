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
