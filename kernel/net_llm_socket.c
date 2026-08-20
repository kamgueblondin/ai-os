#include "net_llm_socket.h"

int net_llm_socket_build_request(int socket_id, net_tls_aes_gcm_session_t* session,
                                 uint8_t provider, uint8_t stream,
                                 uint8_t* json, uint16_t json_capacity,
                                 uint8_t* request, uint16_t request_capacity,
                                 const char* host, const char* path,
                                 const char* bearer_token, const char* model,
                                 const uint8_t* prompt, uint16_t prompt_length,
                                 uint8_t* tls_record, uint32_t tls_capacity,
                                 uint8_t* tcp_segment, uint16_t tcp_capacity,
                                 uint8_t retransmit_limit) {
    int json_length;
    int request_length;
    uint16_t segment_length = 0U;

    if (!session || !json || !request || !host || !path || !model ||
        (!prompt && prompt_length) || !tls_record || !tcp_segment) return -1;
    if (provider != NET_LLM_SOCKET_PROVIDER_OLLAMA && provider != NET_LLM_SOCKET_PROVIDER_OPENAI) return -2;

    if (provider == NET_LLM_SOCKET_PROVIDER_OLLAMA) {
        json_length = stream
            ? net_llm_build_ollama_generate_stream_json(json, json_capacity, model, prompt, prompt_length)
            : net_llm_build_ollama_generate_json(json, json_capacity, model, prompt, prompt_length);
    } else {
        if (!bearer_token || !bearer_token[0]) return -3;
        json_length = stream
            ? net_llm_build_openai_chat_stream_json(json, json_capacity, model, prompt, prompt_length)
            : net_llm_build_openai_chat_json(json, json_capacity, model, prompt, prompt_length);
    }
    if (json_length < 0) return -4;

    request_length = provider == NET_LLM_SOCKET_PROVIDER_OPENAI
        ? net_http_build_post_json_bearer(request, request_capacity, host, path, bearer_token,
                                           json, (uint16_t)json_length)
        : net_http_build_post_json(request, request_capacity, host, path, json, (uint16_t)json_length);
    if (request_length < 0) return -5;

    if (net_socket_send_tls(socket_id, session, NET_TLS_CONTENT_APPLICATION_DATA,
                            request, (uint16_t)request_length, tls_record, tls_capacity,
                            tcp_segment, tcp_capacity, &segment_length,
                            retransmit_limit) != 0) return -6;
    return (int)segment_length;
}
