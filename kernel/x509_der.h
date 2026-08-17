#ifndef AIOS_X509_DER_H
#define AIOS_X509_DER_H
#include <stdint.h>
typedef struct { uint8_t tag; const uint8_t* value; uint32_t length; uint32_t total_length; } der_tlv_view_t;
typedef struct { const uint8_t* certificate; uint32_t certificate_length; const uint8_t* tbs_certificate; uint32_t tbs_certificate_length; const uint8_t* serial; uint32_t serial_length; const uint8_t* issuer; uint32_t issuer_length; const uint8_t* subject; uint32_t subject_length; const uint8_t* not_before; uint32_t not_before_length; const uint8_t* not_after; uint32_t not_after_length; const uint8_t* subject_public_key_info; uint32_t subject_public_key_info_length; const uint8_t* rsa_modulus; uint32_t rsa_modulus_length; const uint8_t* rsa_exponent; uint32_t rsa_exponent_length; } x509_certificate_view_t;
int der_tlv_parse(const uint8_t* input, uint32_t length, der_tlv_view_t* out);
int x509_certificate_parse(const uint8_t* certificate, uint32_t length, x509_certificate_view_t* out);
#endif
