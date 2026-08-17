#include "../../framework/unity.h"
#include "../../../kernel/net_ipv4_udp.h"

void setUp(void) {}
void tearDown(void) {}

void test_build_parse_udp_ipv4(void) {
    uint8_t packet[64] = {0};
    uint8_t source[4] = {10,0,2,15}; uint8_t dest[4] = {10,0,2,2};
    uint8_t payload[3] = {'D','H','C'};
    net_udp_view_t view;
    TEST_ASSERT_EQUAL(31, net_udp_build_ipv4(packet, sizeof(packet), source, dest, 68, 67, payload, 3));
    TEST_ASSERT_EQUAL(0, net_udp_parse_ipv4(packet, 31, &view));
    TEST_ASSERT_EQUAL(68, view.source_port);
    TEST_ASSERT_EQUAL(67, view.destination_port);
    TEST_ASSERT_EQUAL(3, view.payload_length);
    TEST_ASSERT_EQUAL('D', view.payload[0]);
    packet[12] ^= 1U;
    TEST_ASSERT_NOT_EQUAL(0, net_udp_parse_ipv4(packet, 31, &view));
}

int main(void) {
    unity_init();
    RUN_TEST(test_build_parse_udp_ipv4);
    unity_print_results();
    unity_cleanup();
    return (unity_stats.tests_failed == 0) ? 0 : 1;
}
