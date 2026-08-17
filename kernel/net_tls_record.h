#ifndef AIOS_NET_TLS_RECORD_H
#define AIOS_NET_TLS_RECORD_H
#include <stdint.h>
#define NET_TLS_RECORD_HEADER 5U
#define NET_TLS_CONTENT_HANDSHAKE 22U
#define NET_TLS_VERSION_1_2_MAJOR 3U
#define NET_TLS_VERSION_1_2_MINOR 3U
typedef struct { uint8_t content_type; uint8_t major; uint8_t minor; const uint8_t* payload; uint16_t payload_length; } net_tls_record_view_t;
typedef struct { uint8_t* buffer; uint16_t capacity; uint16_t length; } net_tls_record_accumulator_t;
typedef struct { const uint8_t* random; const uint8_t* session_id; uint8_t session_id_length; uint16_t cipher_suite; uint8_t compression_method; } net_tls_server_hello_view_t;
typedef enum { NET_TLS_HANDSHAKE_IDLE=0, NET_TLS_HANDSHAKE_CLIENT_HELLO_SENT=1, NET_TLS_HANDSHAKE_SERVER_HELLO_RECEIVED=2 } net_tls_handshake_state_t;
typedef struct { net_tls_handshake_state_t state; uint16_t cipher_suite; const uint8_t* server_random; } net_tls_handshake_t;
int net_tls_record_build(uint8_t* record, uint32_t capacity, uint8_t content_type, const uint8_t* payload, uint16_t payload_length);
int net_tls_record_parse(const uint8_t* record,uint32_t length,net_tls_record_view_t* out);
/* Parse le premier record d’un flux TCP et publie les octets consommés. */
int net_tls_record_parse_stream(const uint8_t* stream, uint32_t length,
                                net_tls_record_view_t* out, uint16_t* consumed);
int net_tls_record_accumulator_init(net_tls_record_accumulator_t* accumulator,
                                    uint8_t* buffer, uint16_t capacity);
/* Retourne 1 si le record est incomplet, 0 lorsqu’il est complet. */
int net_tls_record_accumulator_feed(net_tls_record_accumulator_t* accumulator,
                                    const uint8_t* fragment, uint16_t fragment_length,
                                    net_tls_record_view_t* out);
int net_tls_server_hello_parse(const uint8_t* handshake, uint16_t length,
                               net_tls_server_hello_view_t* out);
int net_tls_handshake_init(net_tls_handshake_t* handshake);
int net_tls_handshake_note_client_hello(net_tls_handshake_t* handshake);
int net_tls_handshake_accept_server_hello(net_tls_handshake_t* handshake,
                                          const uint8_t* message, uint16_t length);
/* Construit un ClientHello TLS 1.2 minimal dans un record caller-owned. */
int net_tls_client_hello_build(uint8_t* record, uint32_t capacity,
                               const uint8_t random[32]);
#endif
