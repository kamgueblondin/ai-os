#ifndef AIOS_NET_DHCP_H
#define AIOS_NET_DHCP_H

#include <stdint.h>

#define NET_DHCP_FIXED_HEADER 236U
#define NET_DHCP_COOKIE_SIZE 4U
#define NET_DHCP_MAGIC_COOKIE 0x63825363U
#define NET_DHCP_OPTION_ROUTER 3U
#define NET_DHCP_OPTION_DNS 6U
#define NET_DHCP_OPTION_MESSAGE_TYPE 53U
#define NET_DHCP_OPTION_SERVER_ID 54U
#define NET_DHCP_OPTION_PARAMETER_REQUEST_LIST 55U
#define NET_DHCP_OPTION_END 255U
#define NET_DHCP_DISCOVER 1U
#define NET_DHCP_OFFER 2U
#define NET_DHCP_REQUEST 3U
#define NET_DHCP_ACK 5U

typedef struct {
    uint32_t xid;
    uint8_t offered_ip[4];
    uint8_t server_ip[4];
    uint8_t message_type;
} net_dhcp_offer_t;
typedef struct {
    uint8_t valid;
    uint8_t router_valid;
    uint8_t dns_valid;
    uint8_t ipv4[4];
    uint8_t server_ipv4[4];
    uint8_t router_ipv4[4];
    uint8_t dns_ipv4[4];
    uint32_t xid;
} net_dhcp_lease_t;

int net_dhcp_build_discover(uint8_t* packet, uint32_t capacity,
                            uint32_t xid, const uint8_t mac[6]);
int net_dhcp_build_request(uint8_t* packet, uint32_t capacity,
                           uint32_t xid, const uint8_t mac[6],
                           const uint8_t requested_ip[4], const uint8_t server_ip[4]);
int net_dhcp_parse_offer(const uint8_t* packet, uint32_t length,
                         uint32_t expected_xid, net_dhcp_offer_t* out);
int net_dhcp_lease_apply(net_dhcp_lease_t* lease, const net_dhcp_offer_t* offer);
void net_dhcp_lease_clear(net_dhcp_lease_t* lease);
int net_dhcp_parse_ack(const uint8_t* packet, uint32_t length,
                       uint32_t expected_xid, net_dhcp_lease_t* lease);

#endif
