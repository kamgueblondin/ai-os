#ifndef AIOS_GPT2_GGUF_LOADER_H
#define AIOS_GPT2_GGUF_LOADER_H

#include "gpt2_gguf.h"
#include "gpt2_quant.h"
#include "../fs/fat16.h"

typedef struct {
    gpt2_gguf_index_t index;
    uint32_t bytes_loaded;
} gpt2_gguf_loaded_model_t;

typedef struct {
    const gpt2_gguf_loaded_model_t* model;
    gpt2_gguf_layer_t layer;
    uint8_t* scratch;
    uint32_t scratch_capacity;
    uint32_t channels;
    uint32_t position;
} gpt2_gguf_forward_context_t;

/* Prépare une couche pour le futur forward sans prendre possession des buffers. */
int gpt2_gguf_forward_context_init(const gpt2_gguf_loaded_model_t* model,
                                   uint32_t layer_index, uint32_t channels,
                                   uint32_t position, char* name, uint32_t name_capacity,
                                   uint8_t* scratch, uint32_t scratch_capacity,
                                   gpt2_gguf_forward_context_t* out);

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
/* Lit puis exécute un super-bloc quantifié avec un scratch caller-owned. */
int gpt2_gguf_dot_quant_block_fat16(const fat16_volume_t* volume, const char* filename,
                                    const gpt2_gguf_loaded_model_t* model,
                                    const gpt2_gguf_tensor_t* tensor,
                                    uint32_t block_index, const float* input,
                                    uint32_t count, uint8_t* scratch,
                                    uint32_t scratch_capacity, float* out_dot);
/* Accumule plusieurs super-blocs sans charger le tenseur complet. */
int gpt2_gguf_dot_quant_tensor_fat16(const fat16_volume_t* volume, const char* filename,
                                     const gpt2_gguf_loaded_model_t* model,
                                     const gpt2_gguf_tensor_t* tensor,
                                     const float* input, uint32_t count,
                                     uint8_t* scratch, uint32_t scratch_capacity,
                                     float* out_dot);
/* Calcule une ligne `shape[0]` d’un tenseur 1D ou 2D. */
int gpt2_gguf_dot_quant_row_fat16(const fat16_volume_t* volume, const char* filename,
                                  const gpt2_gguf_loaded_model_t* model,
                                  const gpt2_gguf_tensor_t* tensor,
                                  uint32_t row_index, const float* input,
                                  uint32_t count, uint8_t* scratch,
                                  uint32_t scratch_capacity, float* out_dot);
/* Lit les octets d’une ligne quantifiée sans allocation ni chargement complet. */
int gpt2_gguf_read_quant_row_fat16(const fat16_volume_t* volume, const char* filename,
                                   const gpt2_gguf_loaded_model_t* model,
                                   const gpt2_gguf_tensor_t* tensor,
                                   uint32_t row_index, uint8_t* buffer,
                                   uint32_t capacity, uint32_t* out_read);

#endif
