/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ARMINO_STUB_MBEDTLS_ENTROPY_H
#define ARMINO_STUB_MBEDTLS_ENTROPY_H

#include <stddef.h>

#define MBEDTLS_ENTROPY_MAX_GATHER    128
#define MBEDTLS_ENTROPY_SOURCE_STRONG 1

typedef int (*mbedtls_entropy_f_source_ptr)(void *data,
                                            unsigned char *output,
                                            size_t len,
                                            size_t *olen);

typedef struct {
    mbedtls_entropy_f_source_ptr source_fn;
    void *source_data;
} mbedtls_entropy_context;

void mbedtls_entropy_init(mbedtls_entropy_context *ctx);
void mbedtls_entropy_free(mbedtls_entropy_context *ctx);
int mbedtls_entropy_add_source(mbedtls_entropy_context *ctx,
                               mbedtls_entropy_f_source_ptr f_source,
                               void *p_source,
                               size_t threshold,
                               int strong);
int mbedtls_entropy_func(void *data, unsigned char *output, size_t len);

#endif /* ARMINO_STUB_MBEDTLS_ENTROPY_H */
