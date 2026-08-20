#ifndef AIOS_NET_LLM_SOCKET_H
#define AIOS_NET_LLM_SOCKET_H

#include <stdint.h>
#include "net_socket.h"
#include "net_http_tls.h"

#define NET_LLM_SOCKET_PROVIDER_OLLAMA 0U
#define NET_LLM_SOCKET_PROVIDER_OPENAI 1U

/*
 * Construit un POST LLM JSON dans les buffers caller-owned puis l’encapsule
 * dans un record TLS AES-GCM et un segment TCP du socket établi.
 * Retourne la longueur du segment TCP, ou une valeur négative en cas d’erreur.
 */
int net_llm_socket_build_request(int socket_id, net_tls_aes_gcm_session_t* session,
                                 uint8_t provider, uint8_t stream,
                                 uint8_t* json, uint16_t json_capacity,
                                 uint8_t* request, uint16_t request_capacity,
                                 const char* host, const char* path,
                                 const char* bearer_token, const char* model,
                                 const uint8_t* prompt, uint16_t prompt_length,
                                 uint8_t* tls_record, uint32_t tls_capacity,
                                 uint8_t* tcp_segment, uint16_t tcp_capacity,
                                 uint8_t retransmit_limit);

#endif
