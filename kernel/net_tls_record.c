#include "net_tls_record.h"
static uint16_t get16(const uint8_t* p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static uint32_t get24(const uint8_t* p){return ((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];}
static void put16(uint8_t* p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
int net_tls_record_build(uint8_t* record,uint32_t capacity,uint8_t content_type,const uint8_t* payload,uint16_t payload_length){uint16_t i;if(!record||(!payload&&payload_length)||capacity<(uint32_t)NET_TLS_RECORD_HEADER+payload_length||content_type==0U)return -1;record[0]=content_type;record[1]=NET_TLS_VERSION_1_2_MAJOR;record[2]=NET_TLS_VERSION_1_2_MINOR;put16(record+3,payload_length);for(i=0;i<payload_length;i++)record[5+i]=payload[i];return (int)(5U+payload_length);}
int net_tls_record_parse(const uint8_t* record,uint32_t length,net_tls_record_view_t* out){uint16_t payload_length;if(!record||!out||length<NET_TLS_RECORD_HEADER||record[1]!=NET_TLS_VERSION_1_2_MAJOR||record[2]!=NET_TLS_VERSION_1_2_MINOR)return -1;payload_length=get16(record+3);if((uint32_t)payload_length+NET_TLS_RECORD_HEADER>length||record[0]==0U)return -2;out->content_type=record[0];out->major=record[1];out->minor=record[2];out->payload=record+5;out->payload_length=payload_length;return 0;}
int net_tls_record_parse_stream(const uint8_t* stream,uint32_t length,net_tls_record_view_t* out,uint16_t* consumed){
    uint16_t payload_length, total;
    if(!stream||!out||!consumed) return -1;
    *consumed=0U;
    if(length<NET_TLS_RECORD_HEADER) return -2;
    payload_length=get16(stream+3);
    total=(uint16_t)(NET_TLS_RECORD_HEADER+payload_length);
    if(length<total) return -3;
    if(net_tls_record_parse(stream,total,out)!=0) return -4;
    *consumed=total; return 0;
}

int net_tls_server_hello_parse(const uint8_t* handshake,uint16_t length,net_tls_server_hello_view_t* out){
    uint32_t body_length; uint8_t session_length; uint16_t pos;
    if(!handshake||!out||length<4U)return -1;
    body_length=((uint32_t)handshake[1]<<16)|((uint32_t)handshake[2]<<8)|handshake[3];
    if(handshake[0]!=2U||body_length!=length-4U||body_length<38U)return -2;
    if(handshake[4]!=NET_TLS_VERSION_1_2_MAJOR||handshake[5]!=NET_TLS_VERSION_1_2_MINOR)return -3;
    out->random=handshake+6; session_length=handshake[38];
    if((uint32_t)39U+session_length+3U>length)return -4;
    out->session_id_length=session_length; out->session_id=handshake+39; pos=(uint16_t)(39U+session_length);
    out->cipher_suite=get16(handshake+pos); out->compression_method=handshake[pos+2U];
    if(out->compression_method!=0U)return -5;
    pos=(uint16_t)(pos+3U); out->extensions=0; out->extensions_length=0U;
    if((uint32_t)pos==length)return 0;
    if((uint32_t)pos+2U>length)return -6;
    out->extensions_length=get16(handshake+pos); pos=(uint16_t)(pos+2U);
    if((uint32_t)pos+out->extensions_length!=length)return -7;
    out->extensions=handshake+pos; return 0;
}

int net_tls_certificate_parse(const uint8_t* handshake,uint16_t length,net_tls_certificate_view_t* out){
    uint32_t body_length,list_length,pos,certificate_length;
    if(!handshake||!out||length<7U)return -1;
    body_length=get24(handshake+1);
    if(handshake[0]!=11U||body_length!=(uint32_t)length-4U||body_length<6U)return -2;
    list_length=get24(handshake+4);
    if(list_length<3U||list_length!=(uint32_t)length-7U)return -3;
    pos=7U; certificate_length=0U; out->certificate=0; out->certificate_length=0U;
    while(pos<(uint32_t)length){
        if((uint32_t)length-pos<3U)return -4;
        certificate_length=get24(handshake+pos);
        pos+=3U;
        if(certificate_length==0U||(uint32_t)length-pos<certificate_length)return -5;
        if(!out->certificate){out->certificate=handshake+pos;out->certificate_length=certificate_length;}
        pos+=certificate_length;
    }
    if(pos!=(uint32_t)length)return -6;
    out->certificate_list_length=list_length; return 0;
}

int net_tls_server_hello_done_parse(const uint8_t* handshake,uint16_t length){
    if(!handshake||length!=4U)return -1;
    if(handshake[0]!=14U||handshake[1]!=0U||handshake[2]!=0U||handshake[3]!=0U)return -2;
    return 0;
}

int net_tls_handshake_init(net_tls_handshake_t* handshake){
    if(!handshake)return -1; handshake->state=NET_TLS_HANDSHAKE_IDLE; handshake->cipher_suite=0U; handshake->server_random=0; handshake->server_certificate=0; handshake->server_certificate_length=0U; return 0;
}
int net_tls_handshake_note_client_hello(net_tls_handshake_t* handshake){
    if(!handshake||handshake->state!=NET_TLS_HANDSHAKE_IDLE)return -1; handshake->state=NET_TLS_HANDSHAKE_CLIENT_HELLO_SENT; return 0;
}
int net_tls_handshake_accept_server_hello(net_tls_handshake_t* handshake,const uint8_t* message,uint16_t length){
    net_tls_server_hello_view_t view;
    if(!handshake||handshake->state!=NET_TLS_HANDSHAKE_CLIENT_HELLO_SENT)return -1;
    if(net_tls_server_hello_parse(message,length,&view)!=0)return -2;
    handshake->cipher_suite=view.cipher_suite; handshake->server_random=view.random; handshake->state=NET_TLS_HANDSHAKE_SERVER_HELLO_RECEIVED; return 0;
}
int net_tls_handshake_accept_certificate(net_tls_handshake_t* handshake,const uint8_t* message,uint16_t length){
    net_tls_certificate_view_t view;
    if(!handshake||handshake->state!=NET_TLS_HANDSHAKE_SERVER_HELLO_RECEIVED)return -1;
    if(net_tls_certificate_parse(message,length,&view)!=0)return -2;
    handshake->server_certificate=view.certificate; handshake->server_certificate_length=view.certificate_length; handshake->state=NET_TLS_HANDSHAKE_CERTIFICATE_RECEIVED; return 0;
}
int net_tls_handshake_accept_server_hello_done(net_tls_handshake_t* handshake,const uint8_t* message,uint16_t length){
    if(!handshake||handshake->state!=NET_TLS_HANDSHAKE_CERTIFICATE_RECEIVED)return -1;
    if(net_tls_server_hello_done_parse(message,length)!=0)return -2;
    handshake->state=NET_TLS_HANDSHAKE_SERVER_HELLO_DONE_RECEIVED; return 0;
}

int net_tls_record_accumulator_init(net_tls_record_accumulator_t* accumulator,uint8_t* buffer,uint16_t capacity){
    if(!accumulator||!buffer||capacity<NET_TLS_RECORD_HEADER)return -1;
    accumulator->buffer=buffer; accumulator->capacity=capacity; accumulator->length=0U; return 0;
}

int net_tls_record_accumulator_feed(net_tls_record_accumulator_t* accumulator,const uint8_t* fragment,uint16_t fragment_length,net_tls_record_view_t* out){
    uint16_t i, consumed=0U; int status;
    if(!accumulator||!accumulator->buffer||!fragment||!out)return -1;
    if((uint32_t)accumulator->length+fragment_length>accumulator->capacity)return -2;
    for(i=0;i<fragment_length;i++) accumulator->buffer[accumulator->length+i]=fragment[i];
    accumulator->length=(uint16_t)(accumulator->length+fragment_length);
    status=net_tls_record_parse_stream(accumulator->buffer,accumulator->length,out,&consumed);
    if(status==-2||status==-3)return 1;
    if(status!=0)return -3;
    if(consumed!=accumulator->length)return -4;
    return 0;
}

int net_tls_client_hello_build(uint8_t* record,uint32_t capacity,const uint8_t random[32]){
    uint8_t hello[45]; uint8_t i; int length;
    if(!record||!random||capacity<NET_TLS_RECORD_HEADER+49U)return -1;
    hello[0]=1U; hello[1]=0U; hello[2]=0U; hello[3]=44U; hello[4]=NET_TLS_VERSION_1_2_MAJOR; hello[5]=NET_TLS_VERSION_1_2_MINOR;
    for(i=0;i<32U;i++) hello[6U+i]=random[i];
    hello[38]=0U; hello[39]=0U; hello[40]=2U; hello[41]=0x00U; hello[42]=0x9cU; hello[43]=1U; hello[44]=0U;
    length=net_tls_record_build(record,capacity,NET_TLS_CONTENT_HANDSHAKE,hello,sizeof(hello));
    return length;
}
