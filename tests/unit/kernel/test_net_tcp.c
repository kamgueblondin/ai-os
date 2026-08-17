#include "../../framework/unity.h"
#include "../../../kernel/net_tcp.h"

void setUp(void) {}
void tearDown(void) {}

void test_build_and_parse_syn_ack(void) {
    uint8_t segment[32] = {0}; net_tcp_view_t view;
    TEST_ASSERT_EQUAL(20, net_tcp_build_syn(segment, sizeof(segment), 49152, 443, 0x10203040U));
    TEST_ASSERT_EQUAL(0, net_tcp_parse(segment, 20, &view));
    TEST_ASSERT_EQUAL(49152, view.source_port); TEST_ASSERT_EQUAL(443, view.destination_port);
    TEST_ASSERT_EQUAL(0x10203040U, view.sequence); TEST_ASSERT_EQUAL(NET_TCP_FLAG_SYN, view.flags);
    segment[13] = NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK; segment[8]=0x10; segment[9]=0x20; segment[10]=0x30; segment[11]=0x41;
    TEST_ASSERT_EQUAL(0, net_tcp_parse(segment, 20, &view));
    TEST_ASSERT_EQUAL(NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK, view.flags);
    { uint8_t source[4] = {10,0,2,15}; uint8_t destination[4] = {10,0,2,2}; uint16_t checksum = net_tcp_checksum_ipv4(source, destination, segment, 20); TEST_ASSERT_NOT_EQUAL(0, checksum); TEST_ASSERT_EQUAL(checksum, net_tcp_checksum_ipv4(source, destination, segment, 20)); }
    TEST_ASSERT_EQUAL(0x10203041U, view.acknowledgment);
    { uint32_t remote_sequence = 0U;
      view.source_port = 443; view.destination_port = 49152;
      TEST_ASSERT_EQUAL(0, net_tcp_is_syn_ack_for(&view, 49152, 443, 0x10203040U, &remote_sequence));
      TEST_ASSERT_EQUAL(0x10203040U, remote_sequence);
      view.acknowledgment = 7U; TEST_ASSERT_NOT_EQUAL(0, net_tcp_is_syn_ack_for(&view, 49152, 443, 0x10203040U, &remote_sequence)); }
    segment[0] = 0; segment[1] = 0; TEST_ASSERT_NOT_EQUAL(0, net_tcp_parse(segment, 20, &view));
    { uint8_t packet[48] = {0}; uint8_t src[4] = {10,0,2,15}; uint8_t dst[4] = {10,0,2,2};
      TEST_ASSERT_EQUAL(40, net_tcp_build_syn_ipv4(packet, sizeof(packet), src, dst, 49152, 443, 0x10203040U));
      TEST_ASSERT_EQUAL(0x45, packet[0]); TEST_ASSERT_EQUAL(NET_TCP_PROTOCOL, packet[9]);
      TEST_ASSERT_EQUAL(49152, (packet[20] << 8) | packet[21]); TEST_ASSERT_EQUAL(NET_TCP_FLAG_SYN, packet[33] & 0x3f); }
}

void test_connection_builds_first_ack(void) {
    uint8_t segment[20] = {0}; net_tcp_view_t view; net_tcp_connection_t connection;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    TEST_ASSERT_EQUAL(NET_TCP_STATE_SYN_SENT, connection.state);
    view.source_port = 443; view.destination_port = 49152; view.sequence = 700U;
    view.acknowledgment = 101U; view.flags = NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &view));
    TEST_ASSERT_EQUAL(NET_TCP_STATE_ESTABLISHED, connection.state);
    TEST_ASSERT_EQUAL(701U, connection.remote_sequence);
    TEST_ASSERT_EQUAL(20, net_tcp_connection_build_ack(&connection, segment, sizeof(segment)));
    TEST_ASSERT_EQUAL(0, net_tcp_parse(segment, 20, &view));
    TEST_ASSERT_EQUAL(49152, view.source_port); TEST_ASSERT_EQUAL(443, view.destination_port);
    TEST_ASSERT_EQUAL(101U, view.sequence); TEST_ASSERT_EQUAL(701U, view.acknowledgment);
    TEST_ASSERT_EQUAL(NET_TCP_FLAG_ACK, view.flags);
    view.acknowledgment = 102U;
    TEST_ASSERT_NOT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &view));
}

