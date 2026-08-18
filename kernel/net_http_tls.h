#ifndef AIOS_NET_HTTP_TLS_H
#define AIOS_NET_HTTP_TLS_H

#include <stdint.h>
#include "net_tcp.h"

typedef struct {
    uint16_t status_code;
    const uint8_t* body;
    uint16_t body_length;
    uint16_t header_length;
} net_http_response_view_t;
typedef struct {
    uint8_t* buffer;
    uint16_t capacity;
    uint16_t length;
    uint16_t header_length;
    uint16_t expected_body_length;
    uint16_t status_code;
    uint8_t headers_complete;
} net_http_response_accumulator_t;
typedef struct {
    uint8_t* buffer;
    uint16_t capacity;
    uint16_t length;
    uint16_t raw_length;
    uint16_t header_length;
    uint16_t status_code;
    uint32_t chunk_remaining;
    uint8_t line[10];
    uint8_t line_length;
    uint8_t state;
} net_http_chunked_accumulator_t;

/* Construit une requête HTTP/1.1 GET avec Host et Connection: close dans un buffer caller-owned. */
int net_http_build_get(uint8_t* request,uint16_t capacity,const char* host,const char* path);

/* Chiffre une requête GET dans un record TLS applicatif et l’encapsule dans un segment TCP. */
int net_http_tls_build_get(net_tcp_connection_t* connection,net_tls_aes_gcm_session_t* session,
                           uint8_t* tcp_segment,uint32_t tcp_capacity,uint8_t* tls_record,uint32_t tls_capacity,
                           uint8_t* request,uint16_t request_capacity,const char* host,const char* path,
                           uint8_t retransmit_limit);

/* Construit un POST HTTP/1.1 JSON, avec Content-Type, Content-Length et Connection: close. */
int net_http_build_post_json(uint8_t* request,uint16_t capacity,const char* host,const char* path,
                             const uint8_t* json,uint16_t json_length);
/* Construit un POST JSON avec Authorization: Bearer, le token restant uniquement une entrée caller-owned. */
int net_http_build_post_json_bearer(uint8_t* request,uint16_t capacity,const char* host,const char* path,
                                    const char* bearer_token,const uint8_t* json,uint16_t json_length);
/* Chiffre un POST JSON dans un record TLS applicatif et l’encapsule dans un segment TCP. */
int net_http_tls_build_post_json(net_tcp_connection_t* connection,net_tls_aes_gcm_session_t* session,
                                 uint8_t* tcp_segment,uint32_t tcp_capacity,uint8_t* tls_record,uint32_t tls_capacity,
                                 uint8_t* request,uint16_t request_capacity,const char* host,const char* path,
                                 const uint8_t* json,uint16_t json_length,uint8_t retransmit_limit);

/* Parse une réponse HTTP/1.1 complète déjà déchiffrée. Le body reste une vue sur plaintext. */
int net_http_response_parse(const uint8_t* plaintext,uint16_t plaintext_length,net_http_response_view_t* out);
/* Accumule une réponse HTTP/1.1 Content-Length à travers plusieurs plaintexts TLS.
 * Retourne 1 si le body est incomplet, 0 seulement lorsque la réponse complète est disponible. */
int net_http_response_accumulator_init(net_http_response_accumulator_t* accumulator,uint8_t* buffer,uint16_t capacity);
int net_http_response_accumulator_feed(net_http_response_accumulator_t* accumulator,const uint8_t* fragment,uint16_t fragment_length,net_http_response_view_t* out);
/* Décode Transfer-Encoding: chunked à travers plusieurs plaintexts TLS dans un buffer caller-owned. */
int net_http_chunked_accumulator_init(net_http_chunked_accumulator_t* accumulator,uint8_t* buffer,uint16_t capacity);
int net_http_chunked_accumulator_feed(net_http_chunked_accumulator_t* accumulator,const uint8_t* fragment,uint16_t fragment_length,net_http_response_view_t* out);

/* Ouvre transactionnellement un record TLS applicatif TCP et parse sa réponse HTTP/1.1. */
int net_http_tls_open_response(net_tcp_connection_t* connection,net_tls_aes_gcm_session_t* session,
                               const net_tcp_view_t* view,uint8_t* plaintext,uint16_t plaintext_capacity,
                               net_http_response_view_t* response,uint16_t* consumed);
/* Ouvre un record TLS applicatif et alimente un body HTTP Content-Length progressif. */
int net_http_tls_open_response_stream(net_tcp_connection_t* connection,net_tls_aes_gcm_session_t* session,
                                      const net_tcp_view_t* view,uint8_t* plaintext,uint16_t plaintext_capacity,
                                      net_http_response_accumulator_t* accumulator,net_http_response_view_t* response,
                                      uint16_t* consumed);

#endif
