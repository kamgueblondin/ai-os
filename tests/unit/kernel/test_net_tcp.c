#include "../../framework/unity.h"
#include "../../../kernel/net_tcp.h"

void setUp(void) {}
void tearDown(void) {}

void test_build_and_parse_syn_ack(void) {
    uint8_t segment[32] = {0}; net_tcp_view_t view;
    TEST_ASSERT_EQUAL(20, net_tcp_build_syn(segment, sizeof(segment), 49152, 443, 0x10203040U));
    TEST_ASSERT_EQUAL(0, net_tcp_parse(segment, 20, &view));
    TEST_ASSERT_EQUAL(49152, view.source_port); TEST_ASSERT_EQUAL(443, view.destination_port);
    TEST_ASSERT_EQUAL(0x10203040U, view.sequence); TEST_ASSERT_EQUAL(NET_TCP_FLAG_SYN, view.flags);
    segment[13] = NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK; segment[8]=0x10; segment[9]=0x20; segment[10]=0x30; segment[11]=0x41;
    TEST_ASSERT_EQUAL(0, net_tcp_parse(segment, 20, &view));
    TEST_ASSERT_EQUAL(NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK, view.flags);
    { uint8_t source[4] = {10,0,2,15}; uint8_t destination[4] = {10,0,2,2}; uint16_t checksum = net_tcp_checksum_ipv4(source, destination, segment, 20); TEST_ASSERT_NOT_EQUAL(0, checksum); TEST_ASSERT_EQUAL(checksum, net_tcp_checksum_ipv4(source, destination, segment, 20)); }
    TEST_ASSERT_EQUAL(0x10203041U, view.acknowledgment);
    { uint32_t remote_sequence = 0U;
      view.source_port = 443; view.destination_port = 49152;
      TEST_ASSERT_EQUAL(0, net_tcp_is_syn_ack_for(&view, 49152, 443, 0x10203040U, &remote_sequence));
      TEST_ASSERT_EQUAL(0x10203040U, remote_sequence);
      view.acknowledgment = 7U; TEST_ASSERT_NOT_EQUAL(0, net_tcp_is_syn_ack_for(&view, 49152, 443, 0x10203040U, &remote_sequence)); }
    segment[0] = 0; segment[1] = 0; TEST_ASSERT_NOT_EQUAL(0, net_tcp_parse(segment, 20, &view));
    { uint8_t packet[48] = {0}; uint8_t src[4] = {10,0,2,15}; uint8_t dst[4] = {10,0,2,2};
      TEST_ASSERT_EQUAL(40, net_tcp_build_syn_ipv4(packet, sizeof(packet), src, dst, 49152, 443, 0x10203040U));
      TEST_ASSERT_EQUAL(0x45, packet[0]); TEST_ASSERT_EQUAL(NET_TCP_PROTOCOL, packet[9]);
      TEST_ASSERT_EQUAL(49152, (packet[20] << 8) | packet[21]); TEST_ASSERT_EQUAL(NET_TCP_FLAG_SYN, packet[33] & 0x3f); }
}

void test_connection_builds_first_ack(void) {
    uint8_t segment[20] = {0}; net_tcp_view_t view; net_tcp_connection_t connection;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    TEST_ASSERT_EQUAL(NET_TCP_STATE_SYN_SENT, connection.state);
    view.source_port = 443; view.destination_port = 49152; view.sequence = 700U;
    view.acknowledgment = 101U; view.flags = NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &view));
    TEST_ASSERT_EQUAL(NET_TCP_STATE_ESTABLISHED, connection.state);
    TEST_ASSERT_EQUAL(701U, connection.remote_sequence);
    TEST_ASSERT_EQUAL(20, net_tcp_connection_build_ack(&connection, segment, sizeof(segment)));
    TEST_ASSERT_EQUAL(0, net_tcp_parse(segment, 20, &view));
    TEST_ASSERT_EQUAL(49152, view.source_port); TEST_ASSERT_EQUAL(443, view.destination_port);
    TEST_ASSERT_EQUAL(101U, view.sequence); TEST_ASSERT_EQUAL(701U, view.acknowledgment);
    TEST_ASSERT_EQUAL(NET_TCP_FLAG_ACK, view.flags);
    view.acknowledgment = 102U;
    TEST_ASSERT_NOT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &view));
}

