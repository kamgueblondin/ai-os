#ifndef AIOS_GPT2_SAMPLE_H
#define AIOS_GPT2_SAMPLE_H

#include <stdint.h>

#define GPT2_SAMPLE_TOP_K 8U

/*
 * Near-greedy top-k (temperature 0.6). Frequency penalty and the previous-token
 * ban apply only to tokens already emitted, never to the prompt. That stops
 * GPT-2's " The The The" loop without derailing the continuation.
 */
uint32_t gpt2_sample_top_k(const float* logits, uint32_t vocab,
                           const uint32_t* generated, uint32_t generated_count,
                           uint32_t* rng_state);

#endif
