#include "gpt2_infer.h"
#include "gpt2_model.h"
#include "../mem/heap.h"

#define GPT2_BAREMETAL_MAX_CONTEXT 64U

typedef struct {
    const float* wte;
    const float* wpe;
    const float* ln1w;
    const float* ln1b;
    const float* qkvw;
    const float* qkvb;
    const float* attprojw;
    const float* attprojb;
    const float* ln2w;
    const float* ln2b;
    const float* fcw;
    const float* fcb;
    const float* fcprojw;
    const float* fcprojb;
    const float* lnfw;
    const float* lnfb;
} gpt2_params_t;

typedef struct {
    float* residual;
    float* temporary;
    float* normalized;
    float* qkv;
    float* attention;
    float* scores;
    float* hidden;
    float* logits;
    uint32_t context;
    uint32_t channels;
    uint32_t vocab;
    int ready;
} gpt2_workspace_t;

static gpt2_workspace_t workspace;
static const char* infer_status = "GPT-2: moteur CPU non initialise";

static float gpt2_inv_sqrt(float value) {
    union { float f; uint32_t u; } convert;
    float half = 0.5f * value;
    convert.f = value;
    convert.u = 0x5f3759dfU - (convert.u >> 1);
    convert.f = convert.f * (1.5f - half * convert.f * convert.f);
    return convert.f;
}

static float gpt2_fast_exp(float value) {
    union { float f; uint32_t u; } convert;
    if (value < -80.0f) return 0.0f;
    if (value > 80.0f) value = 80.0f;
    convert.u = (uint32_t)(12102203.0f * value + 1064866805.0f);
    return convert.f;
}

static float gpt2_gelu(float value) {
    float x3 = value * value * value;
    float inner = 0.7978845608f * (value + 0.044715f * x3);
    float inner2 = inner * inner;
    float tanh_approx = inner * (27.0f + inner2) / (27.0f + 9.0f * inner2);
    return 0.5f * value * (1.0f + tanh_approx);
}

static void gpt2_map_params(gpt2_params_t* params, const gpt2_model_t* model) {
    const gpt2_config_t* cfg = &model->config;
    uint64_t sizes[16];
    uint64_t c = cfg->channels;
    uint64_t l = cfg->num_layers;
    uint64_t t = cfg->max_seq_len;
    uint64_t v = cfg->padded_vocab_size;
    const float* cursor = model->weights;

    sizes[0] = v * c;
    sizes[1] = t * c;
    sizes[2] = l * c;
    sizes[3] = l * c;
    sizes[4] = l * (3U * c) * c;
    sizes[5] = l * (3U * c);
    sizes[6] = l * c * c;
    sizes[7] = l * c;
    sizes[8] = l * c;
    sizes[9] = l * c;
    sizes[10] = l * (4U * c) * c;
    sizes[11] = l * (4U * c);
    sizes[12] = l * c * (4U * c);
    sizes[13] = l * c;
    sizes[14] = c;
    sizes[15] = c;

    params->wte = cursor; cursor += sizes[0];
    params->wpe = cursor; cursor += sizes[1];
    params->ln1w = cursor; cursor += sizes[2];
    params->ln1b = cursor; cursor += sizes[3];
    params->qkvw = cursor; cursor += sizes[4];
    params->qkvb = cursor; cursor += sizes[5];
    params->attprojw = cursor; cursor += sizes[6];
    params->attprojb = cursor; cursor += sizes[7];
    params->ln2w = cursor; cursor += sizes[8];
    params->ln2b = cursor; cursor += sizes[9];
    params->fcw = cursor; cursor += sizes[10];
    params->fcb = cursor; cursor += sizes[11];
    params->fcprojw = cursor; cursor += sizes[12];
    params->fcprojb = cursor; cursor += sizes[13];
    params->lnfw = cursor; cursor += sizes[14];
    params->lnfb = cursor;
}

static void gpt2_layernorm(float* out, const float* input, const float* weight,
                           const float* bias, uint32_t tokens, uint32_t channels) {
    for (uint32_t token = 0; token < tokens; token++) {
        const float* in = input + token * channels;
        float* dst = out + token * channels;
        float mean = 0.0f;
        float variance = 0.0f;
        for (uint32_t i = 0; i < channels; i++) mean += in[i];
        mean /= (float)channels;
        for (uint32_t i = 0; i < channels; i++) {
            float delta = in[i] - mean;
            variance += delta * delta;
        }
        variance /= (float)channels;
        float scale = gpt2_inv_sqrt(variance + 0.00001f);
        for (uint32_t i = 0; i < channels; i++) {
            dst[i] = (in[i] - mean) * scale * weight[i] + bias[i];
        }
    }
}

static void gpt2_matmul(float* out, const float* input, const float* weight,
                        const float* bias, uint32_t tokens, uint32_t input_size,
                        uint32_t output_size) {
    for (uint32_t token = 0; token < tokens; token++) {
        for (uint32_t output = 0; output < output_size; output++) {
            float value = bias ? bias[output] : 0.0f;
            const float* row = weight + output * input_size;
            const float* src = input + token * input_size;
            for (uint32_t input_index = 0; input_index < input_size; input_index++) {
                value += src[input_index] * row[input_index];
            }
            out[token * output_size + output] = value;
        }
    }
}

