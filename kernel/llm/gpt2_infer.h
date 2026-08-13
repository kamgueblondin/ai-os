#ifndef AIOS_GPT2_INFER_H
#define AIOS_GPT2_INFER_H

#include <stdint.h>

/*
 * Generate one token from a sequence of GPT-2 token identifiers. The current
 * CPU reference backend caps the context at 64 tokens to bound activation
 * memory during the first bare-metal implementation.
 */
int gpt2_generate_next(const uint32_t* tokens, uint32_t token_count, uint32_t* next_token);
/* Top-k, temperature 1.10, penalite de frequence, interdiction du jeton precedent. */
int gpt2_generate_next_sampled(const uint32_t* tokens, uint32_t token_count,
                               uint32_t* next_token, uint32_t* rng_state);
const char* gpt2_infer_status(void);

#endif
