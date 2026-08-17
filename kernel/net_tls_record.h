#ifndef AIOS_NET_TLS_RECORD_H
#define AIOS_NET_TLS_RECORD_H
#include <stdint.h>
#define NET_TLS_RECORD_HEADER 5U
#define NET_TLS_CONTENT_HANDSHAKE 22U
#define NET_TLS_VERSION_1_2_MAJOR 3U
#define NET_TLS_VERSION_1_2_MINOR 3U
typedef struct { uint8_t content_type; uint8_t major; uint8_t minor; const uint8_t* payload; uint16_t payload_length; } net_tls_record_view_t;
int net_tls_record_build(uint8_t* record, uint32_t capacity, uint8_t content_type, const uint8_t* payload, uint16_t payload_length);
int net_tls_record_parse(const uint8_t* record,uint32_t length,net_tls_record_view_t* out);
/* Construit un ClientHello TLS 1.2 minimal dans un record caller-owned. */
int net_tls_client_hello_build(uint8_t* record, uint32_t capacity,
                               const uint8_t random[32]);
#endif
