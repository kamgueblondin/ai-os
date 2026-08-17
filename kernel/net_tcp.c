#include "net_tcp.h"

static uint16_t get16(const uint8_t* p) { return (uint16_t)(((uint16_t)p[0] << 8) | p[1]); }
static uint32_t get32(const uint8_t* p) { return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static void put16(uint8_t* p, uint16_t v) { p[0]=(uint8_t)(v>>8); p[1]=(uint8_t)v; }
static void put32(uint8_t* p, uint32_t v) { p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16); p[2]=(uint8_t)(v>>8); p[3]=(uint8_t)v; }

static int build_control(uint8_t* segment, uint32_t capacity, uint16_t source_port,
                         uint16_t destination_port, uint32_t sequence,
                         uint32_t acknowledgment, uint8_t flags) {
    uint16_t i;
    if (!segment || capacity < NET_TCP_HEADER_SIZE || source_port == 0U || destination_port == 0U) return -1;
    for (i=0; i<NET_TCP_HEADER_SIZE; ++i) segment[i]=0U;
    put16(segment, source_port); put16(segment+2, destination_port);
    put32(segment+4, sequence); put32(segment+8, acknowledgment);
    segment[12]=0x50U; segment[13]=flags; put16(segment+14, 65535U);
    return NET_TCP_HEADER_SIZE;
}

int net_tcp_build_syn(uint8_t* segment, uint32_t capacity, uint16_t source_port,
                      uint16_t destination_port, uint32_t sequence) {
    return build_control(segment, capacity, source_port, destination_port, sequence, 0U, NET_TCP_FLAG_SYN);
}

int net_tcp_build_ack(uint8_t* segment, uint32_t capacity, uint16_t source_port,
                      uint16_t destination_port, uint32_t sequence, uint32_t acknowledgment) {
    return build_control(segment, capacity, source_port, destination_port, sequence, acknowledgment, NET_TCP_FLAG_ACK);
}

int net_tcp_build_fin_ack(uint8_t* segment, uint32_t capacity, uint16_t source_port,
                          uint16_t destination_port, uint32_t sequence, uint32_t acknowledgment) {
    return build_control(segment, capacity, source_port, destination_port, sequence, acknowledgment,
                         NET_TCP_FLAG_FIN | NET_TCP_FLAG_ACK);
}

int net_tcp_build_data(uint8_t* segment, uint32_t capacity, uint16_t source_port,
                       uint16_t destination_port, uint32_t sequence, uint32_t acknowledgment,
                       const uint8_t* payload, uint16_t payload_length) {
    uint16_t i; uint32_t total = NET_TCP_HEADER_SIZE + payload_length;
    if (!segment || (!payload && payload_length != 0U) || total > capacity) return -1;
    if (build_control(segment, capacity, source_port, destination_port, sequence, acknowledgment, NET_TCP_FLAG_ACK) < 0) return -2;
    for (i = 0; i < payload_length; ++i) segment[NET_TCP_HEADER_SIZE + i] = payload[i];
    return (int)total;
}

uint16_t net_tcp_checksum_ipv4(const uint8_t source_ip[4], const uint8_t destination_ip[4],
                               const uint8_t* segment, uint16_t length) {
    uint32_t sum = 0U; uint16_t i;
    if (!source_ip || !destination_ip || !segment || length == 0U) return 0U;
    sum += ((uint16_t)source_ip[0] << 8) | source_ip[1]; sum += ((uint16_t)source_ip[2] << 8) | source_ip[3];
    sum += ((uint16_t)destination_ip[0] << 8) | destination_ip[1]; sum += ((uint16_t)destination_ip[2] << 8) | destination_ip[3];
    sum += NET_TCP_PROTOCOL; sum += length;
    for (i=0; i+1U<length; i+=2U) { sum += get16(segment+i); while (sum>>16) sum=(sum&0xffffU)+(sum>>16); }
    if (length&1U) sum += (uint16_t)segment[length-1U]<<8;
    while (sum>>16) sum=(sum&0xffffU)+(sum>>16);
    return (uint16_t)~sum;
}

