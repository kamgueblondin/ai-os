#ifndef AIOS_GPT2_TOKENIZER_H
#define AIOS_GPT2_TOKENIZER_H

#include <stdint.h>

/* Tokenizer binary format compatible with the lightweight llm.c decoder. */
int gpt2_tokenizer_load_from_initrd(const char* path);
int gpt2_tokenizer_encode_ascii(const char* text, uint32_t* out_tokens,
                                uint32_t max_tokens, uint32_t* out_count);
const char* gpt2_tokenizer_decode(uint32_t token_id);
uint32_t gpt2_tokenizer_eot(void);
const char* gpt2_tokenizer_status(void);

#endif
