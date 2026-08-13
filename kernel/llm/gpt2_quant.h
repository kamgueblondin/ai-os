#ifndef AIOS_GPT2_QUANT_H
#define AIOS_GPT2_QUANT_H

#include <stdint.h>

/* GGML Q8_0: one IEEE-754 binary16 scale and 32 signed quantized values. */
#define GPT2_Q8_0_BLOCK_SIZE 32U
#define GPT2_Q8_0_BLOCK_BYTES 34U

/* Convert an IEEE-754 binary16 bit pattern to FP32 without a libc dependency. */
float gpt2_f16_to_f32(uint16_t bits);

/*
 * Dot product of a FP32 activation vector and a contiguous GGML Q8_0 row.
 * `count` must be a non-zero multiple of 32. Returns 0.0f for invalid input.
 */
float gpt2_q8_0_dot_f32(const float* input, const uint8_t* q8_blocks, uint32_t count);

#endif
