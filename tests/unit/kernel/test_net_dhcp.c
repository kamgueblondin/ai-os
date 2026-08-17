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
    TEST_ASSERT_NOT_EQUAL(0, net_dhcp_parse_offer(packet, 250, 0x87654321U, &offer));
}

int main(void) {
    unity_init();
    RUN_TEST(test_build_and_parse_dhcp_offer);
    unity_print_results();
    unity_cleanup();
    return (unity_stats.tests_failed == 0) ? 0 : 1;
}
