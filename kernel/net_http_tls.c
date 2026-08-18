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

int net_http_build_post_json_bearer(uint8_t* request,uint16_t capacity,const char* host,const char* path,const char* bearer_token,const uint8_t* json,uint16_t json_length){
    uint16_t length=0U;
    if(!request||!host||!path||path[0]!='/'||!bearer_token||!bearer_token[0]||(!json&&json_length))return -1;
    if(net_http_append_text(request,capacity,&length,"POST")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,path)!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,"HTTP/1.1")!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_text(request,capacity,&length,"Host:")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,host)!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_text(request,capacity,&length,"Authorization:")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,"Bearer")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,bearer_token)!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_text(request,capacity,&length,"Content-Type:")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,"application/json")!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_text(request,capacity,&length,"Content-Length:")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_uint16(request,capacity,&length,json_length)!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_text(request,capacity,&length,"Connection:")!=0||net_http_append_char(request,capacity,&length,' ')!=0||net_http_append_text(request,capacity,&length,"close")!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_char(request,capacity,&length,'\r')!=0||net_http_append_char(request,capacity,&length,'\n')!=0||net_http_append_bytes(request,capacity,&length,json,json_length)!=0)return -2;
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

int net_http_chunked_accumulator_init(net_http_chunked_accumulator_t* accumulator,uint8_t* buffer,uint16_t capacity){
    uint8_t index;
    if(!accumulator||!buffer||capacity==0U)return -1;
    accumulator->buffer=buffer;accumulator->capacity=capacity;accumulator->length=0U;accumulator->raw_length=0U;accumulator->header_length=0U;accumulator->status_code=0U;accumulator->chunk_remaining=0U;accumulator->line_length=0U;accumulator->state=0U;
    for(index=0U;index<sizeof(accumulator->line);index++)accumulator->line[index]=0U;
    return 0;
}

static int net_http_chunked_hex(uint8_t value,uint8_t* digit){
    if(value>='0'&&value<='9'){*digit=(uint8_t)(value-'0');return 0;}
    if(value>='a'&&value<='f'){*digit=(uint8_t)(value-'a'+10U);return 0;}
    if(value>='A'&&value<='F'){*digit=(uint8_t)(value-'A'+10U);return 0;}
    return -1;
}

static int net_http_chunked_has_transfer_encoding(const uint8_t* input,uint16_t header_length){
    uint16_t start=13U,index,cursor;
    while(start<(uint16_t)(header_length-2U)){
        index=start;while((uint16_t)(index+1U)<header_length&&!(input[index]=='\r'&&input[index+1U]=='\n'))index++;
        if(net_http_prefix(input,index,start,"Transfer-Encoding:")){
            cursor=(uint16_t)(start+18U);while(cursor<index&&input[cursor]==' ')cursor++;
            if((uint16_t)(index-cursor)==7U&&input[cursor]=='c'&&input[cursor+1U]=='h'&&input[cursor+2U]=='u'&&input[cursor+3U]=='n'&&input[cursor+4U]=='k'&&input[cursor+5U]=='e'&&input[cursor+6U]=='d')return 1;
        }
        start=(uint16_t)(index+2U);
    }
    return 0;
}

static int net_http_chunked_byte(net_http_chunked_accumulator_t* accumulator,uint8_t value){
    uint8_t digit;uint32_t next;
    if(accumulator->state==1U){
        if(value=='\r'){if(accumulator->line_length==0U)return -1;accumulator->chunk_remaining=0U;for(digit=0U;digit<accumulator->line_length;digit++){uint8_t hex;if(net_http_chunked_hex(accumulator->line[digit],&hex)!=0)return -2;next=(accumulator->chunk_remaining<<4U)|hex;if(next>65535U)return -3;accumulator->chunk_remaining=next;}accumulator->line_length=0U;accumulator->state=2U;return 1;}
        if(accumulator->line_length>=sizeof(accumulator->line)||net_http_chunked_hex(value,&digit)!=0)return -4;
        accumulator->line[accumulator->line_length++]=value;
        return 1;
    }
    if(accumulator->state==2U){if(value!='\n')return -5;accumulator->state=accumulator->chunk_remaining==0U?6U:3U;return 1;}
    if(accumulator->state==3U){if(accumulator->length>=accumulator->capacity)return -6;accumulator->buffer[accumulator->length++]=value;if(--accumulator->chunk_remaining==0U)accumulator->state=4U;return 1;}
    if(accumulator->state==4U){if(value!='\r')return -7;accumulator->state=5U;return 1;}
    if(accumulator->state==5U){if(value!='\n')return -8;accumulator->state=1U;return 1;}
    if(accumulator->state==6U){if(value!='\r')return -9;accumulator->state=7U;return 1;}
    if(accumulator->state==7U){if(value!='\n')return -10;accumulator->state=8U;return 0;}
    return -11;
}