int net_tcp_build_syn_ipv4(uint8_t* packet, uint32_t capacity, const uint8_t source_ip[4],
                           const uint8_t destination_ip[4], uint16_t source_port,
                           uint16_t destination_port, uint32_t sequence) {
    uint16_t i, checksum; uint32_t sum=0U;
    if (!packet || !source_ip || !destination_ip || capacity < 40U) return -1;
    for (i=0;i<40U;++i) packet[i]=0U;
    packet[0]=0x45U; put16(packet+2,40U); packet[8]=64U; packet[9]=NET_TCP_PROTOCOL;
    for (i=0;i<4U;++i) { packet[12U+i]=source_ip[i]; packet[16U+i]=destination_ip[i]; }
    if (net_tcp_build_syn(packet+20U,capacity-20U,source_port,destination_port,sequence)<0) return -2;
    checksum=net_tcp_checksum_ipv4(source_ip,destination_ip,packet+20U,20U); put16(packet+36U,checksum);
    for (i=0;i<20U;i+=2U) { sum+=get16(packet+i); while(sum>>16) sum=(sum&0xffffU)+(sum>>16); }
    put16(packet+10U,(uint16_t)~sum); return 40;
}

int net_tcp_is_syn_ack_for(const net_tcp_view_t* view,uint16_t local_port,uint16_t remote_port,
                           uint32_t local_sequence,uint32_t* remote_sequence) {
    if (!view || !remote_sequence || local_port==0U || remote_port==0U) return -1;
    if (view->source_port!=remote_port || view->destination_port!=local_port ||
        (view->flags&(NET_TCP_FLAG_SYN|NET_TCP_FLAG_ACK))!=(NET_TCP_FLAG_SYN|NET_TCP_FLAG_ACK) ||
        view->acknowledgment!=local_sequence+1U) return -2;
    *remote_sequence=view->sequence; return 0;
}

int net_tcp_connection_open(net_tcp_connection_t* connection,uint16_t local_port,uint16_t remote_port,uint32_t local_sequence) {
    if (!connection || local_port==0U || remote_port==0U) return -1;
    connection->local_port=local_port; connection->remote_port=remote_port;
    connection->local_sequence=local_sequence+1U; connection->remote_sequence=0U;
    connection->pending_payload=0; connection->pending_length=0U; connection->retransmit_count=0U;
    connection->retransmit_limit=0U; connection->receive_window=0xffffU;
    connection->state=NET_TCP_STATE_SYN_SENT; return 0;
}

int net_tcp_connection_accept_syn_ack(net_tcp_connection_t* connection,const net_tcp_view_t* view) {
    uint32_t remote_sequence;
    if (!connection || !view || connection->state!=NET_TCP_STATE_SYN_SENT) return -1;
    if (net_tcp_is_syn_ack_for(view,connection->local_port,connection->remote_port,
                               connection->local_sequence-1U,&remote_sequence)!=0) return -2;
    connection->remote_sequence=remote_sequence+1U; connection->state=NET_TCP_STATE_ESTABLISHED; return 0;
}

int net_tcp_connection_build_ack(const net_tcp_connection_t* connection,uint8_t* segment,uint32_t capacity) {
    if (!connection || (connection->state != NET_TCP_STATE_ESTABLISHED &&
                        connection->state != NET_TCP_STATE_FIN_WAIT_2 &&
                        connection->state != NET_TCP_STATE_CLOSE_WAIT)) return -1;
    return net_tcp_build_ack(segment,capacity,connection->local_port,connection->remote_port,
                             connection->local_sequence,connection->remote_sequence);
}

int net_tcp_connection_build_data(net_tcp_connection_t* connection,uint8_t* segment,uint32_t capacity,
                                  const uint8_t* payload,uint16_t payload_length,uint8_t retransmit_limit) {
    int length;
    if (!connection || !segment || !payload || payload_length == 0U || connection->state != NET_TCP_STATE_ESTABLISHED) return -1;
    length = net_tcp_build_data(segment, capacity, connection->local_port, connection->remote_port,
                                connection->local_sequence, connection->remote_sequence,
                                payload, payload_length);
    if (length < 0) return -2;
    if (net_tcp_connection_track_send(connection, payload, payload_length, retransmit_limit) != 0) return -3;
    return length;
}

