/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ARMINO_STUB_OS_MEM_H
#define ARMINO_STUB_OS_MEM_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void *os_malloc(size_t size);
void *os_zalloc(size_t size);
void *os_sram_malloc(size_t size);
void *psram_malloc(size_t size);
void *psram_cache_malloc(size_t size);
void os_free(void *ptr);

#define os_memset memset
#define os_memcpy memcpy

#endif /* ARMINO_STUB_OS_MEM_H */
