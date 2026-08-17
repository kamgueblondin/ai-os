#ifndef AIOS_RSA_VERIFY_H
#define AIOS_RSA_VERIFY_H
#include <stdint.h>
/* Vérifie une signature PKCS#1 v1.5 SHA-256 avec espaces de travail fournis par l'appelant. */
int rsa_pkcs1_v15_sha256_verify(const uint8_t* modulus, uint16_t modulus_length,
                                const uint8_t* exponent, uint16_t exponent_length,
                                const uint8_t digest[32], const uint8_t* signature,
                                uint16_t signature_length, uint8_t* workspace,
                                uint16_t workspace_length);
#endif
