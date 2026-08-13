#include "gpt2_quant.h"

float gpt2_f16_to_f32(uint16_t bits) {
    union { uint32_t u; float f; } out;
    uint32_t sign = ((uint32_t)bits & 0x8000U) << 16;
    uint32_t exponent = ((uint32_t)bits >> 10) & 0x1fU;
    uint32_t fraction = (uint32_t)bits & 0x03ffU;

    if (exponent == 0U) {
        if (fraction == 0U) {
            out.u = sign;
            return out.f;
        }
        /* Normalize the binary16 subnormal before changing exponent bias. */
        while ((fraction & 0x0400U) == 0U) {
            fraction <<= 1;
            exponent--;
        }
        fraction &= 0x03ffU;
        exponent = 1U;
    } else if (exponent == 31U) {
        out.u = sign | 0x7f800000U | (fraction << 13);
        return out.f;
    }

    exponent = exponent + (127U - 15U);
    out.u = sign | (exponent << 23) | (fraction << 13);
    return out.f;
}

float gpt2_q8_0_dot_f32(const float* input, const uint8_t* q8_blocks, uint32_t count) {
    uint32_t block;
    uint32_t blocks;
    float result = 0.0f;

    if (!input || !q8_blocks || count == 0U || (count % GPT2_Q8_0_BLOCK_SIZE) != 0U) return 0.0f;
    blocks = count / GPT2_Q8_0_BLOCK_SIZE;
    for (block = 0U; block < blocks; block++) {
        const uint8_t* raw = q8_blocks + block * GPT2_Q8_0_BLOCK_BYTES;
        uint16_t scale_bits = (uint16_t)raw[0] | ((uint16_t)raw[1] << 8);
        float scale = gpt2_f16_to_f32(scale_bits);
        float sum = 0.0f;
        uint32_t i;
        for (i = 0U; i < GPT2_Q8_0_BLOCK_SIZE; i++) {
            int8_t quant = (int8_t)raw[2U + i];
            sum += input[block * GPT2_Q8_0_BLOCK_SIZE + i] * (float)quant;
        }
        result += scale * sum;
    }
    return result;
}