int net_tcp_connection_build_tls_record(net_tcp_connection_t* connection,uint8_t* segment,uint32_t capacity,
                                        uint8_t* record,uint32_t record_capacity,uint8_t content_type,
                                        const uint8_t* payload,uint16_t payload_length,uint8_t retransmit_limit) {
    int record_length;
    if (!record) return -1;
    record_length = net_tls_record_build(record, record_capacity, content_type, payload, payload_length);
    if (record_length < 0 || record_length > 0xffff) return -2;
    return net_tcp_connection_build_data(connection, segment, capacity, record, (uint16_t)record_length, retransmit_limit);
}

int net_tcp_connection_commit_send(net_tcp_connection_t* connection,uint16_t payload_length) {
    if (!connection || connection->state!=NET_TCP_STATE_ESTABLISHED || payload_length == 0U) return -1;
    connection->local_sequence += payload_length; return 0;
}

int net_tcp_connection_accept_data(net_tcp_connection_t* connection,const net_tcp_view_t* view,
                                   uint16_t* accepted_length) {
    if (!connection || !view || !accepted_length || connection->state != NET_TCP_STATE_ESTABLISHED) return -1;
    if (view->source_port != connection->remote_port || view->destination_port != connection->local_port ||
        (view->flags & NET_TCP_FLAG_ACK) == 0U || view->sequence != connection->remote_sequence ||
        view->acknowledgment > connection->local_sequence || view->payload_length > connection->receive_window) return -2;
    *accepted_length = view->payload_length;
    connection->remote_sequence += view->payload_length;
    connection->receive_window = (uint16_t)(connection->receive_window - view->payload_length);
    return 0;
}

int net_tcp_connection_accept_tls_record(net_tcp_connection_t* connection,const net_tcp_view_t* view,
                                         net_tls_record_view_t* record,uint16_t* consumed) {
    uint16_t accepted;
    if (!connection || !view || !record || !consumed) return -1;
    if (net_tls_record_parse_stream(view->payload, view->payload_length, record, consumed) != 0) return -2;
    if (*consumed != view->payload_length) return -3;
    if (net_tcp_connection_accept_data(connection, view, &accepted) != 0) return -4;
    return accepted == *consumed ? 0 : -5;
}
int net_tcp_connection_accept_tls_handshake(net_tcp_connection_t* connection,const net_tcp_view_t* view,
                                            net_tls_handshake_t* handshake,net_tls_transcript_t* transcript,
                                            uint16_t* consumed) {
    net_tcp_connection_t previous; net_tls_record_view_t record; int status;
    if(!connection||!view||!handshake||!consumed)return -1;
    previous=*connection;
    status=net_tcp_connection_accept_tls_record(connection,view,&record,consumed);
    if(status!=0)return -2;
    if(record.content_type!=NET_TLS_CONTENT_HANDSHAKE){*connection=previous;return -3;}
    status=net_tls_handshake_accept_server_message(handshake,record.payload,record.payload_length,transcript);
    if(status!=0){*connection=previous;return -4;}
    return 0;
}
int net_tcp_connection_accept_tls_postflight(net_tcp_connection_t* connection,const net_tcp_view_t* view,
                                             net_tls_handshake_t* handshake,net_tls_transcript_t* transcript,
                                             const uint8_t expected_verify_data[12],uint16_t* consumed){
    net_tcp_connection_t previous_connection; net_tls_handshake_t previous_handshake; net_tls_record_view_t record; uint16_t previous_transcript_length=0U; int status;
    if(!connection||!view||!handshake||!consumed)return -1;
    previous_connection=*connection; previous_handshake=*handshake; if(transcript)previous_transcript_length=transcript->length;
    status=net_tcp_connection_accept_tls_record(connection,view,&record,consumed);
    if(status!=0)return -2;
    if(record.content_type==NET_TLS_CONTENT_CHANGE_CIPHER_SPEC)status=net_tls_handshake_accept_server_change_cipher_spec(handshake,record.payload,record.payload_length);
    else if(record.content_type==NET_TLS_CONTENT_HANDSHAKE){
        status=net_tls_handshake_accept_server_finished(handshake,record.payload,record.payload_length,expected_verify_data);
        if(status==0&&transcript&&net_tls_transcript_append(transcript,record.payload,record.payload_length)!=0)status=-3;
    } else status=-4;
    if(status!=0){*connection=previous_connection;*handshake=previous_handshake;if(transcript)transcript->length=previous_transcript_length;return -5;}
    return 0;
}

