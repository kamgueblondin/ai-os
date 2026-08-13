#ifndef AIOS_GPT2_SAMPLE_H
#define AIOS_GPT2_SAMPLE_H

#include <stdint.h>

#define GPT2_SAMPLE_TOP_K 8U

/*
 * Top-k sampling with a temperature of 1.10, a frequency penalty on the last
 * 16 history tokens, and a hard ban of the immediately previous token.
 * The ban stops greedy-like loops such as GPT-2 token 262 (" The").
 */
uint32_t gpt2_sample_top_k(const float* logits, uint32_t vocab, const uint32_t* history,
                           uint32_t history_count, uint32_t* rng_state);

#endif
