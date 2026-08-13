#include "gpt2_tokenizer.h"
#include "../../fs/initrd.h"

#define GPT2_TOKENIZER_MAGIC 20240328U
#define GPT2_TOKENIZER_MAX_VOCAB 50257U

typedef struct {
    const uint8_t* bytes;
    uint8_t length;
} gpt2_token_piece_t;

static gpt2_token_piece_t token_table[GPT2_TOKENIZER_MAX_VOCAB];
static uint32_t tokenizer_vocab_size;
static uint32_t tokenizer_eot;
static int tokenizer_ready;
static char decoded_piece[256];
static const char* tokenizer_status = "GPT-2 tokenizer: aucun vocabulaire charge";

static void tokenizer_reset(const char* status) {
    tokenizer_vocab_size = 0;
    tokenizer_eot = 0;
    tokenizer_ready = 0;
    tokenizer_status = status;
}

int gpt2_tokenizer_load_from_initrd(const char* path) {
    const uint8_t* blob = (const uint8_t*)initrd_read_file(path);
    uint32_t blob_size = initrd_get_file_size(path);
    const uint32_t* header;
    uint32_t offset = 1024;

    tokenizer_reset("GPT-2 tokenizer: verification du vocabulaire");
    if (!blob || blob_size < 1024) {
        tokenizer_reset("GPT-2 tokenizer: fichier absent de l'initrd");
        return -1;
    }
    header = (const uint32_t*)blob;
    if (header[0] != GPT2_TOKENIZER_MAGIC || (header[1] != 1U && header[1] != 2U)) {
        tokenizer_reset("GPT-2 tokenizer: en-tete non supporte");
        return -2;
    }
    if (header[2] == 0 || header[2] > GPT2_TOKENIZER_MAX_VOCAB) {
        tokenizer_reset("GPT-2 tokenizer: taille de vocabulaire invalide");
        return -3;
    }

    tokenizer_vocab_size = header[2];
    tokenizer_eot = header[1] == 1U ? 50256U : header[3];
    for (uint32_t token = 0; token < tokenizer_vocab_size; token++) {
        uint8_t length;
        if (offset >= blob_size) {
            tokenizer_reset("GPT-2 tokenizer: vocabulaire tronque");
            return -4;
        }
        length = blob[offset++];
        if (length == 0 || offset + length > blob_size) {
            tokenizer_reset("GPT-2 tokenizer: entree de vocabulaire invalide");
            return -5;
        }
        token_table[token].bytes = blob + offset;
        token_table[token].length = length;
        offset += length;
    }
    tokenizer_ready = 1;
    tokenizer_status = "GPT-2 tokenizer: vocabulaire local valide";
    return 0;
}

int gpt2_tokenizer_encode_ascii(const char* text, uint32_t* out_tokens,
                                uint32_t max_tokens, uint32_t* out_count) {
    uint32_t position = 0;
    uint32_t count = 0;
    if (!text || !out_tokens || !out_count || !tokenizer_ready) return -1;

    while (text[position] != '\0') {
        uint32_t best_token = tokenizer_eot;
        uint32_t best_length = 0;
        for (uint32_t token = 0; token < tokenizer_vocab_size; token++) {
            uint32_t length = token_table[token].length;
            if (length <= best_length) continue;
            uint32_t matched = 1;
            for (uint32_t i = 0; i < length; i++) {
                if (text[position + i] == '\0' ||
                    (uint8_t)text[position + i] != token_table[token].bytes[i]) {
                    matched = 0;
                    break;
                }
            }
            if (matched) {
                best_token = token;
                best_length = length;
            }
        }
        if (best_length == 0) {
            tokenizer_status = "GPT-2 tokenizer: octet non representable";
            return -2;
        }
        if (count >= max_tokens) {
            tokenizer_status = "GPT-2 tokenizer: contexte trop long";
            return -3;
        }
        out_tokens[count++] = best_token;
        position += best_length;
    }
    if (count == 0) {
        if (max_tokens == 0) return -3;
        out_tokens[count++] = tokenizer_eot;
    }
    *out_count = count;
    tokenizer_status = "GPT-2 tokenizer: encodage ASCII termine";
    return 0;
}

const char* gpt2_tokenizer_decode(uint32_t token_id) {
    if (!tokenizer_ready || token_id >= tokenizer_vocab_size) return 0;
    uint32_t length = token_table[token_id].length;
    if (length > 255U) length = 255U;
    for (uint32_t i = 0; i < length; i++) {
        uint8_t value = token_table[token_id].bytes[i];
        decoded_piece[i] = (value >= 32U && value <= 126U) ? (char)value : ' ';
    }
    decoded_piece[length] = '\0';
    return decoded_piece;
}

uint32_t gpt2_tokenizer_eot(void) {
    return tokenizer_eot;
}

const char* gpt2_tokenizer_status(void) {
    return tokenizer_status;
}
