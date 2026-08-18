#ifndef AIOS_X509_DER_H
#define AIOS_X509_DER_H

#include <stdint.h>

typedef struct { uint8_t tag; const uint8_t* value; uint32_t length; uint32_t total_length; } der_tlv_view_t;
typedef struct {
    const uint8_t* certificate; uint32_t certificate_length;
    const uint8_t* tbs_certificate; uint32_t tbs_certificate_length;
    const uint8_t* tbs_certificate_der; uint32_t tbs_certificate_der_length;
    const uint8_t* serial; uint32_t serial_length;
    const uint8_t* issuer; uint32_t issuer_length;
    const uint8_t* subject; uint32_t subject_length;
    const uint8_t* not_before; uint32_t not_before_length;
    const uint8_t* not_after; uint32_t not_after_length;
    const uint8_t* subject_public_key_info; uint32_t subject_public_key_info_length;
    const uint8_t* subject_public_key_algorithm; uint32_t subject_public_key_algorithm_length;
    const uint8_t* rsa_modulus; uint32_t rsa_modulus_length;
    const uint8_t* rsa_exponent; uint32_t rsa_exponent_length;
    const uint8_t* common_name; uint32_t common_name_length;
    const uint8_t* subject_alt_names; uint32_t subject_alt_names_length;
    const uint8_t* signature_algorithm; uint32_t signature_algorithm_length;
    const uint8_t* signature; uint32_t signature_length;
} x509_certificate_view_t;

int der_tlv_parse(const uint8_t* input,uint32_t length,der_tlv_view_t* out);
int x509_certificate_parse(const uint8_t* certificate,uint32_t length,x509_certificate_view_t* out);
/* Compare une identité DNS ASCII au SAN dNSName, ou au CN seulement sans dNSName présent. */
int x509_certificate_hostname_validate(const x509_certificate_view_t* certificate,const char* hostname);
/* Vérifie notBefore <= instant UTC <= notAfter ; l’instant est fourni comme YYYYMMDDhhmmssZ. */
int x509_certificate_valid_at(const x509_certificate_view_t* certificate,const char* utc_time);
/* Vérifie une feuille RSA/SHA-256 signée directement par l’ancre X.509 fournie par l’appelant. */
int x509_certificate_chain_validate_one(const x509_certificate_view_t* leaf,const x509_certificate_view_t* trust_anchor,
                                        uint32_t* workspace,uint16_t workspace_length);
/* Exige chaîne RSA directe, hostname DNS et période UTC pour une identité TLS utilisable. */
int x509_certificate_tls_identity_validate(const x509_certificate_view_t* leaf,const x509_certificate_view_t* trust_anchor,
                                           const char* hostname,const char* utc_time,uint32_t* workspace,uint16_t workspace_length);
/* Vérifie l’OID rsaEncryption, le module positif et l’exposant public impair pris en charge par RSA. */
int x509_rsa_public_key_validate(const x509_certificate_view_t* certificate);

#endif
