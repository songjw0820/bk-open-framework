/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_PORT_MEM_H
#define BK_OPEN_FRAMEWORK_BK_PORT_MEM_H

#include <stddef.h>
#include <stdint.h>
#include "bk_caps.h"
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BK_MEM_CAP_DEFAULT          (1U << 0)
#define BK_MEM_CAP_SRAM             (1U << 1)
#define BK_MEM_CAP_PSRAM            (1U << 2)
#define BK_MEM_CAP_PSRAM_CACHEABLE  (1U << 3)
#define BK_MEM_CAP_DMA_ALIGN64      (1U << 4)

typedef struct {
    size_t current_allocated_bytes;
    size_t peak_allocated_bytes;
    size_t active_allocations;
    size_t total_allocations;
} bk_port_mem_stats_t;

void *bk_port_malloc(size_t size, uint32_t caps, const char *tag);
void *bk_port_calloc(size_t nmemb, size_t size, uint32_t caps, const char *tag);
void bk_port_free(void *ptr);

void bk_port_get_mem_stats(bk_port_mem_stats_t *out_stats);
void bk_port_reset_mem_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_PORT_MEM_H */
