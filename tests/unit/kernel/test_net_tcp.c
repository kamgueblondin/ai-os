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
    segment[0] = 0; segment[1] = 0; TEST_ASSERT_NOT_EQUAL(0, net_tcp_parse(segment, 20, &view));
}

int main(void) {
    unity_init(); RUN_TEST(test_build_and_parse_syn_ack); unity_print_results(); unity_cleanup();
    return (unity_stats.tests_failed == 0) ? 0 : 1;
}
