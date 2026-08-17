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

static int net_http_append_bytes(uint8_t* output,uint16_t capacity,uint16_t* length,const uint8_t* input,uint16_t input_length){
    uint16_t index;
    if(!output||!length||(!input&&input_length)||(uint32_t)*length+input_length>capacity)return -1;
    for(index=0U;index<input_length;index++)output[*length+index]=input[index];
    *length=(uint16_t)(*length+input_length);
    return 0;
}

static int net_http_append_uint16(uint8_t* output,uint16_t capacity,uint16_t* length,uint16_t value){
    uint16_t divisor=10000U;uint8_t emitted=0U;
    while(divisor>0U){
        uint8_t digit=(uint8_t)(value/divisor);
        if(digit||emitted||divisor==1U){if(net_http_append_char(output,capacity,length,(uint8_t)('0'+digit))!=0)return -1;emitted=1U;}
        value=(uint16_t)(value%divisor);divisor=(uint16_t)(divisor/10U);
    }
    return 0;
}

static int net_http_prefix(const uint8_t* input,uint16_t length,uint16_t offset,const char* text){
    uint16_t index=0U;
    while(text[index]){if((uint16_t)(offset+index)>=length||input[offset+index]!=(uint8_t)text[index])return 0;index++;}
    return 1;
}

static int net_http_parse_headers(const uint8_t* input,uint16_t length,uint16_t* status,uint16_t* header_length,uint16_t* content_length,uint8_t* has_content_length){
    uint16_t index,line_start,header_end=0U;uint32_t value;uint8_t seen=0U;
    if(!input||!status||!header_length||!content_length||!has_content_length||length<15U)return -1;
    if(input[0]!='H'||input[1]!='T'||input[2]!='T'||input[3]!='P'||input[4]!='/'||input[5]!='1'||input[6]!='.'||input[7]!='1'||input[8]!=' '||input[9]<'0'||input[9]>'9'||input[10]<'0'||input[10]>'9'||input[11]<'0'||input[11]>'9'||input[12]!=' ')return -2;
    for(index=13U;(uint16_t)(index+3U)<length;index++)if(input[index]=='\r'&&input[index+1U]=='\n'&&input[index+2U]=='\r'&&input[index+3U]=='\n'){header_end=(uint16_t)(index+4U);break;}
    if(header_end==0U)return 1;
    *status=(uint16_t)((uint16_t)(input[9]-'0')*100U+(uint16_t)(input[10]-'0')*10U+(uint16_t)(input[11]-'0'));
    *header_length=header_end;*content_length=0U;*has_content_length=0U;line_start=13U;
    while(line_start<(uint16_t)(header_end-2U)){
        index=line_start;while((uint16_t)(index+1U)<header_end&&!(input[index]=='\r'&&input[index+1U]=='\n'))index++;
        if((uint16_t)(index+1U)>=header_end)return -3;
        if(net_http_prefix(input,index,line_start,"Content-Length:")){
            uint16_t cursor=(uint16_t)(line_start+15U);if(seen)return -4;seen=1U;while(cursor<index&&input[cursor]==' ')cursor++;if(cursor==index)return -5;value=0U;
            while(cursor<index){if(input[cursor]<'0'||input[cursor]>'9')return -6;value=value*10U+(uint32_t)(input[cursor]-'0');if(value>65535U)return -7;cursor++;}
            *content_length=(uint16_t)value;*has_content_length=1U;
        }
        line_start=(uint16_t)(index+2U);
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
    int request_length=net_http_build_get(request,request_capacity,host,path);
    if(request_length<0)return -1;
    return net_tcp_connection_build_tls_aes_gcm(connection,session,tcp_segment,tcp_capacity,tls_record,tls_capacity,NET_TLS_CONTENT_APPLICATION_DATA,request,(uint16_t)request_length,retransmit_limit);
}

int net_http_build_post_json(uint8_t* request,uint16_t capacity,const char* host,const char* path,const uint8_t* json,uint16_t json_length){
    uint16_t length=0U;
    if(!request||!host||!path||path[0]!='/'||(!json&&json_length))return -1;
    if(net_http_append_text(request,capacity,&length,"POST")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,path)!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,"HTTP/1.1")!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_text(request,capacity,&length,"Host:")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,host)!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_text(request,capacity,&length,"Content-Type:")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,"application/json")!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_text(request,capacity,&length,"Content-Length:")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_uint16(request,capacity,&length,json_length)!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_text(request,capacity,&length,"Connection:")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,"close")!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_bytes(request,capacity,&length,json,json_length)!=0)return -2;
    return (int)length;
}