void test_build_and_parse_ack_payload(void) {
    uint8_t segment[32] = {0}; uint8_t payload[4] = {'P','I','N','G'}; net_tcp_view_t view;
    TEST_ASSERT_EQUAL(24, net_tcp_build_data(segment, sizeof(segment), 49152, 443, 101U, 701U, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL(0, net_tcp_parse(segment, 24, &view));
    TEST_ASSERT_EQUAL(NET_TCP_FLAG_ACK, view.flags); TEST_ASSERT_EQUAL(101U, view.sequence);
    TEST_ASSERT_EQUAL(701U, view.acknowledgment); TEST_ASSERT_EQUAL(4, view.payload_length);
    TEST_ASSERT_EQUAL('P', view.payload[0]); TEST_ASSERT_EQUAL('G', view.payload[3]);
    TEST_ASSERT_NOT_EQUAL(0, net_tcp_build_data(segment, 23, 49152, 443, 101U, 701U, payload, sizeof(payload)));
}

void test_connection_advances_sequences_and_accepts_data(void) {
    net_tcp_connection_t connection; net_tcp_view_t view; uint16_t accepted = 0U;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    view.source_port = 443; view.destination_port = 49152; view.sequence = 700U; view.acknowledgment = 101U;
    view.flags = NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &view));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_commit_send(&connection, 4U));
    TEST_ASSERT_EQUAL(105U, connection.local_sequence);
    view.flags = NET_TCP_FLAG_ACK; view.sequence = 701U; view.acknowledgment = 105U; view.payload_length = 3U;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_data(&connection, &view, &accepted));
    TEST_ASSERT_EQUAL(3U, accepted); TEST_ASSERT_EQUAL(704U, connection.remote_sequence);
    view.sequence = 703U; TEST_ASSERT_NOT_EQUAL(0, net_tcp_connection_accept_data(&connection, &view, &accepted));
    TEST_ASSERT_NOT_EQUAL(0, net_tcp_connection_commit_send(&connection, 0U));
}

void test_ack_confirms_pending_payload(void) {
    net_tcp_connection_t connection; uint8_t payload[2] = {'O','K'}; net_tcp_view_t view;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    view = (net_tcp_view_t){443, 49152, 700U, 101U, NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK, 0, 0};
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &view));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_track_send(&connection, payload, sizeof(payload), 2U));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_commit_send(&connection, sizeof(payload)));
    view = (net_tcp_view_t){443, 49152, 701U, 103U, NET_TCP_FLAG_ACK, 0, 0};
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_ack(&connection, &view));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_retransmit_allowed(&connection));
    view.acknowledgment = 104U; TEST_ASSERT_NOT_EQUAL(0, net_tcp_connection_accept_ack(&connection, &view));
}

void test_fin_close_transitions(void) {
    net_tcp_connection_t connection; net_tcp_view_t view; uint8_t segment[20] = {0};
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    view = (net_tcp_view_t){443, 49152, 700U, 101U, NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK, 0, 0};
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &view));
    TEST_ASSERT_EQUAL(20, net_tcp_connection_begin_close(&connection, segment, sizeof(segment)));
    TEST_ASSERT_EQUAL((NET_TCP_FLAG_FIN | NET_TCP_FLAG_ACK), segment[13]);
    TEST_ASSERT_EQUAL(NET_TCP_STATE_FIN_WAIT_1, connection.state); TEST_ASSERT_EQUAL(102U, connection.local_sequence);
    view.flags = NET_TCP_FLAG_ACK; view.acknowledgment = 102U; view.sequence = 700U;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_ack(&connection, &view));
    TEST_ASSERT_EQUAL(NET_TCP_STATE_FIN_WAIT_2, connection.state);
    view.flags = NET_TCP_FLAG_FIN | NET_TCP_FLAG_ACK; view.sequence = 701U;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_fin(&connection, &view));
    TEST_ASSERT_EQUAL(NET_TCP_STATE_CLOSED, connection.state);
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    view.flags = NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK; view.sequence = 700U; view.acknowledgment = 101U;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &view));
    view.flags = NET_TCP_FLAG_FIN | NET_TCP_FLAG_ACK; view.sequence = 701U; view.acknowledgment = 101U;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_fin(&connection, &view));
    TEST_ASSERT_EQUAL(NET_TCP_STATE_CLOSE_WAIT, connection.state); TEST_ASSERT_EQUAL(702U, connection.remote_sequence);
    TEST_ASSERT_EQUAL(20, net_tcp_connection_build_ack(&connection, segment, sizeof(segment)));
    view.sequence = 704U; TEST_ASSERT_NOT_EQUAL(0, net_tcp_connection_accept_fin(&connection, &view));
}

void test_bounded_retransmission_metadata(void) {
    net_tcp_connection_t connection; uint8_t payload[2] = {'O','K'};
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    { net_tcp_view_t syn_ack = {443, 49152, 700U, 101U, NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK, 0, 0};
      TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &syn_ack)); }
    TEST_ASSERT_EQUAL(0, net_tcp_connection_track_send(&connection, payload, sizeof(payload), 2U));
    TEST_ASSERT_EQUAL(1, net_tcp_connection_retransmit_allowed(&connection));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_note_retransmit(&connection));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_note_retransmit(&connection));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_retransmit_allowed(&connection));
    TEST_ASSERT_NOT_EQUAL(0, net_tcp_connection_note_retransmit(&connection));
}

int main(void) {
    unity_init(); RUN_TEST(test_build_and_parse_syn_ack); RUN_TEST(test_connection_builds_first_ack); RUN_TEST(test_build_and_parse_ack_payload); RUN_TEST(test_connection_advances_sequences_and_accepts_data); RUN_TEST(test_ack_confirms_pending_payload); RUN_TEST(test_fin_close_transitions); RUN_TEST(test_bounded_retransmission_metadata); unity_print_results(); unity_cleanup();
    return (unity_stats.tests_failed == 0) ? 0 : 1;
}