void test_build_and_parse_ack_payload(void) {
    uint8_t segment[32] = {0}; uint8_t payload[4] = {'P','I','N','G'}; net_tcp_view_t view;
    TEST_ASSERT_EQUAL(24, net_tcp_build_data(segment, sizeof(segment), 49152, 443, 101U, 701U, payload, sizeof(payload)));
    TEST_ASSERT_EQUAL(0, net_tcp_parse(segment, 24, &view));
    TEST_ASSERT_EQUAL(NET_TCP_FLAG_ACK, view.flags); TEST_ASSERT_EQUAL(101U, view.sequence);
    TEST_ASSERT_EQUAL(701U, view.acknowledgment); TEST_ASSERT_EQUAL(4, view.payload_length);
    TEST_ASSERT_EQUAL('P', view.payload[0]); TEST_ASSERT_EQUAL('G', view.payload[3]);
    TEST_ASSERT_NOT_EQUAL(0, net_tcp_build_data(segment, 23, 49152, 443, 101U, 701U, payload, sizeof(payload)));
}

void test_connection_advances_sequences_and_accepts_data(void) {
    net_tcp_connection_t connection; net_tcp_view_t view; uint16_t accepted = 0U;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    view.source_port = 443; view.destination_port = 49152; view.sequence = 700U; view.acknowledgment = 101U;
    view.flags = NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &view));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_commit_send(&connection, 4U));
    TEST_ASSERT_EQUAL(105U, connection.local_sequence);
    view.flags = NET_TCP_FLAG_ACK; view.sequence = 701U; view.acknowledgment = 105U; view.payload_length = 3U;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_data(&connection, &view, &accepted));
    TEST_ASSERT_EQUAL(3U, accepted); TEST_ASSERT_EQUAL(704U, connection.remote_sequence);
    view.sequence = 703U; TEST_ASSERT_NOT_EQUAL(0, net_tcp_connection_accept_data(&connection, &view, &accepted));
    TEST_ASSERT_NOT_EQUAL(0, net_tcp_connection_commit_send(&connection, 0U));
}

void test_tls_record_is_composed_into_tcp(void) {
    net_tcp_connection_t connection; uint8_t segment[64], record[32]; uint8_t hello[3] = {'H','I','!'}; net_tls_record_view_t view;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    { net_tcp_view_t syn_ack = {443, 49152, 700U, 101U, NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK, 0, 0};
      TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &syn_ack)); }
    TEST_ASSERT_EQUAL(28, net_tcp_connection_build_tls_record(&connection, segment, sizeof(segment), record, sizeof(record), NET_TLS_CONTENT_HANDSHAKE, hello, sizeof(hello), 2U));
    TEST_ASSERT_EQUAL(8, connection.pending_length); TEST_ASSERT_EQUAL(record, connection.pending_payload);
    TEST_ASSERT_EQUAL(0, net_tls_record_parse(record, 8, &view)); TEST_ASSERT_EQUAL(3U, view.payload_length);
}

void test_accept_tls_record_on_tcp_view(void) {
    net_tcp_connection_t connection; uint8_t record[16], payload[3] = {'T','L','S'}; net_tcp_view_t view; net_tls_record_view_t parsed; uint16_t consumed;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    view = (net_tcp_view_t){443,49152,700U,101U,NET_TCP_FLAG_SYN|NET_TCP_FLAG_ACK,0,0}; TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection,&view));
    TEST_ASSERT_EQUAL(8, net_tls_record_build(record,sizeof(record),NET_TLS_CONTENT_HANDSHAKE,payload,3));
    view = (net_tcp_view_t){443,49152,701U,101U,NET_TCP_FLAG_ACK,record,8};
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_tls_record(&connection,&view,&parsed,&consumed)); TEST_ASSERT_EQUAL(8,consumed); TEST_ASSERT_EQUAL(3,parsed.payload_length); TEST_ASSERT_EQUAL(709U,connection.remote_sequence);
    view.payload_length = 7U; TEST_ASSERT_NOT_EQUAL(0, net_tcp_connection_accept_tls_record(&connection,&view,&parsed,&consumed));
}