static void gpt2_attention(float* out, float* scores, const float* qkv,
                           uint32_t tokens, uint32_t channels, uint32_t heads) {
    uint32_t head_size = channels / heads;
    float attention_scale = gpt2_inv_sqrt((float)head_size);

    for (uint32_t head = 0; head < heads; head++) {
        for (uint32_t token = 0; token < tokens; token++) {
            const float* query = qkv + token * (3U * channels) + head * head_size;
            float max_score = -3.4e38f;
            for (uint32_t previous = 0; previous <= token; previous++) {
                const float* key = qkv + previous * (3U * channels) + channels + head * head_size;
                float score = 0.0f;
                for (uint32_t i = 0; i < head_size; i++) score += query[i] * key[i];
                score *= attention_scale;
                scores[previous] = score;
                if (score > max_score) max_score = score;
            }
            float sum = 0.0f;
            for (uint32_t previous = 0; previous <= token; previous++) {
                scores[previous] = gpt2_fast_exp(scores[previous] - max_score);
                sum += scores[previous];
            }
            float* destination = out + token * channels + head * head_size;
            for (uint32_t i = 0; i < head_size; i++) destination[i] = 0.0f;
            for (uint32_t previous = 0; previous <= token; previous++) {
                const float* value = qkv + previous * (3U * channels) + 2U * channels + head * head_size;
                float probability = scores[previous] / sum;
                for (uint32_t i = 0; i < head_size; i++) destination[i] += probability * value[i];
            }
        }
    }
}

static void gpt2_add(float* out, const float* left, const float* right, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) out[i] = left[i] + right[i];
}

static int gpt2_workspace_init(const gpt2_config_t* cfg) {
    uint32_t c = cfg->channels;
    uint32_t v = cfg->padded_vocab_size;
    uint32_t hidden = 4U * c;
    uint32_t tokens = GPT2_BAREMETAL_MAX_CONTEXT;

    if (workspace.ready && workspace.context == tokens && workspace.channels == c && workspace.vocab == v) {
        return 0;
    }
    if (workspace.ready) {
        infer_status = "GPT-2: changement de configuration non supporte pendant la session";
        return -1;
    }

    workspace.residual = (float*)kmalloc(tokens * c * sizeof(float));
    workspace.temporary = (float*)kmalloc(tokens * c * sizeof(float));
    workspace.normalized = (float*)kmalloc(tokens * c * sizeof(float));
    workspace.qkv = (float*)kmalloc(tokens * 3U * c * sizeof(float));
    workspace.attention = (float*)kmalloc(tokens * c * sizeof(float));
    workspace.scores = (float*)kmalloc(tokens * sizeof(float));
    workspace.hidden = (float*)kmalloc(tokens * hidden * sizeof(float));
    workspace.logits = (float*)kmalloc(v * sizeof(float));
    if (!workspace.residual || !workspace.temporary || !workspace.normalized ||
        !workspace.qkv || !workspace.attention || !workspace.scores ||
        !workspace.hidden || !workspace.logits) {
        infer_status = "GPT-2: memoire insuffisante pour les activations";
        return -2;
    }
    workspace.context = tokens;
    workspace.channels = c;
    workspace.vocab = v;
    workspace.ready = 1;
    return 0;
}

/* Selection top-k avec temperature et penalite pour les jetons du contexte recent. */
static uint32_t gpt2_sample_top_k(const float* logits, uint32_t vocab, const uint32_t* history,
                                  uint32_t history_count, uint32_t* rng_state) {
    uint32_t ids[8];
    float values[8];
    float weights[8];
    uint32_t count = 0;
    uint32_t state = rng_state && *rng_state ? *rng_state : 0x9e3779b9U;

    for (uint32_t i = 0; i < vocab; i++) {
        float value = logits[i];
        uint32_t begin = history_count > 16U ? history_count - 16U : 0U;
        for (uint32_t j = begin; j < history_count; j++) {
            if (history[j] == i) value -= 1.35f;
        }
        uint32_t pos = count < 8U ? count++ : 8U;
        while (pos > 0U && value > values[pos - 1U]) {
            if (pos < 8U) { values[pos] = values[pos - 1U]; ids[pos] = ids[pos - 1U]; }
            pos--;
        }
        if (pos < 8U) { values[pos] = value; ids[pos] = i; }
    }
    if (count == 0U) return 0U;

    float total = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        weights[i] = gpt2_fast_exp((values[i] - values[0]) / 1.10f);
        total += weights[i];
    }
    state = state * 1664525U + 1013904223U;
    if (rng_state) *rng_state = state;
    float target = ((float)(state & 0x00ffffffU) / 16777216.0f) * total;
    for (uint32_t i = 0; i < count; i++) {
        if (target <= weights[i]) return ids[i];
        target -= weights[i];
    }
    return ids[count - 1U];
}

