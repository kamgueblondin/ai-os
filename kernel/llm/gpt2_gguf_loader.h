#ifndef AIOS_GPT2_GGUF_LOADER_H
#define AIOS_GPT2_GGUF_LOADER_H

#include "gpt2_gguf.h"
#include "gpt2_quant.h"
#include "../fs/fat16.h"

typedef struct {
    gpt2_gguf_index_t index;
    uint32_t bytes_loaded;
} gpt2_gguf_loaded_model_t;

/* Lit un fichier FAT16 8.3 dans le buffer fourni puis indexe son GGUF. */
int gpt2_gguf_load_fat16(const fat16_volume_t* volume, const char* filename,
                         uint8_t* buffer, uint32_t capacity,
                         gpt2_gguf_loaded_model_t* out);
/* Lit une fenêtre relative aux données d’un tenseur déjà indexé. */
int gpt2_gguf_read_tensor_fat16(const fat16_volume_t* volume, const char* filename,
                                const gpt2_gguf_loaded_model_t* model,
                                const gpt2_gguf_tensor_t* tensor,
                                uint32_t tensor_offset, uint8_t* buffer,
                                uint32_t capacity, uint32_t* out_read);
/* Lit un super-bloc complet d’un tenseur Q3_K/Q4_K/Q6_K. */
int gpt2_gguf_read_quant_block_fat16(const fat16_volume_t* volume, const char* filename,
                                     const gpt2_gguf_loaded_model_t* model,
                                     const gpt2_gguf_tensor_t* tensor,
                                     uint32_t block_index, uint8_t* buffer,
                                     uint32_t capacity, uint32_t* out_read);

#endif
