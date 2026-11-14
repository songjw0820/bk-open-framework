/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ARMINO_STUB_MBEDTLS_X509_CRT_H
#define ARMINO_STUB_MBEDTLS_X509_CRT_H

#include <stddef.h>

typedef struct {
    int parsed_ok;
} mbedtls_x509_crt;

void mbedtls_x509_crt_init(mbedtls_x509_crt *crt);
void mbedtls_x509_crt_free(mbedtls_x509_crt *crt);
int mbedtls_x509_crt_parse(mbedtls_x509_crt *chain, const unsigned char *buf, size_t buflen);

#endif /* ARMINO_STUB_MBEDTLS_X509_CRT_H */