static int gpt2_generate_next_impl(const uint32_t* tokens, uint32_t token_count,
                                   uint32_t* next_token, int sampled, uint32_t* rng_state) {
    const gpt2_model_t* model = gpt2_model_current();
    gpt2_params_t params;
    const gpt2_config_t* cfg;
    uint32_t c;
    uint32_t best_token = 0;
    float best_logit = -3.4e38f;

    if (!tokens || !next_token || !model->ready) {
        infer_status = "GPT-2: checkpoint local indisponible";
        return -1;
    }
    cfg = &model->config;
    if (token_count == 0 || token_count > GPT2_BAREMETAL_MAX_CONTEXT || token_count > cfg->max_seq_len) {
        infer_status = "GPT-2: taille de contexte non supportee";
        return -2;
    }
    for (uint32_t i = 0; i < token_count; i++) {
        if (tokens[i] >= cfg->vocab_size) {
            infer_status = "GPT-2: identifiant de jeton invalide";
            return -3;
        }
    }
    if (gpt2_workspace_init(cfg) != 0) return -4;

    gpt2_map_params(&params, model);
    c = cfg->channels;
    for (uint32_t token = 0; token < token_count; token++) {
        const float* token_embedding = params.wte + tokens[token] * c;
        const float* position_embedding = params.wpe + token * c;
        float* destination = workspace.residual + token * c;
        for (uint32_t i = 0; i < c; i++) destination[i] = token_embedding[i] + position_embedding[i];
    }

    for (uint32_t layer = 0; layer < cfg->num_layers; layer++) {
        const float* ln1w = params.ln1w + layer * c;
        const float* ln1b = params.ln1b + layer * c;
        const float* qkvw = params.qkvw + layer * (3U * c) * c;
        const float* qkvb = params.qkvb + layer * (3U * c);
        const float* attprojw = params.attprojw + layer * c * c;
        const float* attprojb = params.attprojb + layer * c;
        const float* ln2w = params.ln2w + layer * c;
        const float* ln2b = params.ln2b + layer * c;
        const float* fcw = params.fcw + layer * (4U * c) * c;
        const float* fcb = params.fcb + layer * (4U * c);
        const float* fcprojw = params.fcprojw + layer * c * (4U * c);
        const float* fcprojb = params.fcprojb + layer * c;

        gpt2_layernorm(workspace.normalized, workspace.residual, ln1w, ln1b, token_count, c);
        gpt2_matmul(workspace.qkv, workspace.normalized, qkvw, qkvb, token_count, c, 3U * c);
        gpt2_attention(workspace.attention, workspace.scores, workspace.qkv, token_count, c, cfg->num_heads);
        gpt2_matmul(workspace.temporary, workspace.attention, attprojw, attprojb, token_count, c, c);
        gpt2_add(workspace.temporary, workspace.residual, workspace.temporary, token_count * c);
        gpt2_layernorm(workspace.normalized, workspace.temporary, ln2w, ln2b, token_count, c);
        gpt2_matmul(workspace.hidden, workspace.normalized, fcw, fcb, token_count, c, 4U * c);
        for (uint32_t i = 0; i < token_count * 4U * c; i++) workspace.hidden[i] = gpt2_gelu(workspace.hidden[i]);
        gpt2_matmul(workspace.residual, workspace.hidden, fcprojw, fcprojb, token_count, 4U * c, c);
        gpt2_add(workspace.residual, workspace.temporary, workspace.residual, token_count * c);
    }

    gpt2_layernorm(workspace.normalized, workspace.residual, params.lnfw, params.lnfb, token_count, c);
    const float* final_state = workspace.normalized + (token_count - 1U) * c;
    for (uint32_t token = 0; token < cfg->vocab_size; token++) {
        const float* embedding = params.wte + token * c;
        float logit = 0.0f;
        for (uint32_t i = 0; i < c; i++) logit += final_state[i] * embedding[i];
        workspace.logits[token] = logit;
        if (logit > best_logit) {
            best_logit = logit;
            best_token = token;
        }
    }

    if (sampled) {
        *next_token = gpt2_sample_top_k(workspace.logits, cfg->vocab_size, tokens, token_count, rng_state);
        infer_status = "GPT-2: jeton echantillonne localement (top-k)";
    } else {
        *next_token = best_token;
        infer_status = "GPT-2: prochain jeton genere localement";
    }
    return 0;
}

int gpt2_generate_next(const uint32_t* tokens, uint32_t token_count, uint32_t* next_token) {
    return gpt2_generate_next_impl(tokens, token_count, next_token, 0, 0);
}

int gpt2_generate_next_sampled(const uint32_t* tokens, uint32_t token_count,
                               uint32_t* next_token, uint32_t* rng_state) {
    return gpt2_generate_next_impl(tokens, token_count, next_token, 1, rng_state);
}

const char* gpt2_infer_status(void) {
    return infer_status;
}
