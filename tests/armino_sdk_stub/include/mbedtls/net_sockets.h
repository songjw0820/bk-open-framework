/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ARMINO_STUB_MBEDTLS_NET_SOCKETS_H
#define ARMINO_STUB_MBEDTLS_NET_SOCKETS_H

#include <stddef.h>

typedef struct {
    int fd;
} mbedtls_net_context;

int mbedtls_net_send(void *ctx, const unsigned char *buf, size_t len);
int mbedtls_net_recv(void *ctx, unsigned char *buf, size_t len);

#endif /* ARMINO_STUB_MBEDTLS_NET_SOCKETS_H */
