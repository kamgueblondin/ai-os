#include "../../framework/unity.h"
#include "../../../kernel/net_http_tls.h"

void setUp(void) {}
void tearDown(void) {}

static void open_pair(net_tcp_connection_t* client,net_tcp_connection_t* server,net_tls_aes_gcm_session_t* client_session,net_tls_aes_gcm_session_t* server_session,uint8_t key_material[40]){
    net_tcp_view_t syn_ack;net_tls_aes128_gcm_key_block_t block;uint8_t i;
    for(i=0U;i<40U;i++)key_material[i]=(uint8_t)(i+1U);
    block=(net_tls_aes128_gcm_key_block_t){key_material,key_material+16U,key_material+32U,key_material+36U};
    TEST_ASSERT_EQUAL(0,net_tls_aes_gcm_session_init(client_session,&block,1U));
    TEST_ASSERT_EQUAL(0,net_tls_aes_gcm_session_init(server_session,&block,0U));
    TEST_ASSERT_EQUAL(0,net_tcp_connection_open(client,49152U,443U,100U));
    syn_ack=(net_tcp_view_t){443U,49152U,700U,101U,NET_TCP_FLAG_SYN|NET_TCP_FLAG_ACK,0,0};
    TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_syn_ack(client,&syn_ack));
    TEST_ASSERT_EQUAL(0,net_tcp_connection_open(server,443U,49152U,700U));
    syn_ack=(net_tcp_view_t){49152U,443U,100U,701U,NET_TCP_FLAG_SYN|NET_TCP_FLAG_ACK,0,0};
    TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_syn_ack(server,&syn_ack));
}

void test_http_tls_get_and_response(void){
    net_tcp_connection_t client,server;net_tls_aes_gcm_session_t client_session,server_session;net_tcp_view_t view;net_tls_record_view_t record;net_http_response_view_t response;uint8_t key_material[40]={0},tcp_segment[192]={0},tls_record[160]={0},request[128]={0},plaintext[160]={0},response_record[160]={0},response_segment[192]={0},response_text[]={ 'H','T','T','P','/','1','.','1',' ','2','0','0',' ','O','K','\r','\n','C','o','n','n','e','c','t','i','o','n',':',' ','c','l','o','s','e','\r','\n','\r','\n','o','k'};uint16_t consumed=0U;int length;
    open_pair(&client,&server,&client_session,&server_session,key_material);
    TEST_ASSERT_EQUAL(70,net_http_build_get(request,sizeof(request),"api.example.test","/v1/models"));
    TEST_ASSERT_EQUAL('G',request[0]);TEST_ASSERT_EQUAL('/',request[4]);TEST_ASSERT_EQUAL('H',request[15]);
    length=net_http_tls_build_get(&client,&client_session,tcp_segment,sizeof(tcp_segment),tls_record,sizeof(tls_record),request,sizeof(request),"api.example.test","/v1/models",1U);
    TEST_ASSERT_EQUAL(119,length);TEST_ASSERT_EQUAL(0,net_tcp_parse(tcp_segment,(uint16_t)length,&view));
    TEST_ASSERT_EQUAL(0,net_tcp_connection_accept_tls_aes_gcm(&server,&server_session,&view,plaintext,sizeof(plaintext),&record,&consumed));
    TEST_ASSERT_EQUAL(70,record.payload_length);TEST_ASSERT_EQUAL('G',plaintext[0]);TEST_ASSERT_EQUAL('T',plaintext[2]);TEST_ASSERT_EQUAL('H',plaintext[15]);
    TEST_ASSERT_EQUAL(0,net_tcp_connection_commit_send(&client,view.payload_length));
    length=net_tcp_connection_build_tls_aes_gcm(&server,&server_session,response_segment,sizeof(response_segment),response_record,sizeof(response_record),NET_TLS_CONTENT_APPLICATION_DATA,response_text,sizeof(response_text),1U);
    TEST_ASSERT_EQUAL(89,length);TEST_ASSERT_EQUAL(0,net_tcp_parse(response_segment,(uint16_t)length,&view));
    TEST_ASSERT_EQUAL(0,net_http_tls_open_response(&client,&client_session,&view,plaintext,sizeof(plaintext),&response,&consumed));
    TEST_ASSERT_EQUAL(200,response.status_code);TEST_ASSERT_EQUAL(2,response.body_length);TEST_ASSERT_EQUAL('o',response.body[0]);TEST_ASSERT_EQUAL('k',response.body[1]);
}

void test_http_response_parse_rejects_invalid_framing(void){
    net_http_response_view_t response;uint8_t invalid[]={ 'N','O','T',' ','H','T','T','P','\r','\n','\r','\n'};uint8_t incomplete[]={ 'H','T','T','P','/','1','.','1',' ','2','0','0',' ','O','K','\r','\n'};
    TEST_ASSERT_NOT_EQUAL(0,net_http_response_parse(invalid,sizeof(invalid),&response));
    TEST_ASSERT_NOT_EQUAL(0,net_http_response_parse(incomplete,sizeof(incomplete),&response));
}

int main(void){unity_init();RUN_TEST(test_http_tls_get_and_response);RUN_TEST(test_http_response_parse_rejects_invalid_framing);unity_print_results();unity_cleanup();return unity_stats.tests_failed==0?0:1;}
