#include "gpt2_gguf.h"

#define GGUF_DEFAULT_ALIGNMENT 32U
#define GGUF_MAX_TENSORS       512U
#define GGUF_MAX_METADATA      512U
#define GGUF_MAX_ARRAY_ELEMENTS 100000U
#define GGUF_MAX_DIMS          4U
#define GGUF_MAX_STRING        65535U

static int gguf_range(uint32_t offset, uint32_t count, uint32_t size) {
    return offset <= size && count <= size - offset;
}

static int gguf_read_u32(const uint8_t* blob, uint32_t size, uint32_t* offset, uint32_t* out) {
    uint32_t at = *offset;
    if (!gguf_range(at, 4U, size)) return -1;
    *out = (uint32_t)blob[at] |
           ((uint32_t)blob[at + 1U] << 8) |
           ((uint32_t)blob[at + 2U] << 16) |
           ((uint32_t)blob[at + 3U] << 24);
    *offset = at + 4U;
    return 0;
}

static int gguf_read_u64(const uint8_t* blob, uint32_t size, uint32_t* offset, uint64_t* out) {
    uint32_t lo;
    uint32_t hi;
    if (gguf_read_u32(blob, size, offset, &lo) != 0 ||
        gguf_read_u32(blob, size, offset, &hi) != 0) return -1;
    *out = (uint64_t)lo | ((uint64_t)hi << 32);
    return 0;
}

static int gguf_skip(const uint8_t* blob, uint32_t size, uint32_t* offset, uint32_t count) {
    (void)blob;
    if (!gguf_range(*offset, count, size)) return -1;
    *offset += count;
    return 0;
}

static int gguf_read_string(const uint8_t* blob, uint32_t size, uint32_t* offset,
                            const uint8_t** text, uint32_t* length) {
    uint64_t length64;
    if (gguf_read_u64(blob, size, offset, &length64) != 0 ||
        length64 > GGUF_MAX_STRING || length64 > 0xffffffffULL) return -1;
    if (!gguf_range(*offset, (uint32_t)length64, size)) return -1;
    if (text) *text = blob + *offset;
    if (length) *length = (uint32_t)length64;
    *offset += (uint32_t)length64;
    return 0;
}

static int gguf_key_equals(const uint8_t* key, uint32_t key_len, const char* expected) {
    uint32_t i = 0;
    while (expected[i]) {
        if (i >= key_len || key[i] != (uint8_t)expected[i]) return 0;
        i++;
    }
    return i == key_len;
}

static int gguf_value_size(uint32_t type, uint32_t* out) {
    switch (type) {
        case GPT2_GGUF_VALUE_UINT8:
        case GPT2_GGUF_VALUE_INT8:
        case GPT2_GGUF_VALUE_BOOL: *out = 1U; return 0;
        case GPT2_GGUF_VALUE_UINT16:
        case GPT2_GGUF_VALUE_INT16: *out = 2U; return 0;
        case GPT2_GGUF_VALUE_UINT32:
        case GPT2_GGUF_VALUE_INT32:
        case GPT2_GGUF_VALUE_FLOAT32: *out = 4U; return 0;
        case GPT2_GGUF_VALUE_UINT64:
        case GPT2_GGUF_VALUE_INT64:
        case GPT2_GGUF_VALUE_FLOAT64: *out = 8U; return 0;
        default: return -1;
    }
}

static int gguf_skip_value(const uint8_t* blob, uint32_t size, uint32_t* offset,
                           uint32_t type, uint32_t depth) {
    uint32_t item_size;
    uint32_t child_type;
    uint64_t count;
    if (depth > 4U) return -1;
    if (type == GPT2_GGUF_VALUE_STRING) return gguf_read_string(blob, size, offset, 0, 0);
    if (type == GPT2_GGUF_VALUE_ARRAY) {
        if (gguf_read_u32(blob, size, offset, &child_type) != 0 ||
            gguf_read_u64(blob, size, offset, &count) != 0 || count > GGUF_MAX_ARRAY_ELEMENTS) return -1;
        while (count-- > 0U) {
            if (gguf_skip_value(blob, size, offset, child_type, depth + 1U) != 0) return -1;
        }
        return 0;
    }
    if (gguf_value_size(type, &item_size) != 0) return -1;
    return gguf_skip(blob, size, offset, item_size);
}

static int gguf_read_alignment_value(const uint8_t* blob, uint32_t size, uint32_t* offset,
                                     uint32_t type, uint32_t* alignment) {
    uint32_t value;
    if (type != GPT2_GGUF_VALUE_UINT32 || gguf_read_u32(blob, size, offset, &value) != 0) return -1;
    if (value < 8U || (value & 7U) != 0U) return -1;
    *alignment = value;
    return 0;
}

static int gguf_is_gpt2_value(const uint8_t* blob, uint32_t size, uint32_t* offset,
                               uint32_t type, uint8_t* is_gpt2) {
    const uint8_t* text;
    uint32_t length;
    static const char gpt2[] = "gpt2";
    uint32_t i;
    if (type != GPT2_GGUF_VALUE_STRING ||
        gguf_read_string(blob, size, offset, &text, &length) != 0 || length != 4U) return -1;
    for (i = 0; i < 4U; i++) if (text[i] != (uint8_t)gpt2[i]) return -1;
    *is_gpt2 = 1U;
    return 0;
}

