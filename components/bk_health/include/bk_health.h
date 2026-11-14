/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_HEALTH_H
#define BK_OPEN_FRAMEWORK_BK_HEALTH_H

#include <stddef.h>
#include <stdint.h>
#include "bk_err.h"
#include "bk_port_cache.h"
#include "bk_port_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BK_HEALTH_STATUS_OK = 0,
    BK_HEALTH_STATUS_WARNING,
    BK_HEALTH_STATUS_CRITICAL,
} bk_health_status_t;

typedef struct {
    size_t warn_allocated_bytes;
    size_t critical_allocated_bytes;
    size_t max_active_allocations;
} bk_health_thresholds_t;

typedef struct {
    uint64_t uptime_ms;
    bk_port_mem_stats_t mem_stats;
    bk_port_cache_stats_t cache_stats;
    bk_health_status_t status;
} bk_health_snapshot_t;

bk_err_t bk_health_capture(bk_health_snapshot_t *out_snapshot);

bk_err_t bk_health_evaluate(const bk_health_thresholds_t *thresholds,
                            bk_health_snapshot_t *out_snapshot);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_HEALTH_H */