int net_http_tls_build_post_json(net_tcp_connection_t* connection,net_tls_aes_gcm_session_t* session,uint8_t* tcp_segment,uint32_t tcp_capacity,uint8_t* tls_record,uint32_t tls_capacity,uint8_t* request,uint16_t request_capacity,const char* host,const char* path,const uint8_t* json,uint16_t json_length,uint8_t retransmit_limit){
    int request_length=net_http_build_post_json(request,request_capacity,host,path,json,json_length);
    if(request_length<0)return -1;
    return net_tcp_connection_build_tls_aes_gcm(connection,session,tcp_segment,tcp_capacity,tls_record,tls_capacity,NET_TLS_CONTENT_APPLICATION_DATA,request,(uint16_t)request_length,retransmit_limit);
}

int net_http_response_parse(const uint8_t* plaintext,uint16_t plaintext_length,net_http_response_view_t* out){
    uint16_t content_length;uint8_t has_content_length;
    if(!out)return -1;
    if(net_http_parse_headers(plaintext,plaintext_length,&out->status_code,&out->header_length,&content_length,&has_content_length)!=0)return -2;
    out->body=plaintext+out->header_length;out->body_length=(uint16_t)(plaintext_length-out->header_length);return 0;
}

int net_http_response_accumulator_init(net_http_response_accumulator_t* accumulator,uint8_t* buffer,uint16_t capacity){
    if(!accumulator||!buffer||capacity==0U)return -1;
    accumulator->buffer=buffer;accumulator->capacity=capacity;accumulator->length=0U;accumulator->header_length=0U;accumulator->expected_body_length=0U;accumulator->status_code=0U;accumulator->headers_complete=0U;return 0;
}

int net_http_response_accumulator_feed(net_http_response_accumulator_t* accumulator,const uint8_t* fragment,uint16_t fragment_length,net_http_response_view_t* out){
    uint16_t index,content_length;uint8_t has_content_length;int status;
    if(!accumulator||!accumulator->buffer||(!fragment&&fragment_length)||!out)return -1;
    if((uint32_t)accumulator->length+fragment_length>accumulator->capacity)return -2;
    for(index=0U;index<fragment_length;index++){
        accumulator->buffer[accumulator->length+index]=fragment[index];
    }
    accumulator->length=(uint16_t)(accumulator->length+fragment_length);
    if(!accumulator->headers_complete){
        status=net_http_parse_headers(accumulator->buffer,accumulator->length,&accumulator->status_code,&accumulator->header_length,&content_length,&has_content_length);
        if(status==1)return 1;
        if(status!=0||!has_content_length)return -3;
        accumulator->expected_body_length=content_length;accumulator->headers_complete=1U;
    }
    if(accumulator->length<(uint16_t)(accumulator->header_length+accumulator->expected_body_length))return 1;
    if(accumulator->length!=(uint16_t)(accumulator->header_length+accumulator->expected_body_length))return -4;
    out->status_code=accumulator->status_code;out->header_length=accumulator->header_length;out->body=accumulator->buffer+accumulator->header_length;out->body_length=accumulator->expected_body_length;return 0;
}

int net_http_tls_open_response(net_tcp_connection_t* connection,net_tls_aes_gcm_session_t* session,const net_tcp_view_t* view,uint8_t* plaintext,uint16_t plaintext_capacity,net_http_response_view_t* response,uint16_t* consumed){
    net_tcp_connection_t previous_connection;net_tls_aes_gcm_session_t previous_session;net_tls_record_view_t record;int status;
    if(!connection||!session||!view||!plaintext||!response||!consumed)return -1;
    previous_connection=*connection;previous_session=*session;
    status=net_tcp_connection_accept_tls_aes_gcm(connection,session,view,plaintext,plaintext_capacity,&record,consumed);
    if(status!=0||record.content_type!=NET_TLS_CONTENT_APPLICATION_DATA||net_http_response_parse(record.payload,record.payload_length,response)!=0){*connection=previous_connection;*session=previous_session;*consumed=0U;return -2;}
    return 0;
}

int net_http_tls_open_response_stream(net_tcp_connection_t* connection,net_tls_aes_gcm_session_t* session,const net_tcp_view_t* view,uint8_t* plaintext,uint16_t plaintext_capacity,net_http_response_accumulator_t* accumulator,net_http_response_view_t* response,uint16_t* consumed){
    net_tcp_connection_t previous_connection;net_tls_aes_gcm_session_t previous_session;net_http_response_accumulator_t previous_accumulator;net_tls_record_view_t record;int status;
    if(!connection||!session||!view||!plaintext||!accumulator||!response||!consumed)return -1;
    previous_connection=*connection;previous_session=*session;previous_accumulator=*accumulator;
    status=net_tcp_connection_accept_tls_aes_gcm(connection,session,view,plaintext,plaintext_capacity,&record,consumed);
    if(status!=0||record.content_type!=NET_TLS_CONTENT_APPLICATION_DATA)goto rollback;
    status=net_http_response_accumulator_feed(accumulator,record.payload,record.payload_length,response);
    if(status>=0)return status;
rollback:
    *connection=previous_connection;*session=previous_session;*accumulator=previous_accumulator;*consumed=0U;return -2;
}