static int gguf_align(uint32_t offset, uint32_t alignment, uint32_t* out) {
    uint32_t remainder;
    if (alignment == 0U) return -1;
    remainder = offset % alignment;
    if (remainder != 0U && offset > 0xffffffffU - (alignment - remainder)) return -1;
    *out = remainder ? offset + (alignment - remainder) : offset;
    return 0;
}

int gpt2_gguf_probe_blob(const uint8_t* blob, uint32_t blob_size, gpt2_gguf_info_t* out) {
    uint32_t offset = 0;
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count64;
    uint64_t metadata_count64;
    uint32_t tensor_count;
    uint32_t metadata_count;
    uint32_t i;
    gpt2_gguf_info_t info;

    if (!blob || !out) return -1;
    info.version = 0U;
    info.tensor_count = 0U;
    info.metadata_count = 0U;
    info.alignment = GGUF_DEFAULT_ALIGNMENT;
    info.tensor_data_offset = 0U;
    info.f32_tensors = 0U;
    info.q8_0_tensors = 0U;
    info.unsupported_quantized_tensors = 0U;
    info.is_gpt2 = 0U;
    info.is_valid = 0U;

    if (gguf_read_u32(blob, blob_size, &offset, &magic) != 0 || magic != GPT2_GGUF_MAGIC ||
        gguf_read_u32(blob, blob_size, &offset, &version) != 0 || version != GPT2_GGUF_VERSION ||
        gguf_read_u64(blob, blob_size, &offset, &tensor_count64) != 0 ||
        gguf_read_u64(blob, blob_size, &offset, &metadata_count64) != 0 ||
        tensor_count64 > GGUF_MAX_TENSORS || metadata_count64 > GGUF_MAX_METADATA) return -2;

    tensor_count = (uint32_t)tensor_count64;
    metadata_count = (uint32_t)metadata_count64;
    info.version = version;
    info.tensor_count = tensor_count;
    info.metadata_count = metadata_count;

    for (i = 0U; i < metadata_count; i++) {
        const uint8_t* key;
        uint32_t key_len;
        uint32_t type;
        if (gguf_read_string(blob, blob_size, &offset, &key, &key_len) != 0 ||
            gguf_read_u32(blob, blob_size, &offset, &type) != 0) return -3;
        if (gguf_key_equals(key, key_len, "general.architecture")) {
            if (gguf_is_gpt2_value(blob, blob_size, &offset, type, &info.is_gpt2) != 0) return -4;
        } else if (gguf_key_equals(key, key_len, "general.alignment")) {
            if (gguf_read_alignment_value(blob, blob_size, &offset, type, &info.alignment) != 0) return -5;
        } else if (gguf_skip_value(blob, blob_size, &offset, type, 0U) != 0) return -6;
    }

    for (i = 0U; i < tensor_count; i++) {
        uint32_t dimensions;
        uint32_t type;
        uint64_t data_offset;
        uint32_t j;
        if (gguf_read_string(blob, blob_size, &offset, 0, 0) != 0 ||
            gguf_read_u32(blob, blob_size, &offset, &dimensions) != 0 ||
            dimensions == 0U || dimensions > GGUF_MAX_DIMS) return -7;
        for (j = 0U; j < dimensions; j++) {
            uint64_t dimension;
            if (gguf_read_u64(blob, blob_size, &offset, &dimension) != 0 || dimension == 0U) return -8;
        }
        if (gguf_read_u32(blob, blob_size, &offset, &type) != 0 ||
            gguf_read_u64(blob, blob_size, &offset, &data_offset) != 0 ||
            data_offset > 0xffffffffULL || ((uint32_t)data_offset % info.alignment) != 0U) return -9;
        if (type == GPT2_GGUF_TENSOR_F32) info.f32_tensors++;
        else if (type == GPT2_GGUF_TENSOR_Q8_0) info.q8_0_tensors++;
        else if (type != GPT2_GGUF_TENSOR_F16) info.unsupported_quantized_tensors++;
    }

    if (!info.is_gpt2 || gguf_align(offset, info.alignment, &info.tensor_data_offset) != 0 ||
        info.tensor_data_offset > blob_size) return -10;
    info.is_valid = 1U;
    *out = info;
    return 0;
}

const char* gpt2_gguf_probe_status(int status) {
    switch (status) {
        case 0: return "GGUF GPT-2 v3 valide (execution quantifiee non activee)";
        case -1: return "GGUF: argument invalide";
        case -2: return "GGUF: magic, version ou compte invalide";
        case -3: return "GGUF: metadonnees tronquees";
        case -4: return "GGUF: architecture GPT-2 absente";
        case -5: return "GGUF: alignement invalide";
        case -6: return "GGUF: type de metadonnee invalide";
        case -7: return "GGUF: descripteur de tenseur invalide";
        case -8: return "GGUF: dimension de tenseur invalide";
        case -9: return "GGUF: type ou offset de tenseur invalide";
        default: return "GGUF: donnees invalides";
    }
}