void test_accept_tls_handshake_transactional(void) {
    net_tcp_connection_t connection; net_tcp_view_t view; net_tls_handshake_t handshake; net_tls_transcript_t transcript; uint8_t transcript_buffer[64]={0},hello[42]={0},record[64]={0},other[16]={0}; uint16_t consumed=0U;
    hello[0]=NET_TLS_HANDSHAKE_SERVER_HELLO; hello[3]=38; hello[4]=3; hello[5]=3; hello[38]=0; hello[39]=0; hello[40]=0x9c; hello[41]=0;
    TEST_ASSERT_EQUAL(0,net_tcp_connection_open(&connection,49152,443,100U));
    view=(net_tcp_view_t){443,49152,700U,101U,NET_TCP_FLAG_SYN|NET_TCP_FLAG_ACK,0,0}; TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_syn_ack(&connection,&view));
    TEST_ASSERT_EQUAL(0,net_tls_handshake_init(&handshake)); TEST_ASSERT_EQUAL(0,net_tls_handshake_note_client_hello(&handshake)); TEST_ASSERT_EQUAL(0,net_tls_transcript_init(&transcript,transcript_buffer,sizeof(transcript_buffer)));
    TEST_ASSERT_EQUAL(47,net_tls_record_build(record,sizeof(record),NET_TLS_CONTENT_HANDSHAKE,hello,sizeof(hello)));
    view=(net_tcp_view_t){443,49152,701U,101U,NET_TCP_FLAG_ACK,record,47};
    TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_tls_handshake(&connection,&view,&handshake,&transcript,&consumed)); TEST_ASSERT_EQUAL(47,consumed); TEST_ASSERT_EQUAL(NET_TLS_HANDSHAKE_SERVER_HELLO_RECEIVED,handshake.state); TEST_ASSERT_EQUAL(42,transcript.length); TEST_ASSERT_EQUAL(748U,connection.remote_sequence);
    TEST_ASSERT_EQUAL(9,net_tls_record_build(other,sizeof(other),23U,(uint8_t[]){14,0,0,0},4)); view=(net_tcp_view_t){443,49152,748U,101U,NET_TCP_FLAG_ACK,other,9};
    TEST_ASSERT_NOT_EQUAL(0,net_tcp_connection_accept_tls_handshake(&connection,&view,&handshake,&transcript,&consumed)); TEST_ASSERT_EQUAL(748U,connection.remote_sequence);
}

void test_accept_tls_postflight_transactional(void) {
    net_tcp_connection_t connection; net_tcp_view_t view; net_tls_handshake_t handshake; net_tls_transcript_t transcript; uint8_t transcript_buffer[32]={0},record[32]={0},verify[12]={0},bad[12]={0}; uint16_t consumed=0U; uint8_t i;
    for(i=0U;i<12U;i++)verify[i]=i;
    TEST_ASSERT_EQUAL(0,net_tcp_connection_open(&connection,49152,443,100U)); view=(net_tcp_view_t){443,49152,700U,101U,NET_TCP_FLAG_SYN|NET_TCP_FLAG_ACK,0,0}; TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_syn_ack(&connection,&view));
    TEST_ASSERT_EQUAL(0,net_tls_handshake_init(&handshake)); handshake.state=NET_TLS_HANDSHAKE_FINISHED_SENT; TEST_ASSERT_EQUAL(0,net_tls_transcript_init(&transcript,transcript_buffer,sizeof(transcript_buffer)));
    TEST_ASSERT_EQUAL(6,net_tls_change_cipher_spec_build(record,sizeof(record))); view=(net_tcp_view_t){443,49152,701U,101U,NET_TCP_FLAG_ACK,record,6}; TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_tls_postflight(&connection,&view,&handshake,&transcript,verify,&consumed)); TEST_ASSERT_EQUAL(707U,connection.remote_sequence); TEST_ASSERT_EQUAL(NET_TLS_HANDSHAKE_SERVER_CHANGE_CIPHER_SPEC_RECEIVED,handshake.state);
    TEST_ASSERT_EQUAL(21,net_tls_finished_build(record,sizeof(record),verify)); view=(net_tcp_view_t){443,49152,707U,101U,NET_TCP_FLAG_ACK,record,21}; TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_tls_postflight(&connection,&view,&handshake,&transcript,verify,&consumed)); TEST_ASSERT_EQUAL(728U,connection.remote_sequence); TEST_ASSERT_EQUAL(16,transcript.length); TEST_ASSERT_EQUAL(1,net_tls_handshake_is_complete(&handshake));
    TEST_ASSERT_EQUAL(0,net_tls_handshake_init(&handshake)); handshake.state=NET_TLS_HANDSHAKE_SERVER_CHANGE_CIPHER_SPEC_RECEIVED; view.sequence=728U; TEST_ASSERT_NOT_EQUAL(0,net_tcp_connection_accept_tls_postflight(&connection,&view,&handshake,&transcript,bad,&consumed)); TEST_ASSERT_EQUAL(728U,connection.remote_sequence);
}

