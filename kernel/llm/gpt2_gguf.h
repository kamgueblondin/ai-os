#ifndef AIOS_GPT2_GGUF_H
#define AIOS_GPT2_GGUF_H

#include <stdint.h>

/* GGUF v3 is little-endian. The reader intentionally accepts only the
 * structural envelope: execution requires explicit quantization kernels. */
#define GPT2_GGUF_MAGIC   0x46554747U /* bytes: G G U F */
#define GPT2_GGUF_VERSION 3U

#define GPT2_GGUF_VALUE_UINT8    0U
#define GPT2_GGUF_VALUE_INT8     1U
#define GPT2_GGUF_VALUE_UINT16   2U
#define GPT2_GGUF_VALUE_INT16    3U
#define GPT2_GGUF_VALUE_UINT32   4U
#define GPT2_GGUF_VALUE_INT32    5U
#define GPT2_GGUF_VALUE_FLOAT32  6U
#define GPT2_GGUF_VALUE_BOOL     7U
#define GPT2_GGUF_VALUE_STRING   8U
#define GPT2_GGUF_VALUE_ARRAY    9U
#define GPT2_GGUF_VALUE_UINT64  10U
#define GPT2_GGUF_VALUE_INT64   11U
#define GPT2_GGUF_VALUE_FLOAT64 12U

#define GPT2_GGUF_TENSOR_F32  0U
#define GPT2_GGUF_TENSOR_F16  1U
#define GPT2_GGUF_TENSOR_Q4_0 2U
#define GPT2_GGUF_TENSOR_Q8_0 8U
#define GPT2_GGUF_TENSOR_Q3_K 11U
#define GPT2_GGUF_TENSOR_Q4_K 12U
#define GPT2_GGUF_TENSOR_Q6_K 14U

/* A structural report. No pointer from an untrusted blob is exposed. */
typedef struct {
    uint32_t version;
    uint32_t tensor_count;
    uint32_t metadata_count;
    uint32_t alignment;
    uint32_t tensor_data_offset;
    uint32_t f32_tensors;
    uint32_t q8_0_tensors;
    uint32_t q3_k_tensors;
    uint32_t q4_k_tensors;
    uint32_t q6_k_tensors;
    uint32_t unsupported_quantized_tensors;
    uint8_t is_gpt2;
    uint8_t is_valid;
} gpt2_gguf_info_t;

/* Returns 0 only for a bounded, structurally valid GPT-2 GGUF v3 blob.
 * The function does not allocate and does not execute model data. */
int gpt2_gguf_probe_blob(const uint8_t* blob, uint32_t blob_size,
                         gpt2_gguf_info_t* out);

/* Human-readable status for `ai-runtime` and diagnostics. */
const char* gpt2_gguf_probe_status(int status);

#endif
