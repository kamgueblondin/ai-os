#ifndef AIOS_NET_TLS_RECORD_H
#define AIOS_NET_TLS_RECORD_H
#include <stdint.h>
#define NET_TLS_RECORD_HEADER 5U
#define NET_TLS_CONTENT_HANDSHAKE 22U
#define NET_TLS_VERSION_1_2_MAJOR 3U
#define NET_TLS_VERSION_1_2_MINOR 3U
#define NET_TLS_HANDSHAKE_HEADER 4U
#define NET_TLS_HANDSHAKE_SERVER_HELLO 2U
#define NET_TLS_HANDSHAKE_CERTIFICATE 11U
#define NET_TLS_HANDSHAKE_SERVER_KEY_EXCHANGE 12U
#define NET_TLS_HANDSHAKE_CERTIFICATE_REQUEST 13U
#define NET_TLS_HANDSHAKE_SERVER_HELLO_DONE 14U
#define NET_TLS_HANDSHAKE_CLIENT_KEY_EXCHANGE 16U
#define NET_TLS_HANDSHAKE_FINISHED 20U
#define NET_TLS_CONTENT_CHANGE_CIPHER_SPEC 20U
typedef struct { uint8_t content_type; uint8_t major; uint8_t minor; const uint8_t* payload; uint16_t payload_length; } net_tls_record_view_t;
typedef struct { uint8_t* buffer; uint16_t capacity; uint16_t length; } net_tls_record_accumulator_t;
typedef struct { uint8_t type; const uint8_t* body; uint32_t body_length; } net_tls_handshake_view_t;
typedef struct { uint8_t* buffer; uint16_t capacity; uint16_t length; } net_tls_handshake_accumulator_t;
typedef struct { uint8_t* buffer; uint16_t capacity; uint16_t length; } net_tls_transcript_t;
typedef struct { const uint8_t* random; const uint8_t* session_id; uint8_t session_id_length; uint16_t cipher_suite; uint8_t compression_method; const uint8_t* extensions; uint16_t extensions_length; } net_tls_server_hello_view_t;
typedef struct { const uint8_t* certificate; uint32_t certificate_length; uint32_t certificate_list_length; } net_tls_certificate_view_t;
typedef struct { uint16_t named_curve; const uint8_t* public_key; uint8_t public_key_length; uint8_t hash_algorithm; uint8_t signature_algorithm; const uint8_t* signature; uint16_t signature_length; } net_tls_server_key_exchange_view_t;
typedef struct { const uint8_t* certificate_types; uint8_t certificate_types_length; const uint8_t* signature_algorithms; uint16_t signature_algorithms_length; const uint8_t* certificate_authorities; uint16_t certificate_authorities_length; } net_tls_certificate_request_view_t;
typedef enum { NET_TLS_HANDSHAKE_IDLE=0, NET_TLS_HANDSHAKE_CLIENT_HELLO_SENT=1, NET_TLS_HANDSHAKE_SERVER_HELLO_RECEIVED=2, NET_TLS_HANDSHAKE_CERTIFICATE_RECEIVED=3, NET_TLS_HANDSHAKE_SERVER_KEY_EXCHANGE_RECEIVED=4, NET_TLS_HANDSHAKE_CERTIFICATE_REQUEST_RECEIVED=5, NET_TLS_HANDSHAKE_SERVER_HELLO_DONE_RECEIVED=6, NET_TLS_HANDSHAKE_CLIENT_CERTIFICATE_SENT=7, NET_TLS_HANDSHAKE_CLIENT_KEY_EXCHANGE_SENT=8, NET_TLS_HANDSHAKE_CHANGE_CIPHER_SPEC_SENT=9, NET_TLS_HANDSHAKE_FINISHED_SENT=10 } net_tls_handshake_state_t;
typedef struct { net_tls_handshake_state_t state; uint16_t cipher_suite; const uint8_t* server_random; const uint8_t* server_certificate; uint32_t server_certificate_length; uint16_t server_named_curve; const uint8_t* server_public_key; uint8_t server_public_key_length; uint8_t certificate_requested; } net_tls_handshake_t;
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
int net_tls_handshake_parse(const uint8_t* handshake, uint16_t length,
                            net_tls_handshake_view_t* out);
