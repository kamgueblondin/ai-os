#ifndef AIOS_NET_TCP_H
#define AIOS_NET_TCP_H

#include <stdint.h>
#include "net_tls_record.h"

#define NET_TCP_HEADER_SIZE 20U
#define NET_TCP_PROTOCOL 6U
#define NET_TCP_FLAG_FIN 0x01U
#define NET_TCP_FLAG_SYN 0x02U
#define NET_TCP_FLAG_RST 0x04U
#define NET_TCP_FLAG_ACK 0x10U

#define NET_TCP_STATE_CLOSED 0U
#define NET_TCP_STATE_SYN_SENT 1U
#define NET_TCP_STATE_ESTABLISHED 2U
#define NET_TCP_STATE_FIN_WAIT_1 3U
#define NET_TCP_STATE_FIN_WAIT_2 4U
#define NET_TCP_STATE_CLOSE_WAIT 5U
#define NET_TCP_STATE_LAST_ACK 6U

typedef struct {
    uint16_t source_port;
    uint16_t destination_port;
    uint32_t sequence;
    uint32_t acknowledgment;
    uint8_t flags;
    const uint8_t* payload;
    uint16_t payload_length;
} net_tcp_view_t;

typedef struct {
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t local_sequence;
    uint32_t remote_sequence;
    const uint8_t* pending_payload;
    uint16_t pending_length;
    uint8_t retransmit_count;
    uint8_t retransmit_limit;
    uint16_t receive_window;
    uint8_t state;
} net_tcp_connection_t;

int net_tcp_build_syn(uint8_t* segment, uint32_t capacity,
                      uint16_t source_port, uint16_t destination_port,
                      uint32_t sequence);
int net_tcp_build_ack(uint8_t* segment, uint32_t capacity,
                      uint16_t source_port, uint16_t destination_port,
                      uint32_t sequence, uint32_t acknowledgment);
int net_tcp_build_data(uint8_t* segment, uint32_t capacity,
                       uint16_t source_port, uint16_t destination_port,
                       uint32_t sequence, uint32_t acknowledgment,
                       const uint8_t* payload, uint16_t payload_length);
int net_tcp_parse(const uint8_t* segment, uint32_t length,
                  net_tcp_view_t* out);
/* Calcule le checksum TCP IPv4 sur le segment caller-owned. */
uint16_t net_tcp_checksum_ipv4(const uint8_t source_ip[4], const uint8_t destination_ip[4],
                               const uint8_t* segment, uint16_t length);
int net_tcp_build_syn_ipv4(uint8_t* packet, uint32_t capacity,
                           const uint8_t source_ip[4], const uint8_t destination_ip[4],
                           uint16_t source_port, uint16_t destination_port,
                           uint32_t sequence);
int net_tcp_is_syn_ack_for(const net_tcp_view_t* view, uint16_t local_port,
                           uint16_t remote_port, uint32_t local_sequence,
                           uint32_t* remote_sequence);
int net_tcp_connection_open(net_tcp_connection_t* connection,
                            uint16_t local_port, uint16_t remote_port,
                            uint32_t local_sequence);
int net_tcp_connection_accept_syn_ack(net_tcp_connection_t* connection,
                                      const net_tcp_view_t* view);
int net_tcp_connection_build_ack(const net_tcp_connection_t* connection,
                                 uint8_t* segment, uint32_t capacity);
int net_tcp_connection_build_data(net_tcp_connection_t* connection,
                                  uint8_t* segment, uint32_t capacity,
                                  const uint8_t* payload, uint16_t payload_length,
                                  uint8_t retransmit_limit);
int net_tcp_connection_build_tls_record(net_tcp_connection_t* connection,
                                        uint8_t* segment, uint32_t capacity,
                                        uint8_t* record, uint32_t record_capacity,
                                        uint8_t content_type, const uint8_t* payload,
                                        uint16_t payload_length, uint8_t retransmit_limit);
int net_tcp_connection_commit_send(net_tcp_connection_t* connection, uint16_t payload_length);
int net_tcp_connection_accept_data(net_tcp_connection_t* connection,
                                   const net_tcp_view_t* view,
                                   uint16_t* accepted_length);
int net_tcp_connection_set_receive_window(net_tcp_connection_t* connection,
                                           uint16_t receive_window);
int net_tcp_connection_track_send(net_tcp_connection_t* connection,
                                  const uint8_t* payload, uint16_t payload_length,
                                  uint8_t retransmit_limit);
int net_tcp_connection_retransmit_allowed(const net_tcp_connection_t* connection);
int net_tcp_connection_note_retransmit(net_tcp_connection_t* connection);
int net_tcp_connection_accept_ack(net_tcp_connection_t* connection,
                                  const net_tcp_view_t* view);
int net_tcp_build_fin_ack(uint8_t* segment, uint32_t capacity,
                          uint16_t source_port, uint16_t destination_port,
                          uint32_t sequence, uint32_t acknowledgment);
int net_tcp_connection_begin_close(net_tcp_connection_t* connection,
                                   uint8_t* segment, uint32_t capacity);
int net_tcp_connection_accept_fin(net_tcp_connection_t* connection,
                                  const net_tcp_view_t* view);

#endif
