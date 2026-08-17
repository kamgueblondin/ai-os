#include "net_http_tls.h"

static int net_http_append_char(uint8_t* output,uint16_t capacity,uint16_t* length,uint8_t value){
    if(!output||!length||*length>=capacity)return -1;
    output[*length]=value;*length=(uint16_t)(*length+1U);return 0;
}

static int net_http_append_text(uint8_t* output,uint16_t capacity,uint16_t* length,const char* text){
    uint16_t index=0U;
    if(!text)return -1;
    while(text[index]){
        uint8_t value=(uint8_t)text[index++];
        if(value<0x21U||value>0x7eU)return -2;
        if(net_http_append_char(output,capacity,length,value)!=0)return -3;
    }
    return 0;
}

int net_http_build_get(uint8_t* request,uint16_t capacity,const char* host,const char* path){
    uint16_t length=0U;
    if(!request||!host||!path||path[0]!='/')return -1;
    if(net_http_append_text(request,capacity,&length,"GET")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,path)!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,"HTTP/1.1")!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_text(request,capacity,&length,"Host:")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,host)!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_text(request,capacity,&length,"Connection:")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,"close")!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0)return -2;
    return (int)length;
}

int net_http_tls_build_get(net_tcp_connection_t* connection,net_tls_aes_gcm_session_t* session,uint8_t* tcp_segment,uint32_t tcp_capacity,uint8_t* tls_record,uint32_t tls_capacity,uint8_t* request,uint16_t request_capacity,const char* host,const char* path,uint8_t retransmit_limit){
    int request_length;
    request_length=net_http_build_get(request,request_capacity,host,path);
    if(request_length<0)return -1;
    return net_tcp_connection_build_tls_aes_gcm(connection,session,tcp_segment,tcp_capacity,tls_record,tls_capacity,NET_TLS_CONTENT_APPLICATION_DATA,request,(uint16_t)request_length,retransmit_limit);
}

int net_http_response_parse(const uint8_t* plaintext,uint16_t plaintext_length,net_http_response_view_t* out){
    uint16_t index,header_length;
    if(!plaintext||!out||plaintext_length<15U)return -1;
    if(plaintext[0]!='H'||plaintext[1]!='T'||plaintext[2]!='T'||plaintext[3]!='P'||plaintext[4]!='/'||plaintext[5]!='1'||plaintext[6]!='.'||plaintext[7]!='1'||plaintext[8]!=' '||plaintext[9]<'0'||plaintext[9]>'9'||plaintext[10]<'0'||plaintext[10]>'9'||plaintext[11]<'0'||plaintext[11]>'9'||plaintext[12]!=' ')return -2;
    header_length=0U;
    for(index=13U;(uint16_t)(index+3U)<plaintext_length;index++){
        if(plaintext[index]=='\r'&&plaintext[index+1U]=='\n'&&plaintext[index+2U]=='\r'&&plaintext[index+3U]=='\n'){header_length=(uint16_t)(index+4U);break;}
    }
    if(header_length==0U)return -3;
    out->status_code=(uint16_t)((uint16_t)(plaintext[9]-'0')*100U+(uint16_t)(plaintext[10]-'0')*10U+(uint16_t)(plaintext[11]-'0'));
    out->header_length=header_length;out->body=plaintext+header_length;out->body_length=(uint16_t)(plaintext_length-header_length);return 0;
}

int net_http_tls_open_response(net_tcp_connection_t* connection,net_tls_aes_gcm_session_t* session,const net_tcp_view_t* view,uint8_t* plaintext,uint16_t plaintext_capacity,net_http_response_view_t* response,uint16_t* consumed){
    net_tcp_connection_t previous_connection;net_tls_aes_gcm_session_t previous_session;net_tls_record_view_t record;int status;
    if(!connection||!session||!view||!plaintext||!response||!consumed)return -1;
    previous_connection=*connection;previous_session=*session;
    status=net_tcp_connection_accept_tls_aes_gcm(connection,session,view,plaintext,plaintext_capacity,&record,consumed);
    if(status!=0||record.content_type!=NET_TLS_CONTENT_APPLICATION_DATA||net_http_response_parse(record.payload,record.payload_length,response)!=0){*connection=previous_connection;*session=previous_session;*consumed=0U;return -2;}
    return 0;
}
