#include "../../framework/unity.h"
#include "../../framework/test_kernel.h"
#include "../../../kernel/net_ethernet_arp.h"

void setUp(void) {}
void tearDown(void) {}

void test_build_and_parse_arp_request(void) {
    uint8_t frame[64] = {0};
    uint8_t mac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
    uint8_t ip[4] = {10, 0, 2, 15};
    uint8_t target[4] = {10, 0, 2, 2};
    net_arp_packet_t packet;
    int length = net_arp_build_request(frame, sizeof(frame), mac, ip, target);
    TEST_ASSERT_EQUAL(42, length);
    TEST_ASSERT_EQUAL(0, net_arp_parse(frame, (uint32_t)length, &packet));
    TEST_ASSERT_EQUAL(NET_ARP_OPCODE_REQUEST, packet.opcode);
    TEST_ASSERT_EQUAL_MEMORY(mac, packet.sender_mac, 6);
    TEST_ASSERT_EQUAL_MEMORY(ip, packet.sender_ipv4, 4);
    TEST_ASSERT_EQUAL_MEMORY(target, packet.target_ipv4, 4);
}

void test_rejects_short_or_non_arp_frame(void) {
    uint8_t frame[42] = {0};
    net_arp_packet_t packet;
    frame[12] = 0x08;
    frame[13] = 0x00;
    TEST_ASSERT_NOT_EQUAL(0, net_arp_parse(frame, 13, &packet));
    TEST_ASSERT_NOT_EQUAL(0, net_arp_parse(frame, sizeof(frame), &packet));
}

void test_reply_matches_local_and_requested_addresses(void) {
    net_arp_packet_t packet = {0};
    uint8_t local[4] = {10, 0, 2, 15};
    uint8_t requested[4] = {10, 0, 2, 2};
    packet.opcode = NET_ARP_OPCODE_REPLY;
    packet.target_ipv4[0] = 10; packet.target_ipv4[1] = 0;
    packet.target_ipv4[2] = 2; packet.target_ipv4[3] = 15;
    packet.sender_ipv4[0] = 10; packet.sender_ipv4[1] = 0;
    packet.sender_ipv4[2] = 2; packet.sender_ipv4[3] = 2;
    TEST_ASSERT_EQUAL(1, net_arp_is_reply_for(&packet, local, requested));
    packet.sender_ipv4[3] = 3;
    TEST_ASSERT_EQUAL(0, net_arp_is_reply_for(&packet, local, requested));
}

int main(void) {
    unity_init();
    RUN_TEST(test_build_and_parse_arp_request);
    RUN_TEST(test_rejects_short_or_non_arp_frame);
    RUN_TEST(test_reply_matches_local_and_requested_addresses);
    unity_print_results();
    unity_cleanup();
    return (unity_stats.tests_failed == 0) ? 0 : 1;
}
