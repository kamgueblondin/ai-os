#include "../../framework/unity.h"
#include "../../../kernel/net_llm_socket.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static void init_socket_session(int* socket_id, net_tls_aes_gcm_session_t* session,
                                uint8_t key_material[40]) {
    net_tls_aes128_gcm_key_block_t block;
    net_tcp_view_t syn_ack;
    uint8_t i;
    for (i = 0U; i < 40U; i++) key_material[i] = (uint8_t)(i + 1U);
    block = (net_tls_aes128_gcm_key_block_t){key_material, key_material + 16U,
                                             key_material + 32U, key_material + 36U};
    TEST_ASSERT_EQUAL(0, net_tls_aes_gcm_session_init(session, &block, 1U));
    net_socket_reset_all();
    *socket_id = net_socket_open(49152U, 443U, 100U);
    TEST_ASSERT_TRUE(*socket_id >= 0);
    syn_ack = (net_tcp_view_t){443U, 49152U, 700U, 101U,
                                NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK, 0, 0};
    TEST_ASSERT_EQUAL(0, net_socket_accept_syn_ack(*socket_id, &syn_ack));
}

void test_llm_socket_builds_openai_stream_request(void) {
    int socket_id;
    net_tls_aes_gcm_session_t session;
    uint8_t key_material[40], json[256] = {0}, request[512] = {0};
    uint8_t record[640] = {0}, segment[700] = {0};
    net_tcp_view_t view;
    int built;

    init_socket_session(&socket_id, &session, key_material);
    built = net_llm_socket_build_request(socket_id, &session,
                                         NET_LLM_SOCKET_PROVIDER_OPENAI, 1U,
                                         json, sizeof(json), request, sizeof(request),
                                         "api.example.test", "/v1/chat/completions", "sk-test",
                                         "gpt-4o-mini", (const uint8_t*)"bonjour", 7U,
                                         record, sizeof(record), segment, sizeof(segment), 2U);
    TEST_ASSERT_GREATER_THAN(20U, built);
    TEST_ASSERT_NOT_NULL(strstr((const char*)json, "\"stream\":true"));
    TEST_ASSERT_NOT_NULL(strstr((const char*)request, "Authorization: Bearer sk-test\r\n"));
    TEST_ASSERT_NOT_NULL(strstr((const char*)request, "POST /v1/chat/completions HTTP/1.1\r\n"));
    TEST_ASSERT_EQUAL(1U, session.write_sequence);
    TEST_ASSERT_EQUAL(0, net_tcp_parse(segment, (uint16_t)built, &view));
    TEST_ASSERT_GREATER_THAN(7U, view.payload_length);
    TEST_ASSERT_EQUAL(0, net_socket_close(socket_id));
}

void test_llm_socket_rejects_missing_openai_bearer(void) {
    int socket_id;
    net_tls_aes_gcm_session_t session;
    uint8_t key_material[40], json[64] = {0}, request[128] = {0};
    uint8_t record[128] = {0}, segment[160] = {0};

    init_socket_session(&socket_id, &session, key_material);
    TEST_ASSERT_EQUAL(-3, net_llm_socket_build_request(socket_id, &session,
                                                         NET_LLM_SOCKET_PROVIDER_OPENAI, 0U,
                                                         json, sizeof(json), request, sizeof(request),
                                                         "api.example.test", "/v1/chat", 0,
                                                         "model", (const uint8_t*)"x", 1U,
                                                         record, sizeof(record), segment, sizeof(segment), 1U));
    TEST_ASSERT_EQUAL(0U, session.write_sequence);
    TEST_ASSERT_EQUAL(0, net_socket_close(socket_id));
}

int main(void) {
    unity_init();
    RUN_TEST(test_llm_socket_builds_openai_stream_request);
    RUN_TEST(test_llm_socket_rejects_missing_openai_bearer);
    unity_print_results();
    unity_cleanup();
    return unity_stats.tests_failed == 0 ? 0 : 1;
}
