/* test_gpt2_sample.c - top-k sampling without a 124M checkpoint */

#include "../../framework/unity.h"
#include "../../../kernel/llm/gpt2_sample.h"

#define SAMPLE_VOCAB 16U

static void fill_the_loop_logits(float* logits) {
    uint32_t i;
    for (i = 0; i < SAMPLE_VOCAB; i++) logits[i] = 0.0f;
    /* Token 3 stands in for GPT-2 262 (" The"): far above the rest. */
    logits[3] = 20.0f;
    logits[5] = 12.0f;
    logits[7] = 11.0f;
}

static void test_sample_bans_previous_the_token(void) {
    float logits[SAMPLE_VOCAB];
    uint32_t history[4];
    uint32_t rng = 1U;
    uint32_t picked;
    uint32_t i;

    fill_the_loop_logits(logits);
    history[0] = 1;
    history[1] = 2;
    history[2] = 3;
    picked = gpt2_sample_top_k(logits, SAMPLE_VOCAB, history, 3, &rng);
    TEST_ASSERT_NOT_EQUAL(3, (int)picked);
    TEST_ASSERT_TRUE(picked < SAMPLE_VOCAB);
    for (i = 0; i < 12U; i++) {
        rng = i + 1U;
        picked = gpt2_sample_top_k(logits, SAMPLE_VOCAB, history, 3, &rng);
        TEST_ASSERT_NOT_EQUAL(3, (int)picked);
    }
}

static void test_sample_picks_peak_without_history(void) {
    float logits[SAMPLE_VOCAB];
    uint32_t rng = 1U;
    uint32_t picked;

    fill_the_loop_logits(logits);
    picked = gpt2_sample_top_k(logits, SAMPLE_VOCAB, 0, 0, &rng);
    TEST_ASSERT_EQUAL(3, (int)picked);
}

static void test_sample_penalizes_repeated_history(void) {
    float logits[SAMPLE_VOCAB];
    uint32_t history[5];
    uint32_t rng = 1U;
    uint32_t picked;

    fill_the_loop_logits(logits);
    history[0] = 3;
    history[1] = 3;
    history[2] = 3;
    history[3] = 3;
    history[4] = 7;
    picked = gpt2_sample_top_k(logits, SAMPLE_VOCAB, history, 5, &rng);
    TEST_ASSERT_NOT_EQUAL(3, (int)picked);
    TEST_ASSERT_NOT_EQUAL(7, (int)picked);
    TEST_ASSERT_EQUAL(5, (int)picked);
}

static void test_sample_same_seed_is_deterministic(void) {
    float logits[SAMPLE_VOCAB];
    uint32_t history[1];
    uint32_t rng_a = 42U;
    uint32_t rng_b = 42U;
    uint32_t first;
    uint32_t second;

    fill_the_loop_logits(logits);
    history[0] = 3;
    first = gpt2_sample_top_k(logits, SAMPLE_VOCAB, history, 1, &rng_a);
    second = gpt2_sample_top_k(logits, SAMPLE_VOCAB, history, 1, &rng_b);
    TEST_ASSERT_EQUAL((int)first, (int)second);
    TEST_ASSERT_NOT_EQUAL(3, (int)first);
}

int main(void) {
    unity_init();
    RUN_TEST(test_sample_bans_previous_the_token);
    RUN_TEST(test_sample_picks_peak_without_history);
    RUN_TEST(test_sample_penalizes_repeated_history);
    RUN_TEST(test_sample_same_seed_is_deterministic);
    unity_print_results();
    unity_cleanup();
    return (unity_stats.tests_failed == 0) ? 0 : 1;
}
