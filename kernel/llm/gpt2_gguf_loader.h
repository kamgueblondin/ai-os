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

typedef struct {
    float* storage;
    uint32_t layers;
    uint32_t max_positions;
    uint32_t channels;
    uint32_t count;
} gpt2_gguf_kv_cache_t;

/* Prépare une couche pour le futur forward sans prendre possession des buffers. */
int gpt2_gguf_forward_context_init(const gpt2_gguf_loaded_model_t* model,
                                   uint32_t layer_index, uint32_t channels,
                                   uint32_t position, char* name, uint32_t name_capacity,
                                   uint8_t* scratch, uint32_t scratch_capacity,
                                   gpt2_gguf_forward_context_t* out);
/* Initialise un cache KV fourni par l’appelant. */
int gpt2_gguf_kv_cache_init(float* storage, uint32_t storage_floats,
                            uint32_t layers, uint32_t max_positions,
                            uint32_t channels, gpt2_gguf_kv_cache_t* out);
/* Écrit les K/V d’une couche et d’une position dans le cache. */
int gpt2_gguf_kv_cache_put(gpt2_gguf_kv_cache_t* cache, uint32_t layer,
                           uint32_t position, const float* key, const float* value);
/* Relit les K/V d’une couche et d’une position dans les buffers appelant. */
int gpt2_gguf_kv_cache_get(const gpt2_gguf_kv_cache_t* cache, uint32_t layer,
                           uint32_t position, float* key, float* value);
/* Copie un intervalle de positions historiques d’une couche. */
int gpt2_gguf_kv_cache_copy_history(const gpt2_gguf_kv_cache_t* cache, uint32_t layer,
                                    uint32_t start_position, uint32_t position_count,
                                    float* key_out, uint32_t key_capacity,
                                    float* value_out, uint32_t value_capacity,
                                    uint32_t* out_count);
/* Calcule les scores query-key bruts sur un historique d’une couche. */
int gpt2_gguf_kv_cache_query_scores(const gpt2_gguf_kv_cache_t* cache, uint32_t layer,
                                    uint32_t start_position, uint32_t position_count,
                                    const float* query, float* key_scratch,
                                    uint32_t key_scratch_capacity, float* scores,
                                    uint32_t score_capacity, uint32_t* out_count);
/* Accumule les values historiques avec les poids fournis par l’appelant. */
int gpt2_gguf_kv_cache_accumulate_values(const gpt2_gguf_kv_cache_t* cache, uint32_t layer,
                                         uint32_t start_position, uint32_t position_count,
                                         const float* weights, uint32_t weight_capacity,
                                         float* output, uint32_t output_capacity,
                                         uint32_t* out_count);
/* Met les scores query-key à l’échelle par l’inverse de sqrt(head_size). */
int gpt2_gguf_attention_scale_scores(float* scores, uint32_t score_count, uint32_t head_size);
/* Transforme des scores en probabilités par softmax stable, sans allocation. */
int gpt2_gguf_attention_softmax(float* scores, uint32_t score_count, uint32_t* out_count);
/* Exécute l’attention autoregressive complète pour une tête du cache KV. */
int gpt2_gguf_kv_cache_attention_head(const gpt2_gguf_kv_cache_t* cache, uint32_t layer,
                                      uint32_t start_position, uint32_t position_count,
                                      const float* query, uint32_t head_count,
                                      uint32_t head_index, float* key_scratch,
                                      uint32_t key_scratch_capacity, float* scores,
                                      uint32_t score_capacity, float* output,
                                      uint32_t output_capacity, uint32_t* out_count);
/* Concatène les sorties de têtes dans le vecteur d’attention caller-owned. */
int gpt2_gguf_attention_concat_heads(const float* head_outputs, uint32_t head_count,
                                      uint32_t head_size, float* output,
                                      uint32_t output_capacity, uint32_t* out_count);
/* Exécute l’attention complète de toutes les têtes puis concatène les sorties. */
int gpt2_gguf_kv_cache_attention_multi_head(const gpt2_gguf_kv_cache_t* cache, uint32_t layer,
                                            uint32_t start_position, uint32_t position_count,
                                            const float* query, uint32_t head_count,
                                            float* head_outputs, uint32_t head_output_capacity,
                                            float* key_scratch, uint32_t key_scratch_capacity,
                                            float* scores, uint32_t score_capacity,
                                            float* output, uint32_t output_capacity,
                                            uint32_t* out_count);

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
/* Calcule une ligne quantifiée déjà présente dans un buffer caller-owned. */
int gpt2_gguf_dot_quant_row_buffer(const gpt2_gguf_tensor_t* tensor,
                                   const uint8_t* row_buffer, uint32_t row_capacity,
                                   const float* input, uint32_t count, float* out_dot);
/* Lit et calcule une sortie QKV unique d’une matrice GGUF `[C,3C]`. */
int gpt2_gguf_project_qkv_row_fat16(const fat16_volume_t* volume, const char* filename,
                                    const gpt2_gguf_loaded_model_t* model,
                                    const gpt2_gguf_tensor_t* tensor, uint32_t channels,
                                    uint32_t output_index, const float* input,
                                    uint8_t* row_buffer, uint32_t row_capacity,
                                    float* out_value);
/* Accumule les 3C sorties dans trois buffers query/key/value caller-owned. */
int gpt2_gguf_project_qkv_fat16(const fat16_volume_t* volume, const char* filename,
                                const gpt2_gguf_loaded_model_t* model,
                                const gpt2_gguf_tensor_t* tensor, uint32_t channels,
                                const float* input, uint8_t* row_buffer,
                                uint32_t row_capacity, float* query, uint32_t query_capacity,
                                float* key, uint32_t key_capacity,
                                float* value, uint32_t value_capacity);
/* Projette un vecteur dans une matrice GGUF quantifiée `[input, output]`. */
int gpt2_gguf_project_matrix_fat16(const fat16_volume_t* volume, const char* filename,
                                   const gpt2_gguf_loaded_model_t* model,
                                   const gpt2_gguf_tensor_t* tensor,
                                   uint32_t input_channels, uint32_t output_channels,
                                   const float* input, uint8_t* row_buffer,
                                   uint32_t row_capacity, float* output,
                                   uint32_t output_capacity);

#endif