void test_tcp_tls_aes_gcm_transport(void) {
    net_tcp_connection_t client_connection,server_connection; net_tcp_view_t syn_ack,view; net_tls_aes128_gcm_key_block_t key_block; net_tls_aes_gcm_session_t client_session,server_session; net_tls_record_view_t record_view; uint8_t key_material[40]={0},segment[64]={0},record[48]={0},plaintext[4]={'p','i','n','g'},decoded[8]={0},i; uint16_t consumed=0U;
    for(i=0U;i<40U;i++)key_material[i]=(uint8_t)(i+1U);key_block=(net_tls_aes128_gcm_key_block_t){key_material,key_material+16U,key_material+32U,key_material+36U};
    TEST_ASSERT_EQUAL(0,net_tls_aes_gcm_session_init(&client_session,&key_block,1));TEST_ASSERT_EQUAL(0,net_tls_aes_gcm_session_init(&server_session,&key_block,0));
    TEST_ASSERT_EQUAL(0,net_tcp_connection_open(&client_connection,49152,443,100U));syn_ack=(net_tcp_view_t){443,49152,700U,101U,NET_TCP_FLAG_SYN|NET_TCP_FLAG_ACK,0,0};TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_syn_ack(&client_connection,&syn_ack));
    TEST_ASSERT_EQUAL(0,net_tcp_connection_open(&server_connection,443,49152,700U));syn_ack=(net_tcp_view_t){49152,443,100U,701U,NET_TCP_FLAG_SYN|NET_TCP_FLAG_ACK,0,0};TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_syn_ack(&server_connection,&syn_ack));
    TEST_ASSERT_EQUAL(53,net_tcp_connection_build_tls_aes_gcm(&client_connection,&client_session,segment,sizeof(segment),record,sizeof(record),NET_TLS_CONTENT_APPLICATION_DATA,plaintext,sizeof(plaintext),1));TEST_ASSERT_EQUAL(1,client_session.write_sequence);TEST_ASSERT_EQUAL(0,net_tcp_parse(segment,53,&view));TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_tls_aes_gcm(&server_connection,&server_session,&view,decoded,sizeof(decoded),&record_view,&consumed));TEST_ASSERT_EQUAL(33,consumed);TEST_ASSERT_EQUAL(1,server_session.read_sequence);TEST_ASSERT_EQUAL('p',decoded[0]);TEST_ASSERT_EQUAL('g',decoded[3]);
}