int net_tcp_connection_set_receive_window(net_tcp_connection_t* connection,uint16_t receive_window) {
    if (!connection) return -1;
    connection->receive_window = receive_window; return 0;
}

int net_tcp_connection_track_send(net_tcp_connection_t* connection,const uint8_t* payload,
                                  uint16_t payload_length,uint8_t retransmit_limit) {
    if (!connection || !payload || payload_length == 0U || connection->state != NET_TCP_STATE_ESTABLISHED) return -1;
    connection->pending_payload=payload; connection->pending_length=payload_length;
    connection->retransmit_count=0U; connection->retransmit_limit=retransmit_limit; return 0;
}

int net_tcp_connection_retransmit_allowed(const net_tcp_connection_t* connection) {
    if (!connection || !connection->pending_payload || connection->pending_length == 0U) return 0;
    return connection->retransmit_count < connection->retransmit_limit;
}

int net_tcp_connection_note_retransmit(net_tcp_connection_t* connection) {
    if (!net_tcp_connection_retransmit_allowed(connection)) return -1;
    connection->retransmit_count++; return 0;
}

int net_tcp_connection_begin_close(net_tcp_connection_t* connection,uint8_t* segment,uint32_t capacity) {
    int length;
    if (!connection || connection->state != NET_TCP_STATE_ESTABLISHED) return -1;
    length = net_tcp_build_fin_ack(segment, capacity, connection->local_port, connection->remote_port,
                                   connection->local_sequence, connection->remote_sequence);
    if (length < 0) return -2;
    connection->local_sequence++; connection->state = NET_TCP_STATE_FIN_WAIT_1;
    return length;
}

int net_tcp_connection_accept_ack(net_tcp_connection_t* connection,const net_tcp_view_t* view) {
    if (!connection || !view || (connection->state != NET_TCP_STATE_ESTABLISHED && connection->state != NET_TCP_STATE_FIN_WAIT_1)) return -1;
    if (view->source_port != connection->remote_port || view->destination_port != connection->local_port ||
        (view->flags & NET_TCP_FLAG_ACK) == 0U || view->acknowledgment > connection->local_sequence) return -2;
    if (connection->pending_length != 0U && view->acknowledgment < connection->local_sequence) return -3;
    if (connection->pending_length != 0U) {
        connection->pending_payload = 0; connection->pending_length = 0U;
        connection->retransmit_count = 0U; connection->retransmit_limit = 0U;
    }
    if (connection->state == NET_TCP_STATE_FIN_WAIT_1 && view->acknowledgment == connection->local_sequence)
        connection->state = NET_TCP_STATE_FIN_WAIT_2;
    return 0;
}

int net_tcp_connection_accept_fin(net_tcp_connection_t* connection,const net_tcp_view_t* view) {
    if (!connection || !view || (connection->state != NET_TCP_STATE_ESTABLISHED && connection->state != NET_TCP_STATE_FIN_WAIT_2)) return -1;
    if (view->source_port != connection->remote_port || view->destination_port != connection->local_port ||
        (view->flags & NET_TCP_FLAG_FIN) == 0U || view->sequence != connection->remote_sequence) return -2;
    connection->remote_sequence++;
    connection->state = (connection->state == NET_TCP_STATE_FIN_WAIT_2) ? NET_TCP_STATE_CLOSED : NET_TCP_STATE_CLOSE_WAIT;
    return 0;
}

int net_tcp_parse(const uint8_t* segment,uint32_t length,net_tcp_view_t* out) {
    uint8_t header_words; uint16_t header_size;
    if (!segment || !out || length<NET_TCP_HEADER_SIZE) return -1;
    header_words=(uint8_t)(segment[12]>>4); header_size=(uint16_t)header_words*4U;
    if (header_words<5U || header_size>length) return -2;
    out->source_port=get16(segment); out->destination_port=get16(segment+2);
    if (out->source_port==0U || out->destination_port==0U) return -3;
    out->sequence=get32(segment+4); out->acknowledgment=get32(segment+8); out->flags=(uint8_t)(segment[13]&0x3fU);
    out->payload=segment+header_size; out->payload_length=(uint16_t)(length-header_size); return 0;
}