int net_http_chunked_accumulator_feed(net_http_chunked_accumulator_t* accumulator,const uint8_t* fragment,uint16_t fragment_length,net_http_response_view_t* out){
    uint16_t index,header_length,content_length;uint8_t has_content_length;int status;
    if(!accumulator||!accumulator->buffer||(!fragment&&fragment_length)||!out)return -1;
    if(accumulator->state==8U)return -2;
    if(accumulator->state==0U){
        if((uint32_t)accumulator->raw_length+fragment_length>accumulator->capacity)return -3;
        for(index=0U;index<fragment_length;index++)accumulator->buffer[accumulator->raw_length+index]=fragment[index];
        accumulator->raw_length=(uint16_t)(accumulator->raw_length+fragment_length);
        status=net_http_parse_headers(accumulator->buffer,accumulator->raw_length,&accumulator->status_code,&header_length,&content_length,&has_content_length);
        if(status==1)return 1;
        if(status!=0||has_content_length||!net_http_chunked_has_transfer_encoding(accumulator->buffer,header_length))return -4;
        accumulator->header_length=header_length;accumulator->length=0U;accumulator->state=1U;
        for(index=header_length;index<accumulator->raw_length;index++){status=net_http_chunked_byte(accumulator,accumulator->buffer[index]);if(status<0)return -5;if(status==0)break;}
        accumulator->raw_length=0U;
    }else{
        for(index=0U;index<fragment_length;index++){status=net_http_chunked_byte(accumulator,fragment[index]);if(status<0)return -6;if(status==0)break;}
    }
    if(accumulator->state!=8U)return 1;
    out->status_code=accumulator->status_code;out->header_length=accumulator->header_length;out->body=accumulator->buffer;out->body_length=accumulator->length;return 0;
}

static int net_json_key_match(const uint8_t* input,uint16_t start,uint16_t end,const char* key){uint16_t index=0U;if(!key)return 0;while(key[index]){if((uint16_t)(start+index)>=end||input[start+index]!=(uint8_t)key[index])return 0;index++;}return (uint16_t)(start+index)==end;}
static uint16_t net_json_skip_space(const uint8_t* input,uint16_t length,uint16_t index){while(index<length&&(input[index]==' '||input[index]=='\t'||input[index]=='\r'||input[index]=='\n'))index++;return index;}
int net_json_extract_string(const uint8_t* json,uint16_t json_length,const char* key,uint8_t* output,uint16_t output_capacity,uint16_t* output_length){uint16_t index,start,end,cursor,out=0U;uint8_t escaped,value;
    if(!json||!key||!key[0]||!output||!output_length)return -1;
    for(index=0U;index<json_length;index++){
        if(json[index]!='"')continue;start=(uint16_t)(index+1U);escaped=0U;for(index=start;index<json_length;index++){if(escaped){escaped=0U;continue;}if(json[index]=='\\'){escaped=1U;continue;}if(json[index]=='"')break;}if(index>=json_length)return -2;end=index;
        if(!net_json_key_match(json,start,end,key))continue;cursor=net_json_skip_space(json,json_length,(uint16_t)(end+1U));if(cursor>=json_length||json[cursor]!=':')continue;cursor=net_json_skip_space(json,json_length,(uint16_t)(cursor+1U));if(cursor>=json_length||json[cursor]!='"')return -3;cursor++;
        for(;cursor<json_length;cursor++){value=json[cursor];if(value=='"'){*output_length=out;return 0;}if(value=='\\'){if(++cursor>=json_length)return -4;value=json[cursor];if(value=='"'||value=='\\'||value=='/'){}else if(value=='b')value='\b';else if(value=='f')value='\f';else if(value=='n')value='\n';else if(value=='r')value='\r';else if(value=='t')value='\t';else return -5;}else if(value<0x20U)return -6;if(out>=output_capacity)return -7;output[out++]=value;}
        return -8;
    }
    return -9;
}