void test_tcp_tls_x25519_flight_postflight_and_application(void) {net_tcp_connection_t client,server;net_tcp_view_t view,syn_ack;net_tls_handshake_t handshake;net_tls_x25519_context_t context={0};net_tls_transcript_t transcript;net_tls_aes_gcm_session_t client_session,server_session;net_tls_aes128_gcm_key_block_t block;net_tls_record_view_t opened;uint8_t client_private[32]={5U},server_private[32]={11U},server_public[32]={0},client_random[32]={0},server_random[32]={0},transcript_buffer[160]={0},master[48]={0},key_block[40]={0},tcp_segment[160]={0},flight[128]={0},ccs[8]={0},server_record[48]={0},bad_record[48]={0},server_finished[16]={0},server_verify[12]={0},plaintext[32]={0},app_record[48]={0},app_segment[64]={0},app_plain[8]={0},done[4]={14U,0U,0U,0U};uint32_t x_workspace[136]={0},flight_length=99U;uint8_t prf_workspace[256]={0},hash[32]={0},i;uint16_t consumed=0U,accepted=0U;for(i=0U;i<32U;i++){client_random[i]=i;server_random[i]=(uint8_t)(i+32U);}TEST_ASSERT_EQUAL(0,x25519_public_key(server_public,server_private,x_workspace,136U));TEST_ASSERT_EQUAL(0,net_tcp_connection_open(&client,49152U,443U,100U));syn_ack=(net_tcp_view_t){443U,49152U,700U,101U,NET_TCP_FLAG_SYN|NET_TCP_FLAG_ACK,0,0};TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_syn_ack(&client,&syn_ack));TEST_ASSERT_EQUAL(0,net_tcp_connection_open(&server,443U,49152U,700U));syn_ack=(net_tcp_view_t){49152U,443U,100U,701U,NET_TCP_FLAG_SYN|NET_TCP_FLAG_ACK,0,0};TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_syn_ack(&server,&syn_ack));TEST_ASSERT_EQUAL(0,net_tls_handshake_init(&handshake));handshake.state=NET_TLS_HANDSHAKE_SERVER_HELLO_DONE_RECEIVED;handshake.cipher_suite=NET_TLS_CIPHER_ECDHE_RSA_WITH_AES_128_GCM_SHA256;handshake.server_named_curve=NET_TLS_NAMED_CURVE_X25519;handshake.server_public_key=server_public;handshake.server_public_key_length=32U;handshake.server_random=server_random;TEST_ASSERT_EQUAL(0,net_tls_transcript_init(&transcript,transcript_buffer,sizeof(transcript_buffer)));TEST_ASSERT_EQUAL(0,net_tls_transcript_append(&transcript,done,sizeof(done)));TEST_ASSERT_NOT_EQUAL(0,net_tcp_connection_build_tls_x25519_flight(&client,&handshake,&context,client_private,client_random,&transcript,master,key_block,&client_session,tcp_segment,112U,flight,sizeof(flight),&flight_length,x_workspace,136U,prf_workspace,sizeof(prf_workspace),1U));TEST_ASSERT_EQUAL(0,flight_length);TEST_ASSERT_EQUAL(NET_TLS_HANDSHAKE_SERVER_HELLO_DONE_RECEIVED,handshake.state);TEST_ASSERT_EQUAL(4,transcript.length);TEST_ASSERT_EQUAL(113,net_tcp_connection_build_tls_x25519_flight(&client,&handshake,&context,client_private,client_random,&transcript,master,key_block,&client_session,tcp_segment,sizeof(tcp_segment),flight,sizeof(flight),&flight_length,x_workspace,136U,prf_workspace,sizeof(prf_workspace),1U));TEST_ASSERT_EQUAL(93,flight_length);TEST_ASSERT_EQUAL(NET_TLS_HANDSHAKE_FINISHED_SENT,handshake.state);TEST_ASSERT_EQUAL(0,net_tcp_parse(tcp_segment,113U,&view));TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_data(&server,&view,&accepted));TEST_ASSERT_EQUAL(93,accepted);TEST_ASSERT_EQUAL(0,net_tcp_connection_commit_send(&client,flight_length));TEST_ASSERT_EQUAL(194,client.local_sequence);block=(net_tls_aes128_gcm_key_block_t){key_block,key_block+16U,key_block+32U,key_block+36U};TEST_ASSERT_EQUAL(0,net_tls_aes_gcm_session_init(&server_session,&block,0U));TEST_ASSERT_EQUAL(6,net_tls_change_cipher_spec_build(ccs,sizeof(ccs)));view=(net_tcp_view_t){443U,49152U,701U,194U,NET_TCP_FLAG_ACK,ccs,6U};TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_tls_x25519_postflight(&client,&handshake,&transcript,master,&client_session,&view,plaintext,sizeof(plaintext),prf_workspace,sizeof(prf_workspace),&consumed));TEST_ASSERT_EQUAL(NET_TLS_HANDSHAKE_SERVER_CHANGE_CIPHER_SPEC_RECEIVED,handshake.state);TEST_ASSERT_EQUAL(0,net_tls_server_finished_verify_data(server_verify,master,&transcript,hash,prf_workspace,sizeof(prf_workspace)));server_finished[0]=NET_TLS_HANDSHAKE_FINISHED;server_finished[3]=12U;for(i=0U;i<12U;i++)server_finished[4U+i]=server_verify[i];TEST_ASSERT_EQUAL(45,net_tls_aes_gcm_session_build(&server_session,server_record,sizeof(server_record),NET_TLS_CONTENT_HANDSHAKE,server_finished,sizeof(server_finished)));for(i=0U;i<45U;i++)bad_record[i]=server_record[i];bad_record[44]^=1U;view=(net_tcp_view_t){443U,49152U,707U,194U,NET_TCP_FLAG_ACK,bad_record,45U};TEST_ASSERT_NOT_EQUAL(0,net_tcp_connection_accept_tls_x25519_postflight(&client,&handshake,&transcript,master,&client_session,&view,plaintext,sizeof(plaintext),prf_workspace,sizeof(prf_workspace),&consumed));TEST_ASSERT_EQUAL(707,client.remote_sequence);TEST_ASSERT_EQUAL(NET_TLS_HANDSHAKE_SERVER_CHANGE_CIPHER_SPEC_RECEIVED,handshake.state);view.payload=server_record;TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_tls_x25519_postflight(&client,&handshake,&transcript,master,&client_session,&view,plaintext,sizeof(plaintext),prf_workspace,sizeof(prf_workspace),&consumed));TEST_ASSERT_EQUAL(1,net_tls_handshake_is_complete(&handshake));TEST_ASSERT_EQUAL(752,client.remote_sequence);TEST_ASSERT_EQUAL(1,client_session.read_sequence);TEST_ASSERT_EQUAL(0,net_tcp_connection_commit_send(&server,6U));TEST_ASSERT_EQUAL(0,net_tcp_connection_commit_send(&server,45U));server_session.read_sequence=1U;TEST_ASSERT_EQUAL(52,net_tcp_connection_build_tls_aes_gcm(&client,&client_session,app_segment,sizeof(app_segment),app_record,sizeof(app_record),NET_TLS_CONTENT_APPLICATION_DATA,(uint8_t[]){'o','k','!'},3U,1U));TEST_ASSERT_EQUAL(0,net_tcp_parse(app_segment,52U,&view));TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_tls_aes_gcm(&server,&server_session,&view,app_plain,sizeof(app_plain),&opened,&consumed));TEST_ASSERT_EQUAL(3,opened.payload_length);TEST_ASSERT_EQUAL('o',app_plain[0]);TEST_ASSERT_EQUAL('!',app_plain[2]);}
void test_receive_window_is_bounded(void) {
    net_tcp_connection_t connection; net_tcp_view_t view; uint16_t accepted = 0U;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    view = (net_tcp_view_t){443, 49152, 700U, 101U, NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK, 0, 0};
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &view));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_set_receive_window(&connection, 3U));
    view.flags = NET_TCP_FLAG_ACK; view.sequence = 701U; view.acknowledgment = 101U; view.payload_length = 2U;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_data(&connection, &view, &accepted));
    TEST_ASSERT_EQUAL(1U, connection.receive_window);
    view.sequence = 703U; view.payload_length = 2U;
    TEST_ASSERT_NOT_EQUAL(0, net_tcp_connection_accept_data(&connection, &view, &accepted));
}

