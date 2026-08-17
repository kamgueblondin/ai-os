#include "../../framework/unity.h"
#include "../../../kernel/net_tls_record.h"
void setUp(void){} void tearDown(void){}
void test_tls_record_build_parse(void){uint8_t record[32]={0},payload[3]={'C','H','1'};net_tls_record_view_t view;TEST_ASSERT_EQUAL(8,net_tls_record_build(record,sizeof(record),NET_TLS_CONTENT_HANDSHAKE,payload,3));TEST_ASSERT_EQUAL(0,net_tls_record_parse(record,8,&view));TEST_ASSERT_EQUAL(NET_TLS_CONTENT_HANDSHAKE,view.content_type);TEST_ASSERT_EQUAL(3,view.payload_length);TEST_ASSERT_EQUAL('C',view.payload[0]);record[2]=2;TEST_ASSERT_NOT_EQUAL(0,net_tls_record_parse(record,8,&view));}
int main(void){unity_init();RUN_TEST(test_tls_record_build_parse);unity_print_results();unity_cleanup();return unity_stats.tests_failed==0?0:1;}