int net_tls_handshake_accumulator_init(net_tls_handshake_accumulator_t* accumulator,
                                       uint8_t* buffer, uint16_t capacity);
int net_tls_handshake_accumulator_feed(net_tls_handshake_accumulator_t* accumulator,
                                       const uint8_t* fragment, uint16_t fragment_length,
                                       net_tls_handshake_view_t* out);
int net_tls_transcript_init(net_tls_transcript_t* transcript, uint8_t* buffer, uint16_t capacity);
int net_tls_transcript_append(net_tls_transcript_t* transcript, const uint8_t* handshake, uint16_t length);
int net_tls_transcript_sha256(const net_tls_transcript_t* transcript, uint8_t digest[32]);
int net_tls_prf_sha256(uint8_t* output, uint16_t output_length,
                       const uint8_t* secret, uint16_t secret_length,
                       const uint8_t* label, uint16_t label_length,
                       const uint8_t* seed_a, uint16_t seed_a_length,
                       const uint8_t* seed_b, uint16_t seed_b_length,
                       uint8_t* workspace, uint32_t workspace_capacity);
int net_tls_derive_master_secret(uint8_t master_secret[48],
                                 const uint8_t* premaster_secret, uint16_t premaster_length,
                                 const uint8_t client_random[32], const uint8_t server_random[32],
                                 uint8_t* workspace, uint32_t workspace_capacity);
int net_tls_finished_verify_data(uint8_t verify_data[12], const uint8_t master_secret[48],
                                 const net_tls_transcript_t* transcript, uint8_t transcript_hash[32],
                                 uint8_t* workspace, uint32_t workspace_capacity);
int net_tls_server_hello_parse(const uint8_t* handshake, uint16_t length,
                               net_tls_server_hello_view_t* out);
int net_tls_certificate_parse(const uint8_t* handshake, uint16_t length,
                              net_tls_certificate_view_t* out);
int net_tls_server_key_exchange_parse(const uint8_t* handshake, uint16_t length,
                                      net_tls_server_key_exchange_view_t* out);
int net_tls_certificate_request_parse(const uint8_t* handshake, uint16_t length,
                                      net_tls_certificate_request_view_t* out);
int net_tls_server_hello_done_parse(const uint8_t* handshake, uint16_t length);
int net_tls_handshake_init(net_tls_handshake_t* handshake);
int net_tls_handshake_note_client_hello(net_tls_handshake_t* handshake);
int net_tls_handshake_accept_server_hello(net_tls_handshake_t* handshake,
                                          const uint8_t* message, uint16_t length);
int net_tls_handshake_accept_certificate(net_tls_handshake_t* handshake,
                                         const uint8_t* message, uint16_t length);
int net_tls_handshake_accept_server_key_exchange(net_tls_handshake_t* handshake,
                                               const uint8_t* message, uint16_t length);
int net_tls_handshake_accept_certificate_request(net_tls_handshake_t* handshake,
                                                 const uint8_t* message, uint16_t length);
int net_tls_handshake_accept_server_hello_done(net_tls_handshake_t* handshake,
                                               const uint8_t* message, uint16_t length);
int net_tls_handshake_accept_server_message(net_tls_handshake_t* handshake,
                                            const uint8_t* message, uint16_t length,
                                            net_tls_transcript_t* transcript);
int net_tls_client_certificate_empty_build(uint8_t* handshake, uint32_t capacity);
int net_tls_client_key_exchange_build(uint8_t* handshake, uint32_t capacity,
                                      const uint8_t* public_key, uint8_t public_key_length);
int net_tls_change_cipher_spec_build(uint8_t* record, uint32_t capacity);
int net_tls_finished_build(uint8_t* record, uint32_t capacity, const uint8_t verify_data[12]);
int net_tls_handshake_note_client_certificate(net_tls_handshake_t* handshake);
int net_tls_handshake_note_client_key_exchange(net_tls_handshake_t* handshake);
int net_tls_handshake_note_change_cipher_spec(net_tls_handshake_t* handshake);
int net_tls_handshake_note_finished(net_tls_handshake_t* handshake);
/* Construit un ClientHello TLS 1.2 minimal dans un record caller-owned. */
int net_tls_client_hello_build(uint8_t* record, uint32_t capacity,
                               const uint8_t random[32]);
#endif
