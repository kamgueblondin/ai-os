#include "../../framework/unity.h"
#include "../../../kernel/net_socket.h"

void setUp(void) {}
void tearDown(void) {}

void test_socket_lifecycle_send_receive(void) {
    int id;
    uint8_t segment[64] = {0};
    uint8_t received[8] = {0};
    uint8_t payload[4] = {'P', 'I', 'N', 'G'};
    uint16_t segment_length = 0U, received_length = 0U;
    uint8_t state = 0U;
    net_tcp_view_t view;

    net_socket_reset_all();
    id = net_socket_open(49152U, 443U, 100U);
    TEST_ASSERT_TRUE(id >= 0);
    TEST_ASSERT_EQUAL(0, net_socket_get_state(id, &state));
    TEST_ASSERT_EQUAL(NET_TCP_STATE_SYN_SENT, state);
    view = (net_tcp_view_t){443U, 49152U, 700U, 101U, NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK, 0, 0};
    TEST_ASSERT_EQUAL(0, net_socket_accept_syn_ack(id, &view));
    TEST_ASSERT_EQUAL(0, net_socket_get_state(id, &state));
    TEST_ASSERT_EQUAL(NET_TCP_STATE_ESTABLISHED, state);
    TEST_ASSERT_EQUAL(0, net_socket_send(id, payload, sizeof(payload), segment, sizeof(segment), &segment_length));
    TEST_ASSERT_EQUAL(24U, segment_length);
    TEST_ASSERT_EQUAL(0, net_tcp_parse(segment, segment_length, &view));
    TEST_ASSERT_EQUAL(4U, view.payload_length);
    TEST_ASSERT_EQUAL('P', view.payload[0]);
    TEST_ASSERT_EQUAL(24, net_tcp_build_data(segment, sizeof(segment), 443U, 49152U,
                                              701U, 104U, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL(0, net_socket_feed(id, segment, 24U));
    TEST_ASSERT_EQUAL(0, net_socket_receive(id, received, sizeof(received), &received_length));
    TEST_ASSERT_EQUAL(4U, received_length);
    TEST_ASSERT_EQUAL('P', received[0]);
    TEST_ASSERT_EQUAL('G', received[3]);
    TEST_ASSERT_EQUAL(0, net_socket_close(id));
    TEST_ASSERT_EQUAL(NET_SOCKET_NOT_OPEN, net_socket_get_state(id, &state));
}

int main(void) {
    unity_init();
    RUN_TEST(test_socket_lifecycle_send_receive);
    unity_print_results();
    unity_cleanup();
    return 0;
}
