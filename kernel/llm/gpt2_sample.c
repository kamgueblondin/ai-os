#include "gpt2_sample.h"

#define GPT2_REPEAT_WINDOW 16U
#define GPT2_REPEAT_PENALTY 8.0f
#define GPT2_SAMPLE_TEMPERATURE 1.10f

static float gpt2_fast_exp(float value) {
    union { float f; uint32_t u; } convert;
    if (value < -80.0f) return 0.0f;
    if (value > 80.0f) value = 80.0f;
    convert.u = (uint32_t)(12102203.0f * value + 1064866805.0f);
    return convert.f;
}

uint32_t gpt2_sample_top_k(const float* logits, uint32_t vocab, const uint32_t* history,
                           uint32_t history_count, uint32_t* rng_state) {
    uint32_t ids[GPT2_SAMPLE_TOP_K];
    float values[GPT2_SAMPLE_TOP_K];
    float weights[GPT2_SAMPLE_TOP_K];
    uint32_t count = 0;
    uint32_t state = rng_state && *rng_state ? *rng_state : 0x9e3779b9U;
    uint32_t banned = (history && history_count > 0U) ? history[history_count - 1U] : 0xFFFFFFFFu;

    if (!logits || vocab == 0U) return 0U;

    for (uint32_t i = 0; i < vocab; i++) {
        float value;
        uint32_t begin;
        uint32_t repeats = 0;
        uint32_t pos;

        if (i == banned) continue;
        value = logits[i];
        begin = history_count > GPT2_REPEAT_WINDOW ? history_count - GPT2_REPEAT_WINDOW : 0U;
        if (history) {
            for (uint32_t j = begin; j < history_count; j++) {
                if (history[j] == i) repeats++;
            }
        }
        if (repeats > 0U) value -= GPT2_REPEAT_PENALTY * (float)repeats;
        pos = count < GPT2_SAMPLE_TOP_K ? count++ : GPT2_SAMPLE_TOP_K;
        while (pos > 0U && value > values[pos - 1U]) {
            if (pos < GPT2_SAMPLE_TOP_K) {
                values[pos] = values[pos - 1U];
                ids[pos] = ids[pos - 1U];
            }
            pos--;
        }
        if (pos < GPT2_SAMPLE_TOP_K) {
            values[pos] = value;
            ids[pos] = i;
        }
    }
    if (count == 0U) return banned == 0xFFFFFFFFu ? 0U : banned;

    {
        float total = 0.0f;
        float target;
        for (uint32_t i = 0; i < count; i++) {
            weights[i] = gpt2_fast_exp((values[i] - values[0]) / GPT2_SAMPLE_TEMPERATURE);
            total += weights[i];
        }
        state = state * 1664525U + 1013904223U;
        if (rng_state) *rng_state = state;
        target = ((float)(state & 0x00ffffffU) / 16777216.0f) * total;
        for (uint32_t i = 0; i < count; i++) {
            if (target <= weights[i]) return ids[i];
            target -= weights[i];
        }
        return ids[count - 1U];
    }
}
