/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_BATTERY_POLICY_H
#define BK_OPEN_FRAMEWORK_BK_BATTERY_POLICY_H

#include <stdbool.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BK_BATTERY_CUTOFF_MV         3450U
#define BK_BATTERY_WARNING_MV        3600U
#define BK_BATTERY_FULL_MV           4200U
#define BK_BATTERY_HYSTERESIS_MV     50U
#define BK_BATTERY_FILTER_WINDOW_LEN 4U

typedef enum {
    BK_BATTERY_LEVEL_NORMAL = 0,
    BK_BATTERY_LEVEL_WARNING,
    BK_BATTERY_LEVEL_CRITICAL_CUTOFF,
} bk_battery_level_t;

typedef struct {
    uint32_t samples_mv[BK_BATTERY_FILTER_WINDOW_LEN];
    uint8_t sample_count;
    uint8_t write_idx;
    uint32_t filtered_mv;
    bk_battery_level_t current_level;
    bool is_charging;
    bool initialized;
} bk_battery_monitor_t;

bk_err_t bk_battery_evaluate_mv(uint32_t millivolts,
                                bk_battery_level_t *out_level,
                                bool *out_allow_motion);

void bk_battery_monitor_init(bk_battery_monitor_t *mon);

/*
 * Feeds a raw battery ADC sample (in mV) into a 4-sample moving-average filter
 * with +50mV Schmitt-trigger recovery hysteresis. Prevents transient voltage
 * droop during servo/PA current peaks from falsely triggering cutoff or oscillation.
 */
bk_err_t bk_battery_monitor_feed_mv(bk_battery_monitor_t *mon,
                                    uint32_t raw_mv,
                                    bool is_charging,
                                    bk_battery_level_t *out_level,
                                    uint8_t *out_soc_percent,
                                    bool *out_allow_motion);

uint8_t bk_battery_estimate_soc_percent(uint32_t filtered_mv);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_BATTERY_POLICY_H */
