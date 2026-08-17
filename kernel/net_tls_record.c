#include "net_tls_record.h"
static uint16_t get16(const uint8_t* p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static void put16(uint8_t* p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
int net_tls_record_build(uint8_t* record,uint32_t capacity,uint8_t content_type,const uint8_t* payload,uint16_t payload_length){uint16_t i;if(!record||(!payload&&payload_length)||capacity<(uint32_t)NET_TLS_RECORD_HEADER+payload_length||content_type==0U)return -1;record[0]=content_type;record[1]=NET_TLS_VERSION_1_2_MAJOR;record[2]=NET_TLS_VERSION_1_2_MINOR;put16(record+3,payload_length);for(i=0;i<payload_length;i++)record[5+i]=payload[i];return (int)(5U+payload_length);}
int net_tls_record_parse(const uint8_t* record,uint32_t length,net_tls_record_view_t* out){uint16_t payload_length;if(!record||!out||length<NET_TLS_RECORD_HEADER||record[1]!=NET_TLS_VERSION_1_2_MAJOR||record[2]!=NET_TLS_VERSION_1_2_MINOR)return -1;payload_length=get16(record+3);if((uint32_t)payload_length+NET_TLS_RECORD_HEADER>length||record[0]==0U)return -2;out->content_type=record[0];out->major=record[1];out->minor=record[2];out->payload=record+5;out->payload_length=payload_length;return 0;}
int net_tls_client_hello_build(uint8_t* record,uint32_t capacity,const uint8_t random[32]){
    uint8_t hello[45]; uint8_t i; int length;
    if(!record||!random||capacity<NET_TLS_RECORD_HEADER+49U)return -1;
    hello[0]=1U; hello[1]=0U; hello[2]=0U; hello[3]=44U; hello[4]=NET_TLS_VERSION_1_2_MAJOR; hello[5]=NET_TLS_VERSION_1_2_MINOR;
    for(i=0;i<32U;i++) hello[6U+i]=random[i];
    hello[38]=0U; hello[39]=0U; hello[40]=2U; hello[41]=0x00U; hello[42]=0x9cU; hello[43]=1U; hello[44]=0U;
    length=net_tls_record_build(record,capacity,NET_TLS_CONTENT_HANDSHAKE,hello,sizeof(hello));
    return length;
}
