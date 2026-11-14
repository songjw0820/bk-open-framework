/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_PORT_CACHE_H
#define BK_OPEN_FRAMEWORK_BK_PORT_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_caps.h"
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t clean_count;
    size_t invalidate_count;
    size_t last_clean_bytes;
    size_t last_invalidate_bytes;
} bk_port_cache_stats_t;

/*
 * Wraps bk_dcache_clean_for_producer(addr, size) on bk7259_v4
 * (ap/middleware/arch/cortex-m/cache.c).
 */
bk_err_t bk_port_dcache_clean(void *addr, size_t size);

/*
 * Wraps bk_dcache_invalidate_for_consumer(addr, size) on bk7259_v4
 * (ap/middleware/arch/cortex-m/cache.c).
 */
bk_err_t bk_port_dcache_invalidate(void *addr, size_t size);

/*
 * Wraps bk_dcache_addr_is_cacheable(addr) on bk7259_v4.
 */
bool bk_port_addr_is_cacheable(const void *addr);

void bk_port_get_cache_stats(bk_port_cache_stats_t *out_stats);
void bk_port_reset_cache_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_PORT_CACHE_H */