int net_llm_ollama_response_extract(const uint8_t* json,uint16_t json_length,uint8_t* output,uint16_t output_capacity,uint16_t* output_length){
    return net_json_extract_string(json,json_length,"response",output,output_capacity,output_length);
}
int net_llm_openai_response_extract(const uint8_t* json,uint16_t json_length,uint8_t* output,uint16_t output_capacity,uint16_t* output_length){
    return net_json_extract_string(json,json_length,"content",output,output_capacity,output_length);
}

static int net_json_append_escaped(uint8_t* output,uint16_t capacity,uint16_t* length,const uint8_t* input,uint16_t input_length){uint16_t index;uint8_t value;if(!input&&input_length)return -1;for(index=0U;index<input_length;index++){value=input[index];if(value=='"'||value=='\\'){if(net_http_append_char(output,capacity,length,'\\')!=0||net_http_append_char(output,capacity,length,value)!=0)return -2;}else if(value=='\n'||value=='\r'||value=='\t'){uint8_t escaped=value=='\n'?'n':(value=='\r'?'r':'t');if(net_http_append_char(output,capacity,length,'\\')!=0||net_http_append_char(output,capacity,length,escaped)!=0)return -3;}else if(value<0x20U||value>=0x80U)return -4;else if(net_http_append_char(output,capacity,length,value)!=0)return -5;}return 0;}
static int net_llm_build_json(uint8_t* output,uint16_t capacity,const char* model,const uint8_t* prompt,uint16_t prompt_length,uint8_t openai){uint16_t length=0U,model_length=0U;if(!output||!model||!model[0]||(!prompt&&prompt_length))return -1;while(model[model_length]){if(model_length==65535U)return -2;model_length++;}if(net_http_append_text(output,capacity,&length,openai?"{\"model\":\"":"{\"model\":\"")!=0||net_json_append_escaped(output,capacity,&length,(const uint8_t*)model,model_length)!=0)return -3;if(openai){if(net_http_append_text(output,capacity,&length,"\",\"messages\":[{\"role\":\"user\",\"content\":\"")!=0)return -4;}else if(net_http_append_text(output,capacity,&length,"\",\"prompt\":\"")!=0)return -5;if(net_json_append_escaped(output,capacity,&length,prompt,prompt_length)!=0)return -6;if(net_http_append_text(output,capacity,&length,openai?"\"}],\"stream\":false}":"\",\"stream\":false}")!=0)return -7;return (int)length;}
int net_llm_build_ollama_generate_json(uint8_t* output,uint16_t output_capacity,const char* model,const uint8_t* prompt,uint16_t prompt_length){return net_llm_build_json(output,output_capacity,model,prompt,prompt_length,0U);}
int net_llm_build_openai_chat_json(uint8_t* output,uint16_t output_capacity,const char* model,const uint8_t* prompt,uint16_t prompt_length){return net_llm_build_json(output,output_capacity,model,prompt,prompt_length,1U);}
int net_llm_http_status_classify(uint16_t status_code){if(status_code>=200U&&status_code<300U)return NET_LLM_HTTP_STATUS_SUCCESS;if(status_code==401U||status_code==403U)return NET_LLM_HTTP_STATUS_AUTH;if(status_code==408U||status_code==425U||status_code==429U||(status_code>=500U&&status_code<600U))return NET_LLM_HTTP_STATUS_RETRYABLE;if(status_code>=300U&&status_code<500U)return NET_LLM_HTTP_STATUS_PERMANENT;return NET_LLM_HTTP_STATUS_PROTOCOL;}
int net_llm_http_retry_consume(uint16_t status_code,uint8_t retry_limit,uint8_t* retries_used){if(!retries_used)return -1;if(net_llm_http_status_classify(status_code)!=NET_LLM_HTTP_STATUS_RETRYABLE)return 0;if(*retries_used>=retry_limit)return 0;(*retries_used)++;return 1;}
