#include "net_tls_record.h"
#include "sha256.h"
#include "aes_gcm.h"
static uint16_t get16(const uint8_t* p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static uint32_t get24(const uint8_t* p){return ((uint32_t)p[0]<<16)|((uint32_t)p[1]<<8)|p[2];}
static void put16(uint8_t* p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void put64(uint8_t* p,uint64_t v){uint8_t i;for(i=0U;i<8U;i++)p[i]=(uint8_t)(v>>(56U-8U*i));}
int net_tls_record_build(uint8_t* record,uint32_t capacity,uint8_t content_type,const uint8_t* payload,uint16_t payload_length){uint16_t i;if(!record||(!payload&&payload_length)||capacity<(uint32_t)NET_TLS_RECORD_HEADER+payload_length||content_type==0U)return -1;record[0]=content_type;record[1]=NET_TLS_VERSION_1_2_MAJOR;record[2]=NET_TLS_VERSION_1_2_MINOR;put16(record+3,payload_length);for(i=0;i<payload_length;i++)record[5+i]=payload[i];return (int)(5U+payload_length);}
int net_tls_record_parse(const uint8_t* record,uint32_t length,net_tls_record_view_t* out){uint16_t payload_length;if(!record||!out||length<NET_TLS_RECORD_HEADER||record[1]!=NET_TLS_VERSION_1_2_MAJOR||record[2]!=NET_TLS_VERSION_1_2_MINOR)return -1;payload_length=get16(record+3);if((uint32_t)payload_length+NET_TLS_RECORD_HEADER>length||record[0]==0U)return -2;out->content_type=record[0];out->major=record[1];out->minor=record[2];out->payload=record+5;out->payload_length=payload_length;return 0;}
int net_tls_aes_gcm_record_build(uint8_t* record,uint32_t capacity,uint8_t content_type,uint64_t sequence_number,const uint8_t key[16],const uint8_t fixed_iv[4],const uint8_t* plaintext,uint16_t plaintext_length){
    uint8_t nonce[8],additional[13],tag[16]; uint16_t payload_length;
    if(!record||!key||!fixed_iv||(!plaintext&&plaintext_length)||content_type==0U||plaintext_length>65511U)return -1;
    payload_length=(uint16_t)(plaintext_length+24U); if(capacity<(uint32_t)NET_TLS_RECORD_HEADER+payload_length)return -2;
    record[0]=content_type; record[1]=NET_TLS_VERSION_1_2_MAJOR; record[2]=NET_TLS_VERSION_1_2_MINOR; put16(record+3U,payload_length); put64(nonce,sequence_number); put64(record+5U,sequence_number);
    put64(additional,sequence_number); additional[8]=content_type; additional[9]=NET_TLS_VERSION_1_2_MAJOR; additional[10]=NET_TLS_VERSION_1_2_MINOR; put16(additional+11U,plaintext_length);
    if(aes128_gcm_encrypt(key,fixed_iv,nonce,additional,sizeof(additional),plaintext,plaintext_length,record+13U,tag)!=0)return -3;
    {uint8_t i;for(i=0U;i<16U;i++)record[13U+plaintext_length+i]=tag[i];}
    return (int)(NET_TLS_RECORD_HEADER+payload_length);
}
int net_tls_aes_gcm_record_open(const uint8_t* record,uint32_t length,uint64_t sequence_number,const uint8_t key[16],const uint8_t fixed_iv[4],uint8_t* plaintext,uint16_t plaintext_capacity,net_tls_record_view_t* out){
    net_tls_record_view_t encrypted; uint8_t additional[13]; uint16_t plaintext_length;
    if(!record||!key||!fixed_iv||!plaintext||!out)return -1;
    if(net_tls_record_parse(record,length,&encrypted)!=0)return -2;
    if(encrypted.payload_length<24U)return -3; plaintext_length=(uint16_t)(encrypted.payload_length-24U); if(plaintext_capacity<plaintext_length)return -4;
    put64(additional,sequence_number); additional[8]=encrypted.content_type; additional[9]=encrypted.major; additional[10]=encrypted.minor; put16(additional+11U,plaintext_length);
    if(aes128_gcm_decrypt(key,fixed_iv,encrypted.payload,additional,sizeof(additional),encrypted.payload+8U,plaintext_length,encrypted.payload+8U+plaintext_length,plaintext)!=0)return -5;
    out->content_type=encrypted.content_type; out->major=encrypted.major; out->minor=encrypted.minor; out->payload=plaintext; out->payload_length=plaintext_length; return 0;
}
int net_tls_aes_gcm_session_init(net_tls_aes_gcm_session_t* session,const net_tls_aes128_gcm_key_block_t* key_block,uint8_t is_client){
    if(!session||!key_block||!key_block->client_write_key||!key_block->server_write_key||!key_block->client_fixed_iv||!key_block->server_fixed_iv)return -1;
    if(is_client){session->write_key=key_block->client_write_key;session->write_fixed_iv=key_block->client_fixed_iv;session->read_key=key_block->server_write_key;session->read_fixed_iv=key_block->server_fixed_iv;}
    else{session->write_key=key_block->server_write_key;session->write_fixed_iv=key_block->server_fixed_iv;session->read_key=key_block->client_write_key;session->read_fixed_iv=key_block->client_fixed_iv;}
    session->write_sequence=0U;session->read_sequence=0U;return 0;
}
int net_tls_aes_gcm_session_build(net_tls_aes_gcm_session_t* session,uint8_t* record,uint32_t capacity,uint8_t content_type,const uint8_t* plaintext,uint16_t plaintext_length){
    int status;if(!session)return -1;status=net_tls_aes_gcm_record_build(record,capacity,content_type,session->write_sequence,session->write_key,session->write_fixed_iv,plaintext,plaintext_length);if(status<0)return status;session->write_sequence++;return status;
}
int net_tls_aes_gcm_session_open(net_tls_aes_gcm_session_t* session,const uint8_t* record,uint32_t length,uint8_t* plaintext,uint16_t plaintext_capacity,net_tls_record_view_t* out){
    int status;if(!session)return -1;status=net_tls_aes_gcm_record_open(record,length,session->read_sequence,session->read_key,session->read_fixed_iv,plaintext,plaintext_capacity,out);if(status!=0)return status;session->read_sequence++;return 0;
}

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

int net_tls_handshake_parse(const uint8_t* handshake,uint16_t length,net_tls_handshake_view_t* out){
    uint32_t body_length;
    if(!handshake||!out||length<NET_TLS_HANDSHAKE_HEADER)return -1;
    body_length=get24(handshake+1);
    if(body_length!=(uint32_t)length-NET_TLS_HANDSHAKE_HEADER)return -2;
    out->type=handshake[0]; out->body=handshake+NET_TLS_HANDSHAKE_HEADER; out->body_length=body_length; return 0;
}
int net_tls_handshake_accumulator_init(net_tls_handshake_accumulator_t* accumulator,uint8_t* buffer,uint16_t capacity){
    if(!accumulator||!buffer||capacity<NET_TLS_HANDSHAKE_HEADER)return -1;
    accumulator->buffer=buffer; accumulator->capacity=capacity; accumulator->length=0U; return 0;
}
int net_tls_handshake_accumulator_feed(net_tls_handshake_accumulator_t* accumulator,const uint8_t* fragment,uint16_t fragment_length,net_tls_handshake_view_t* out){
    uint16_t i; uint32_t total;
    if(!accumulator||!accumulator->buffer||(!fragment&&fragment_length)||!out)return -1;
    if((uint32_t)accumulator->length+fragment_length>accumulator->capacity)return -2;
    for(i=0U;i<fragment_length;i++)accumulator->buffer[accumulator->length+i]=fragment[i];
    accumulator->length=(uint16_t)(accumulator->length+fragment_length);
    if(accumulator->length<NET_TLS_HANDSHAKE_HEADER)return 1;
    total=(uint32_t)NET_TLS_HANDSHAKE_HEADER+get24(accumulator->buffer+1);
    if(total>accumulator->capacity)return -3;
    if(accumulator->length<total)return 1;
    if(accumulator->length!=total)return -4;
    return net_tls_handshake_parse(accumulator->buffer,accumulator->length,out);
}
int net_tls_transcript_init(net_tls_transcript_t* transcript,uint8_t* buffer,uint16_t capacity){
    if(!transcript||!buffer)return -1;
    transcript->buffer=buffer; transcript->capacity=capacity; transcript->length=0U; return 0;
}
int net_tls_transcript_append(net_tls_transcript_t* transcript,const uint8_t* handshake,uint16_t length){
    uint16_t i; net_tls_handshake_view_t view;
    if(!transcript||!transcript->buffer)return -1;
    if(net_tls_handshake_parse(handshake,length,&view)!=0)return -2;
    if((uint32_t)transcript->length+length>transcript->capacity)return -3;
    for(i=0U;i<length;i++)transcript->buffer[transcript->length+i]=handshake[i];
    transcript->length=(uint16_t)(transcript->length+length); return 0;
}

int net_tls_transcript_sha256(const net_tls_transcript_t* transcript,uint8_t digest[32]){
    sha256_ctx_t context;
    if(!transcript||!transcript->buffer||!digest)return -1;
    sha256_init(&context); sha256_update(&context,transcript->buffer,transcript->length); sha256_final(&context,digest); return 0;
}
int net_tls_prf_sha256(uint8_t* output,uint16_t output_length,const uint8_t* secret,uint16_t secret_length,const uint8_t* label,uint16_t label_length,const uint8_t* seed_a,uint16_t seed_a_length,const uint8_t* seed_b,uint16_t seed_b_length,uint8_t* workspace,uint32_t workspace_capacity){
    uint32_t seed_length=(uint32_t)label_length+seed_a_length+seed_b_length,required,offset=0U,j; uint16_t i; uint8_t digest[32]; uint8_t *seed,*a,*message;
    if(!output||output_length==0U||!secret||secret_length==0U||!label||label_length==0U||(!seed_a&&seed_a_length)||(!seed_b&&seed_b_length)||!workspace)return -1;
    required=2U*seed_length+64U; if(workspace_capacity<required)return -2;
    seed=workspace; a=workspace+seed_length; message=a+32U;
    for(i=0U;i<label_length;i++)seed[i]=label[i]; for(i=0U;i<seed_a_length;i++)seed[label_length+i]=seed_a[i]; for(i=0U;i<seed_b_length;i++)seed[label_length+seed_a_length+i]=seed_b[i];
    hmac_sha256(secret,secret_length,seed,seed_length,a);
    while(offset<output_length){
        for(i=0U;i<32U;i++)message[i]=a[i]; for(j=0U;j<seed_length;j++)message[32U+j]=seed[j];
        hmac_sha256(secret,secret_length,message,32U+seed_length,digest);
        for(i=0U;i<32U&&offset<output_length;i++)output[offset++]=digest[i];
        hmac_sha256(secret,secret_length,a,32U,a);
    }
    return 0;
}
int net_tls_derive_master_secret(uint8_t master_secret[48],const uint8_t* premaster_secret,uint16_t premaster_length,const uint8_t client_random[32],const uint8_t server_random[32],uint8_t* workspace,uint32_t workspace_capacity){
    static const uint8_t label[]={'m','a','s','t','e','r',' ','s','e','c','r','e','t'};
    if(!master_secret||!premaster_secret||premaster_length==0U||!client_random||!server_random)return -1;
    return net_tls_prf_sha256(master_secret,48U,premaster_secret,premaster_length,label,sizeof(label),client_random,32U,server_random,32U,workspace,workspace_capacity);
}
int net_tls_finished_verify_data(uint8_t verify_data[12],const uint8_t master_secret[48],const net_tls_transcript_t* transcript,uint8_t transcript_hash[32],uint8_t* workspace,uint32_t workspace_capacity){
    static const uint8_t label[]={'c','l','i','e','n','t',' ','f','i','n','i','s','h','e','d'};
    if(!verify_data||!master_secret||!transcript_hash)return -1;
    if(net_tls_transcript_sha256(transcript,transcript_hash)!=0)return -2;
    return net_tls_prf_sha256(verify_data,12U,master_secret,48U,label,sizeof(label),transcript_hash,32U,0,0U,workspace,workspace_capacity);
}
int net_tls_derive_aes128_gcm_key_block(uint8_t* key_block,uint32_t key_block_capacity,const uint8_t master_secret[48],const uint8_t client_random[32],const uint8_t server_random[32],net_tls_aes128_gcm_key_block_t* out,uint8_t* workspace,uint32_t workspace_capacity){
    static const uint8_t label[]={'k','e','y',' ','e','x','p','a','n','s','i','o','n'};
    int status;
    if(!key_block||key_block_capacity<NET_TLS_AES_128_GCM_KEY_BLOCK_LENGTH||!master_secret||!client_random||!server_random||!out)return -1;
    status=net_tls_prf_sha256(key_block,NET_TLS_AES_128_GCM_KEY_BLOCK_LENGTH,master_secret,48U,label,sizeof(label),server_random,32U,client_random,32U,workspace,workspace_capacity);
    if(status!=0)return -2;
    out->client_write_key=key_block; out->server_write_key=key_block+16U; out->client_fixed_iv=key_block+32U; out->server_fixed_iv=key_block+36U; return 0;
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

int net_tls_server_key_exchange_parse(const uint8_t* handshake,uint16_t length,net_tls_server_key_exchange_view_t* out){
    uint16_t pos,signature_length; uint8_t public_key_length;
    if(!handshake||!out||length<14U)return -1;
    if(handshake[0]!=NET_TLS_HANDSHAKE_SERVER_KEY_EXCHANGE||get24(handshake+1)!=(uint32_t)length-4U)return -2;
    if(handshake[4]!=3U)return -3;
    out->named_curve=get16(handshake+5); public_key_length=handshake[7]; pos=8U;
    if(public_key_length==0U||(uint32_t)pos+public_key_length+4U>length)return -4;
    out->public_key=handshake+pos; out->public_key_length=public_key_length; pos=(uint16_t)(pos+public_key_length);
    out->hash_algorithm=handshake[pos]; out->signature_algorithm=handshake[pos+1U]; signature_length=get16(handshake+pos+2U); pos=(uint16_t)(pos+4U);
    if(signature_length==0U||(uint32_t)pos+signature_length!=length)return -5;
    out->signature=handshake+pos; out->signature_length=signature_length; return 0;
}
int net_tls_certificate_request_parse(const uint8_t* handshake,uint16_t length,net_tls_certificate_request_view_t* out){
    uint16_t pos,signature_length,authorities_length; uint8_t types_length;
    if(!handshake||!out||length<9U)return -1;
    if(handshake[0]!=NET_TLS_HANDSHAKE_CERTIFICATE_REQUEST||get24(handshake+1)!=(uint32_t)length-4U)return -2;
    types_length=handshake[4]; pos=5U;
    if(types_length==0U||(uint32_t)pos+types_length+4U>length)return -3;
    out->certificate_types=handshake+pos; out->certificate_types_length=types_length; pos=(uint16_t)(pos+types_length);
    signature_length=get16(handshake+pos); pos=(uint16_t)(pos+2U);
    if(signature_length==0U||(signature_length&1U)!=0U||(uint32_t)pos+signature_length+2U>length)return -4;
    out->signature_algorithms=handshake+pos; out->signature_algorithms_length=signature_length; pos=(uint16_t)(pos+signature_length);
    authorities_length=get16(handshake+pos); pos=(uint16_t)(pos+2U);
    if((uint32_t)pos+authorities_length!=length)return -5;
    out->certificate_authorities=handshake+pos; out->certificate_authorities_length=authorities_length; return 0;
}

int net_tls_change_cipher_spec_parse(const uint8_t* payload,uint16_t length){
    if(!payload||length!=1U||payload[0]!=1U)return -1;
    return 0;
}
int net_tls_finished_parse(const uint8_t* handshake,uint16_t length,const uint8_t expected_verify_data[12]){
    uint8_t difference=0U,i;
    if(!handshake||!expected_verify_data||length!=16U)return -1;
    if(handshake[0]!=NET_TLS_HANDSHAKE_FINISHED||handshake[1]!=0U||handshake[2]!=0U||handshake[3]!=12U)return -2;
    for(i=0U;i<12U;i++)difference|=(uint8_t)(handshake[4U+i]^expected_verify_data[i]);
    return difference==0U?0:-3;
}
int net_tls_server_hello_done_parse(const uint8_t* handshake,uint16_t length){
    if(!handshake||length!=4U)return -1;
    if(handshake[0]!=14U||handshake[1]!=0U||handshake[2]!=0U||handshake[3]!=0U)return -2;
    return 0;
}

int net_tls_handshake_init(net_tls_handshake_t* handshake){
    if(!handshake)return -1; handshake->state=NET_TLS_HANDSHAKE_IDLE; handshake->cipher_suite=0U; handshake->server_random=0; handshake->server_certificate=0; handshake->server_certificate_length=0U; handshake->server_x509_valid=0U; handshake->server_named_curve=0U; handshake->server_public_key=0; handshake->server_public_key_length=0U; handshake->certificate_requested=0U; return 0;
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
    handshake->server_certificate=view.certificate; handshake->server_certificate_length=view.certificate_length; handshake->server_x509_valid=0U; handshake->state=NET_TLS_HANDSHAKE_CERTIFICATE_RECEIVED; return 0;
}
int net_tls_handshake_parse_server_certificate_x509(net_tls_handshake_t* handshake){
    if(!handshake||handshake->state<NET_TLS_HANDSHAKE_CERTIFICATE_RECEIVED||!handshake->server_certificate)return -1;
    if(x509_certificate_parse(handshake->server_certificate,handshake->server_certificate_length,&handshake->server_x509)!=0)return -2;
    handshake->server_x509_valid=1U;return 0;
}
int net_tls_handshake_accept_server_key_exchange(net_tls_handshake_t* handshake,const uint8_t* message,uint16_t length){
    net_tls_server_key_exchange_view_t view;
    if(!handshake||handshake->state!=NET_TLS_HANDSHAKE_CERTIFICATE_RECEIVED)return -1;
    if(net_tls_server_key_exchange_parse(message,length,&view)!=0)return -2;
    handshake->server_named_curve=view.named_curve; handshake->server_public_key=view.public_key; handshake->server_public_key_length=view.public_key_length; handshake->state=NET_TLS_HANDSHAKE_SERVER_KEY_EXCHANGE_RECEIVED; return 0;
}
int net_tls_handshake_accept_certificate_request(net_tls_handshake_t* handshake,const uint8_t* message,uint16_t length){
    net_tls_certificate_request_view_t view;
    if(!handshake||(handshake->state!=NET_TLS_HANDSHAKE_CERTIFICATE_RECEIVED&&handshake->state!=NET_TLS_HANDSHAKE_SERVER_KEY_EXCHANGE_RECEIVED))return -1;
    if(net_tls_certificate_request_parse(message,length,&view)!=0)return -2;
    handshake->certificate_requested=1U; handshake->state=NET_TLS_HANDSHAKE_CERTIFICATE_REQUEST_RECEIVED; return 0;
}
int net_tls_handshake_accept_server_hello_done(net_tls_handshake_t* handshake,const uint8_t* message,uint16_t length){
    if(!handshake||(handshake->state!=NET_TLS_HANDSHAKE_CERTIFICATE_RECEIVED&&handshake->state!=NET_TLS_HANDSHAKE_SERVER_KEY_EXCHANGE_RECEIVED&&handshake->state!=NET_TLS_HANDSHAKE_CERTIFICATE_REQUEST_RECEIVED))return -1;
    if(net_tls_server_hello_done_parse(message,length)!=0)return -2;
    handshake->state=NET_TLS_HANDSHAKE_SERVER_HELLO_DONE_RECEIVED; return 0;
}
int net_tls_handshake_accept_server_message(net_tls_handshake_t* handshake,const uint8_t* message,uint16_t length,net_tls_transcript_t* transcript){
    net_tls_handshake_t previous; int status;
    if(!handshake||!message)return -1;
    previous=*handshake;
    if(message[0]==NET_TLS_HANDSHAKE_SERVER_HELLO)status=net_tls_handshake_accept_server_hello(handshake,message,length);
    else if(message[0]==NET_TLS_HANDSHAKE_CERTIFICATE)status=net_tls_handshake_accept_certificate(handshake,message,length);
    else if(message[0]==NET_TLS_HANDSHAKE_SERVER_KEY_EXCHANGE)status=net_tls_handshake_accept_server_key_exchange(handshake,message,length);
    else if(message[0]==NET_TLS_HANDSHAKE_CERTIFICATE_REQUEST)status=net_tls_handshake_accept_certificate_request(handshake,message,length);
    else if(message[0]==NET_TLS_HANDSHAKE_SERVER_HELLO_DONE)status=net_tls_handshake_accept_server_hello_done(handshake,message,length);
    else return -2;
    if(status!=0)return -3;
    if(transcript&&net_tls_transcript_append(transcript,message,length)!=0){*handshake=previous;return -4;}
    return 0;
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

int net_tls_client_certificate_empty_build(uint8_t* handshake,uint32_t capacity){
    if(!handshake||capacity<7U)return -1;
    handshake[0]=NET_TLS_HANDSHAKE_CERTIFICATE; handshake[1]=0U; handshake[2]=0U; handshake[3]=3U; handshake[4]=0U; handshake[5]=0U; handshake[6]=0U; return 7;
}
int net_tls_client_key_exchange_build(uint8_t* handshake,uint32_t capacity,const uint8_t* public_key,uint8_t public_key_length){
    uint8_t i;
    if(!handshake||!public_key||public_key_length==0U||capacity<(uint32_t)5U+public_key_length)return -1;
    handshake[0]=NET_TLS_HANDSHAKE_CLIENT_KEY_EXCHANGE; handshake[1]=0U; handshake[2]=0U; handshake[3]=(uint8_t)(public_key_length+1U); handshake[4]=public_key_length;
    for(i=0U;i<public_key_length;i++)handshake[5U+i]=public_key[i]; return (int)(5U+public_key_length);
}
int net_tls_change_cipher_spec_build(uint8_t* record,uint32_t capacity){uint8_t payload=1U;return net_tls_record_build(record,capacity,NET_TLS_CONTENT_CHANGE_CIPHER_SPEC,&payload,1U);}
int net_tls_finished_build(uint8_t* record,uint32_t capacity,const uint8_t verify_data[12]){
    uint8_t finished[16],i;
    if(!record||!verify_data)return -1;
    finished[0]=NET_TLS_HANDSHAKE_FINISHED; finished[1]=0U; finished[2]=0U; finished[3]=12U;
    for(i=0U;i<12U;i++)finished[4U+i]=verify_data[i]; return net_tls_record_build(record,capacity,NET_TLS_CONTENT_HANDSHAKE,finished,sizeof(finished));
}
int net_tls_handshake_note_client_certificate(net_tls_handshake_t* handshake){
    if(!handshake||handshake->state!=NET_TLS_HANDSHAKE_SERVER_HELLO_DONE_RECEIVED||handshake->certificate_requested==0U)return -1;
    handshake->state=NET_TLS_HANDSHAKE_CLIENT_CERTIFICATE_SENT; return 0;
}
int net_tls_handshake_note_client_key_exchange(net_tls_handshake_t* handshake){
    if(!handshake||(handshake->state!=NET_TLS_HANDSHAKE_SERVER_HELLO_DONE_RECEIVED&&handshake->state!=NET_TLS_HANDSHAKE_CLIENT_CERTIFICATE_SENT))return -1;
    handshake->state=NET_TLS_HANDSHAKE_CLIENT_KEY_EXCHANGE_SENT; return 0;
}
int net_tls_handshake_note_change_cipher_spec(net_tls_handshake_t* handshake){
    if(!handshake||handshake->state!=NET_TLS_HANDSHAKE_CLIENT_KEY_EXCHANGE_SENT)return -1;
    handshake->state=NET_TLS_HANDSHAKE_CHANGE_CIPHER_SPEC_SENT; return 0;
}
int net_tls_handshake_note_finished(net_tls_handshake_t* handshake){
    if(!handshake||handshake->state!=NET_TLS_HANDSHAKE_CHANGE_CIPHER_SPEC_SENT)return -1;
    handshake->state=NET_TLS_HANDSHAKE_FINISHED_SENT; return 0;
}
int net_tls_handshake_accept_server_change_cipher_spec(net_tls_handshake_t* handshake,const uint8_t* payload,uint16_t length){
    if(!handshake||handshake->state!=NET_TLS_HANDSHAKE_FINISHED_SENT)return -1;
    if(net_tls_change_cipher_spec_parse(payload,length)!=0)return -2;
    handshake->state=NET_TLS_HANDSHAKE_SERVER_CHANGE_CIPHER_SPEC_RECEIVED; return 0;
}
int net_tls_handshake_accept_server_finished(net_tls_handshake_t* handshake,const uint8_t* message,uint16_t length,const uint8_t expected_verify_data[12]){
    if(!handshake||handshake->state!=NET_TLS_HANDSHAKE_SERVER_CHANGE_CIPHER_SPEC_RECEIVED)return -1;
    if(net_tls_finished_parse(message,length,expected_verify_data)!=0)return -2;
    handshake->state=NET_TLS_HANDSHAKE_SERVER_FINISHED_RECEIVED; return 0;
}
int net_tls_handshake_is_complete(const net_tls_handshake_t* handshake){return handshake&&handshake->state==NET_TLS_HANDSHAKE_SERVER_FINISHED_RECEIVED;}

int net_tls_client_hello_build(uint8_t* record,uint32_t capacity,const uint8_t random[32]){
    uint8_t hello[45]; uint8_t i; int length;
    if(!record||!random||capacity<NET_TLS_RECORD_HEADER+49U)return -1;
    hello[0]=1U; hello[1]=0U; hello[2]=0U; hello[3]=44U; hello[4]=NET_TLS_VERSION_1_2_MAJOR; hello[5]=NET_TLS_VERSION_1_2_MINOR;
    for(i=0;i<32U;i++) hello[6U+i]=random[i];
    hello[38]=0U; hello[39]=0U; hello[40]=2U; hello[41]=0x00U; hello[42]=0x9cU; hello[43]=1U; hello[44]=0U;
    length=net_tls_record_build(record,capacity,NET_TLS_CONTENT_HANDSHAKE,hello,sizeof(hello));
    return length;
}
