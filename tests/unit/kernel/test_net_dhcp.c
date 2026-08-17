#include "../../framework/unity.h"
#include "../../../kernel/net_dhcp.h"

void setUp(void) {}
void tearDown(void) {}

void test_build_and_parse_dhcp_offer(void) {
    uint8_t packet[256] = {0};
    uint8_t mac[6] = {2, 0, 0, 0, 0, 1};
    net_dhcp_offer_t offer;
    TEST_ASSERT_EQUAL(244, net_dhcp_build_discover(packet, sizeof(packet), 0x12345678U, mac));
    TEST_ASSERT_EQUAL(1, packet[0]);
    TEST_ASSERT_EQUAL(0x63, packet[236]);
    packet[0] = 2;
    packet[16] = 10; packet[17] = 0; packet[18] = 2; packet[19] = 15;
    packet[240] = NET_DHCP_OPTION_MESSAGE_TYPE; packet[241] = 1; packet[242] = NET_DHCP_OFFER;
    packet[243] = NET_DHCP_OPTION_SERVER_ID; packet[244] = 4;
    packet[245] = 10; packet[246] = 0; packet[247] = 2; packet[248] = 2;
    packet[249] = NET_DHCP_OPTION_END;
    TEST_ASSERT_EQUAL(0, net_dhcp_parse_offer(packet, 250, 0x12345678U, &offer));
    TEST_ASSERT_EQUAL(10, offer.offered_ip[0]);
    TEST_ASSERT_EQUAL(2, offer.server_ip[3]);
    { net_dhcp_lease_t lease = {0};
      TEST_ASSERT_EQUAL(0, net_dhcp_lease_apply(&lease, &offer));
      TEST_ASSERT_EQUAL(1, lease.valid); TEST_ASSERT_EQUAL(10, lease.ipv4[0]);
      TEST_ASSERT_EQUAL(2, lease.server_ipv4[3]);
      net_dhcp_lease_clear(&lease); TEST_ASSERT_EQUAL(0, lease.valid); }
    TEST_ASSERT_NOT_EQUAL(0, net_dhcp_parse_offer(packet, 250, 0x87654321U, &offer));
    { uint8_t request[260] = {0}; uint8_t requested[4] = {10,0,2,15}; uint8_t server[4] = {10,0,2,2};
      net_dhcp_lease_t ack = {0};
      TEST_ASSERT_EQUAL(255, net_dhcp_build_request(request, sizeof(request), 0x12345678U, mac, requested, server));
      TEST_ASSERT_EQUAL(NET_DHCP_REQUEST, request[242]);
      request[0] = 2; request[16] = 10; request[17] = 0; request[18] = 2; request[19] = 15;
      request[242] = NET_DHCP_ACK;
      TEST_ASSERT_EQUAL(0, net_dhcp_parse_ack(request, 255, 0x12345678U, &ack));
      TEST_ASSERT_EQUAL(1, ack.valid); TEST_ASSERT_EQUAL(10, ack.ipv4[0]); }
}

int main(void) {
    unity_init();
    RUN_TEST(test_build_and_parse_dhcp_offer);
    unity_print_results();
    unity_cleanup();
    return (unity_stats.tests_failed == 0) ? 0 : 1;
}
