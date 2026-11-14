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
    return BK_OK;
}
