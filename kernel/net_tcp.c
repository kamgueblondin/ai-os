#include "net_tcp.h"

static uint16_t get16(const uint8_t* p) { return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
static uint32_t get32(const uint8_t* p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static void put16(uint8_t* p, uint16_t v) { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static void put32(uint8_t* p, uint32_t v) { p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }

int net_tcp_build_syn(uint8_t* segment, uint32_t capacity,
                      uint16_t source_port, uint16_t destination_port,
                      uint32_t sequence) {
    uint16_t i;
    if (!segment || capacity < NET_TCP_HEADER_SIZE || source_port == 0U || destination_port == 0U) return -1;
    for (i=0; i<NET_TCP_HEADER_SIZE; ++i) segment[i]=0U;
    put16(segment, source_port); put16(segment+2, destination_port);
    put32(segment+4, sequence); segment[12]=0x50U; segment[13]=NET_TCP_FLAG_SYN;
    put16(segment+14, 65535U);
    return NET_TCP_HEADER_SIZE;
}

int net_tcp_parse(const uint8_t* segment, uint32_t length, net_tcp_view_t* out) {
    uint8_t header_words;
    uint16_t header_size;
    if (!segment || !out || length < NET_TCP_HEADER_SIZE) return -1;
    header_words = (uint8_t)(segment[12] >> 4); header_size = (uint16_t)header_words * 4U;
    if (header_words < 5U || header_size > length) return -2;
    out->source_port=get16(segment); out->destination_port=get16(segment+2);
    if (out->source_port == 0U || out->destination_port == 0U) return -3;
    out->sequence=get32(segment+4); out->acknowledgment=get32(segment+8);
    out->flags=(uint8_t)(segment[13] & 0x3fU);
    out->payload=segment+header_size; out->payload_length=(uint16_t)(length-header_size);
    return 0;
}
