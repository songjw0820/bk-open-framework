/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ARMINO_STUB_MBEDTLS_SSL_H
#define ARMINO_STUB_MBEDTLS_SSL_H

#include <stddef.h>
#include <stdint.h>
#include "mbedtls/x509_crt.h"

#define MBEDTLS_SSL_IS_CLIENT        0
#define MBEDTLS_SSL_TRANSPORT_STREAM 0
#define MBEDTLS_SSL_PRESET_DEFAULT   0
#define MBEDTLS_SSL_VERIFY_REQUIRED  2

#define MBEDTLS_ERR_SSL_WANT_READ    (-0x6900)
#define MBEDTLS_ERR_SSL_WANT_WRITE   (-0x6880)

typedef int mbedtls_ssl_send_t(void *ctx, const unsigned char *buf, size_t len);
typedef int mbedtls_ssl_recv_t(void *ctx, unsigned char *buf, size_t len);
typedef int mbedtls_ssl_recv_timeout_t(void *ctx, unsigned char *buf, size_t len, uint32_t timeout);

typedef struct {
    int authmode;
    mbedtls_x509_crt *ca_chain;
} mbedtls_ssl_config;

typedef struct {
    const mbedtls_ssl_config *conf;
    char hostname[128];
    void *p_bio;
    mbedtls_ssl_send_t *f_send;
    mbedtls_ssl_recv_t *f_recv;
    int handshake_done;
} mbedtls_ssl_context;

void mbedtls_ssl_init(mbedtls_ssl_context *ssl);
void mbedtls_ssl_free(mbedtls_ssl_context *ssl);
void mbedtls_ssl_config_init(mbedtls_ssl_config *conf);
void mbedtls_ssl_config_free(mbedtls_ssl_config *conf);

int mbedtls_ssl_config_defaults(mbedtls_ssl_config *conf,
                                int endpoint,
                                int transport,
                                int preset);
void mbedtls_ssl_conf_authmode(mbedtls_ssl_config *conf, int authmode);
void mbedtls_ssl_conf_ca_chain(mbedtls_ssl_config *conf,
                               mbedtls_x509_crt *ca_chain,
                               void *ca_crl);
void mbedtls_ssl_conf_rng(mbedtls_ssl_config *conf,
                          int (*f_rng)(void *, unsigned char *, size_t),
                          void *p_rng);

int mbedtls_ssl_setup(mbedtls_ssl_context *ssl, const mbedtls_ssl_config *conf);
int mbedtls_ssl_set_hostname(mbedtls_ssl_context *ssl, const char *hostname);
void mbedtls_ssl_set_bio(mbedtls_ssl_context *ssl,
                         void *p_bio,
                         mbedtls_ssl_send_t *f_send,
                         mbedtls_ssl_recv_t *f_recv,
                         mbedtls_ssl_recv_timeout_t *f_recv_timeout);

int mbedtls_ssl_handshake(mbedtls_ssl_context *ssl);
uint32_t mbedtls_ssl_get_verify_result(const mbedtls_ssl_context *ssl);
size_t mbedtls_ssl_get_bytes_avail(const mbedtls_ssl_context *ssl);
int mbedtls_ssl_read(mbedtls_ssl_context *ssl, unsigned char *buf, size_t len);
int mbedtls_ssl_write(mbedtls_ssl_context *ssl, const unsigned char *buf, size_t len);
int mbedtls_ssl_close_notify(mbedtls_ssl_context *ssl);

#endif /* ARMINO_STUB_MBEDTLS_SSL_H */
