#ifndef AIOS_NET_DHCP_H
#define AIOS_NET_DHCP_H

#include <stdint.h>

#define NET_DHCP_FIXED_HEADER 236U
#define NET_DHCP_COOKIE_SIZE 4U
#define NET_DHCP_MAGIC_COOKIE 0x63825363U
#define NET_DHCP_OPTION_MESSAGE_TYPE 53U
#define NET_DHCP_OPTION_SERVER_ID 54U
#define NET_DHCP_OPTION_END 255U
#define NET_DHCP_DISCOVER 1U
#define NET_DHCP_OFFER 2U

typedef struct {
    uint32_t xid;
    uint8_t offered_ip[4];
    uint8_t server_ip[4];
    uint8_t message_type;
} net_dhcp_offer_t;

int net_dhcp_build_discover(uint8_t* packet, uint32_t capacity,
                            uint32_t xid, const uint8_t mac[6]);
int net_dhcp_parse_offer(const uint8_t* packet, uint32_t length,
                         uint32_t expected_xid, net_dhcp_offer_t* out);

#endif
