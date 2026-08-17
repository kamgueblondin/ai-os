#include "net_dhcp.h"

static void put_be32(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24); p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8); p[3] = (uint8_t)value;
}
static uint32_t get_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}
static void copy_bytes(uint8_t* dst, const uint8_t* src, uint32_t n) {
    uint32_t i; for (i = 0; i < n; ++i) dst[i] = src[i];
}

int net_dhcp_build_discover(uint8_t* packet, uint32_t capacity,
                            uint32_t xid, const uint8_t mac[6]) {
    uint32_t i;
    if (!packet || !mac || capacity < 244U) return -1;
    for (i = 0; i < 244U; ++i) packet[i] = 0U;
    packet[0] = 1U; packet[1] = 1U; packet[2] = 6U;
    put_be32(packet + 4, xid);
    copy_bytes(packet + 28, mac, 6);
    put_be32(packet + 236, NET_DHCP_MAGIC_COOKIE);
    packet[240] = NET_DHCP_OPTION_MESSAGE_TYPE; packet[241] = 1U;
    packet[242] = NET_DHCP_DISCOVER; packet[243] = NET_DHCP_OPTION_END;
    return 244;
}

int net_dhcp_parse_offer(const uint8_t* packet, uint32_t length,
                         uint32_t expected_xid, net_dhcp_offer_t* out) {
    uint32_t pos;
    uint8_t type = 0U;
    uint8_t server_seen = 0U;
    if (!packet || !out || length < 244U || packet[0] != 2U ||
        get_be32(packet + 4) != expected_xid ||
        get_be32(packet + 236) != NET_DHCP_MAGIC_COOKIE)
        return -1;
    copy_bytes(out->offered_ip, packet + 16, 4);
    out->server_ip[0] = out->server_ip[1] = out->server_ip[2] = out->server_ip[3] = 0U;
    out->xid = expected_xid;
    for (pos = 240U; pos < length;) {
        uint8_t code = packet[pos++];
        uint8_t size;
        if (code == NET_DHCP_OPTION_END) break;
        if (code == 0U) continue;
        if (pos >= length) return -2;
        size = packet[pos++];
        if (pos + size > length) return -2;
        if (code == NET_DHCP_OPTION_MESSAGE_TYPE && size == 1U) type = packet[pos];
        if (code == NET_DHCP_OPTION_SERVER_ID && size == 4U) {
            copy_bytes(out->server_ip, packet + pos, 4); server_seen = 1U;
        }
        pos += size;
    }
    if (type != NET_DHCP_OFFER || !server_seen) return -3;
    out->message_type = type;
    return 0;
}