void test_build_data_tracks_until_commit(void) {
    net_tcp_connection_t connection; uint8_t segment[24]; uint8_t payload[4] = {'P','I','N','G'};
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    { net_tcp_view_t syn_ack = {443, 49152, 700U, 101U, NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK, 0, 0};
      TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &syn_ack)); }
    TEST_ASSERT_EQUAL(24, net_tcp_connection_build_data(&connection, segment, sizeof(segment), payload, sizeof(payload), 3U));
    TEST_ASSERT_EQUAL(101U, connection.local_sequence); TEST_ASSERT_EQUAL(payload, connection.pending_payload);
    TEST_ASSERT_EQUAL(0, net_tcp_connection_commit_send(&connection, sizeof(payload)));
    TEST_ASSERT_EQUAL(105U, connection.local_sequence); TEST_ASSERT_EQUAL(1, net_tcp_connection_retransmit_allowed(&connection));
}

void test_ack_confirms_pending_payload(void) {
    net_tcp_connection_t connection; uint8_t payload[2] = {'O','K'}; net_tcp_view_t view;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    view = (net_tcp_view_t){443, 49152, 700U, 101U, NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK, 0, 0};
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &view));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_track_send(&connection, payload, sizeof(payload), 2U));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_commit_send(&connection, sizeof(payload)));
    view = (net_tcp_view_t){443, 49152, 701U, 103U, NET_TCP_FLAG_ACK, 0, 0};
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_ack(&connection, &view));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_retransmit_allowed(&connection));
    view.acknowledgment = 104U; TEST_ASSERT_NOT_EQUAL(0, net_tcp_connection_accept_ack(&connection, &view));
}

