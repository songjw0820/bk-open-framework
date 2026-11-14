/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_health.h"
#include "bk_port_os.h"

bk_err_t bk_health_capture(bk_health_snapshot_t *out_snapshot)
{
    if (out_snapshot == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    out_snapshot->uptime_ms = bk_port_get_time_ms();
    bk_port_get_mem_stats(&out_snapshot->mem_stats);
    bk_port_get_cache_stats(&out_snapshot->cache_stats);
    out_snapshot->status = BK_HEALTH_STATUS_OK;
    return BK_OK;
}

bk_err_t bk_health_evaluate(const bk_health_thresholds_t *thresholds,
                            bk_health_snapshot_t *out_snapshot)
{
    if (thresholds == NULL || out_snapshot == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_err_t err = bk_health_capture(out_snapshot);
    if (err != BK_OK) {
        return err;
    }

    size_t cur_bytes = out_snapshot->mem_stats.current_allocated_bytes;
    size_t cur_allocs = out_snapshot->mem_stats.active_allocations;

    if ((thresholds->critical_allocated_bytes > 0U &&
         cur_bytes >= thresholds->critical_allocated_bytes) ||
        (thresholds->max_active_allocations > 0U &&
         cur_allocs > thresholds->max_active_allocations)) {
        out_snapshot->status = BK_HEALTH_STATUS_CRITICAL;
    } else if (thresholds->warn_allocated_bytes > 0U &&
               cur_bytes >= thresholds->warn_allocated_bytes) {
        out_snapshot->status = BK_HEALTH_STATUS_WARNING;
    } else {
        out_snapshot->status = BK_HEALTH_STATUS_OK;
    }
    return BK_OK;
}
