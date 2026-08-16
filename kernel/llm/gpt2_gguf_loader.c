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
