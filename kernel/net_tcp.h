#ifndef AIOS_NET_TCP_H
#define AIOS_NET_TCP_H

#include <stdint.h>

#define NET_TCP_HEADER_SIZE 20U
#define NET_TCP_FLAG_FIN 0x01U
#define NET_TCP_FLAG_SYN 0x02U
#define NET_TCP_FLAG_RST 0x04U
#define NET_TCP_FLAG_ACK 0x10U

typedef struct {
    uint16_t source_port;
    uint16_t destination_port;
    uint32_t sequence;
    uint32_t acknowledgment;
    uint8_t flags;
    const uint8_t* payload;
    uint16_t payload_length;
} net_tcp_view_t;

int net_tcp_build_syn(uint8_t* segment, uint32_t capacity,
                      uint16_t source_port, uint16_t destination_port,
                      uint32_t sequence);
int net_tcp_parse(const uint8_t* segment, uint32_t length,
                  net_tcp_view_t* out);

#endif
