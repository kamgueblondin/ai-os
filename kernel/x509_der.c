#include "x509_der.h"
int der_tlv_parse(const uint8_t* input,uint32_t length,der_tlv_view_t* out){
    uint32_t value_length=0U,offset=2U,i;uint8_t count;
    if(!input||!out||length<2U)return -1;
    if((input[1]&128U)==0U)value_length=input[1];
    else{count=(uint8_t)(input[1]&127U);if(count==0U||count>4U||length<(uint32_t)2U+count)return -2;for(i=0U;i<count;i++){if(value_length>0x00ffffffU)return -3;value_length=(value_length<<8U)|input[2U+i];}offset+=(uint32_t)count;if(value_length<128U)return -4;}
    if(value_length>length-offset)return -5;
    out->tag=input[0];out->value=input+offset;out->length=value_length;out->total_length=offset+value_length;return 0;
}
static int next_tlv(const uint8_t** cursor,uint32_t* remaining,der_tlv_view_t* out){if(!cursor||!*cursor||!remaining||der_tlv_parse(*cursor,*remaining,out)!=0)return -1;*cursor+=out->total_length;*remaining-=out->total_length;return 0;}
static int require_sequence(const uint8_t** cursor,uint32_t* remaining,der_tlv_view_t* out){if(next_tlv(cursor,remaining,out)!=0||out->tag!=0x30U)return -1;return 0;}
int x509_certificate_parse(const uint8_t* certificate,uint32_t length,x509_certificate_view_t* out){
    der_tlv_view_t outer,tbs,field,validity,spki,algorithm,bit_string,rsa,modulus,exponent;const uint8_t *p,*sp;uint32_t remaining,sp_remaining;
    if(!certificate||!out)return -1;if(der_tlv_parse(certificate,length,&outer)!=0||outer.tag!=0x30U||outer.total_length!=length)return -2;
    out->certificate=certificate;out->certificate_length=length;p=outer.value;remaining=outer.length;
    if(require_sequence(&p,&remaining,&tbs)!=0)return -3;out->tbs_certificate=tbs.value;out->tbs_certificate_length=tbs.length;
    {const uint8_t* q=tbs.value;uint32_t left=tbs.length;if(left&&q[0]==0xa0U){if(next_tlv(&q,&left,&field)!=0)return -4;}if(next_tlv(&q,&left,&field)!=0||field.tag!=0x02U)return -5;out->serial=field.value;out->serial_length=field.length;if(require_sequence(&q,&left,&field)!=0)return -6;if(require_sequence(&q,&left,&field)!=0)return -7;out->issuer=field.value;out->issuer_length=field.length;if(require_sequence(&q,&left,&validity)!=0)return -8;{const uint8_t* v=validity.value;uint32_t vl=validity.length;if(next_tlv(&v,&vl,&field)!=0||(field.tag!=0x17U&&field.tag!=0x18U))return -9;out->not_before=field.value;out->not_before_length=field.length;if(next_tlv(&v,&vl,&field)!=0||(field.tag!=0x17U&&field.tag!=0x18U)||vl!=0U)return -10;out->not_after=field.value;out->not_after_length=field.length;}if(require_sequence(&q,&left,&field)!=0)return -11;out->subject=field.value;out->subject_length=field.length;if(require_sequence(&q,&left,&spki)!=0)return -12;out->subject_public_key_info=spki.value;out->subject_public_key_info_length=spki.length;sp=spki.value;sp_remaining=spki.length;if(require_sequence(&sp,&sp_remaining,&algorithm)!=0)return -13;out->subject_public_key_algorithm=algorithm.value;out->subject_public_key_algorithm_length=algorithm.length;if(next_tlv(&sp,&sp_remaining,&bit_string)!=0||bit_string.tag!=0x03U||bit_string.length<1U||bit_string.value[0]!=0U||sp_remaining!=0U)return -14;if(der_tlv_parse(bit_string.value+1U,bit_string.length-1U,&rsa)!=0||rsa.tag!=0x30U||rsa.total_length!=bit_string.length-1U)return -15;sp=rsa.value;sp_remaining=rsa.length;if(next_tlv(&sp,&sp_remaining,&modulus)!=0||modulus.tag!=0x02U||modulus.length==0U)return -16;if(next_tlv(&sp,&sp_remaining,&exponent)!=0||exponent.tag!=0x02U||exponent.length==0U||sp_remaining!=0U)return -17;out->rsa_modulus=modulus.value;out->rsa_modulus_length=modulus.length;out->rsa_exponent=exponent.value;out->rsa_exponent_length=exponent.length;}
    if(require_sequence(&p,&remaining,&field)!=0)return -18;if(next_tlv(&p,&remaining,&field)!=0||field.tag!=0x03U||remaining!=0U)return -19;return 0;
}

int x509_rsa_public_key_validate(const x509_certificate_view_t* certificate){
    static const uint8_t rsa_encryption_oid[]={0x06U,0x09U,0x2aU,0x86U,0x48U,0x86U,0xf7U,0x0dU,0x01U,0x01U,0x01U};
    const uint8_t *modulus,*exponent;uint32_t modulus_length,exponent_length,value=0U;uint32_t i;
    if(!certificate||!certificate->subject_public_key_algorithm||certificate->subject_public_key_algorithm_length!=(uint32_t)(sizeof(rsa_encryption_oid)+2U)||!certificate->rsa_modulus||!certificate->rsa_exponent)return -1;
    for(i=0U;i<sizeof(rsa_encryption_oid);i++)if(certificate->subject_public_key_algorithm[i]!=rsa_encryption_oid[i])return -2;
    if(certificate->subject_public_key_algorithm[sizeof(rsa_encryption_oid)]!=0x05U||certificate->subject_public_key_algorithm[sizeof(rsa_encryption_oid)+1U]!=0x00U)return -2;
    modulus=certificate->rsa_modulus;modulus_length=certificate->rsa_modulus_length;
    if(modulus_length==0U)return -3;
    if(modulus[0]==0U){if(modulus_length==1U||(modulus[1]&0x80U)==0U)return -3;modulus++;modulus_length--;}
    else if((modulus[0]&0x80U)!=0U)return -3;
    exponent=certificate->rsa_exponent;exponent_length=certificate->rsa_exponent_length;
    while(exponent_length>1U&&*exponent==0U){exponent++;exponent_length--;}
    if(exponent_length==0U||exponent_length>4U||(*exponent&0x80U)!=0U)return -4;
    for(i=0U;i<exponent_length;i++)value=(value<<8U)|exponent[i];
    if(value<3U||(value&1U)==0U)return -5;
    return 0;
}