void test_fin_close_transitions(void) {
    net_tcp_connection_t connection; net_tcp_view_t view; uint8_t segment[20] = {0};
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    view = (net_tcp_view_t){443, 49152, 700U, 101U, NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK, 0, 0};
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &view));
    TEST_ASSERT_EQUAL(20, net_tcp_connection_begin_close(&connection, segment, sizeof(segment)));
    TEST_ASSERT_EQUAL((NET_TCP_FLAG_FIN | NET_TCP_FLAG_ACK), segment[13]);
    TEST_ASSERT_EQUAL(NET_TCP_STATE_FIN_WAIT_1, connection.state); TEST_ASSERT_EQUAL(102U, connection.local_sequence);
    view.flags = NET_TCP_FLAG_ACK; view.acknowledgment = 102U; view.sequence = 700U;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_ack(&connection, &view));
    TEST_ASSERT_EQUAL(NET_TCP_STATE_FIN_WAIT_2, connection.state);
    view.flags = NET_TCP_FLAG_FIN | NET_TCP_FLAG_ACK; view.sequence = 701U;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_fin(&connection, &view));
    TEST_ASSERT_EQUAL(NET_TCP_STATE_CLOSED, connection.state);
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    view.flags = NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK; view.sequence = 700U; view.acknowledgment = 101U;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &view));
    view.flags = NET_TCP_FLAG_FIN | NET_TCP_FLAG_ACK; view.sequence = 701U; view.acknowledgment = 101U;
    TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_fin(&connection, &view));
    TEST_ASSERT_EQUAL(NET_TCP_STATE_CLOSE_WAIT, connection.state); TEST_ASSERT_EQUAL(702U, connection.remote_sequence);
    TEST_ASSERT_EQUAL(20, net_tcp_connection_build_ack(&connection, segment, sizeof(segment)));
    view.sequence = 704U; TEST_ASSERT_NOT_EQUAL(0, net_tcp_connection_accept_fin(&connection, &view));
}

void test_bounded_retransmission_metadata(void) {
    net_tcp_connection_t connection; uint8_t payload[2] = {'O','K'};
    TEST_ASSERT_EQUAL(0, net_tcp_connection_open(&connection, 49152, 443, 100U));
    { net_tcp_view_t syn_ack = {443, 49152, 700U, 101U, NET_TCP_FLAG_SYN | NET_TCP_FLAG_ACK, 0, 0};
      TEST_ASSERT_EQUAL(0, net_tcp_connection_accept_syn_ack(&connection, &syn_ack)); }
    TEST_ASSERT_EQUAL(0, net_tcp_connection_track_send(&connection, payload, sizeof(payload), 2U));
    TEST_ASSERT_EQUAL(1, net_tcp_connection_retransmit_allowed(&connection));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_note_retransmit(&connection));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_note_retransmit(&connection));
    TEST_ASSERT_EQUAL(0, net_tcp_connection_retransmit_allowed(&connection));
    TEST_ASSERT_NOT_EQUAL(0, net_tcp_connection_note_retransmit(&connection));
}

int main(void) {
    unity_init(); RUN_TEST(test_build_and_parse_syn_ack); RUN_TEST(test_connection_builds_first_ack); RUN_TEST(test_build_and_parse_ack_payload); RUN_TEST(test_connection_advances_sequences_and_accepts_data); RUN_TEST(test_tls_record_is_composed_into_tcp); RUN_TEST(test_accept_tls_record_on_tcp_view); RUN_TEST(test_accept_tls_handshake_transactional); RUN_TEST(test_accept_tls_postflight_transactional); RUN_TEST(test_tcp_tls_aes_gcm_transport); RUN_TEST(test_tcp_tls_x25519_flight_postflight_and_application); RUN_TEST(test_receive_window_is_bounded); RUN_TEST(test_build_data_tracks_until_commit); RUN_TEST(test_ack_confirms_pending_payload); RUN_TEST(test_fin_close_transitions); RUN_TEST(test_bounded_retransmission_metadata); unity_print_results(); unity_cleanup();
    return (unity_stats.tests_failed == 0) ? 0 : 1;
}
