/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_battery_policy.h"
#include "bk_board.h"

#include <string.h>

bk_err_t bk_battery_evaluate_mv(uint32_t millivolts,
                                bk_battery_level_t *out_level,
                                bool *out_allow_motion)
{
    if (out_level == NULL || out_allow_motion == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_err_t cap_err = bk_board_require_cap(BK_BOARD_CAP_BATTERY_ADC, false);
    if (cap_err != BK_OK) {
        return cap_err;
    }
    if (millivolts < BK_BATTERY_CUTOFF_MV) {
        *out_level = BK_BATTERY_LEVEL_CRITICAL_CUTOFF;
        *out_allow_motion = false;
    } else if (millivolts < BK_BATTERY_WARNING_MV) {
        *out_level = BK_BATTERY_LEVEL_WARNING;
        *out_allow_motion = true;
    } else {
        *out_level = BK_BATTERY_LEVEL_NORMAL;
        *out_allow_motion = true;
    }
    return BK_OK;
}

void bk_battery_monitor_init(bk_battery_monitor_t *mon)
{
    if (mon != NULL) {
        memset(mon, 0, sizeof(*mon));
        mon->current_level = BK_BATTERY_LEVEL_NORMAL;
        mon->initialized = true;
    }
}

uint8_t bk_battery_estimate_soc_percent(uint32_t filtered_mv)
{
    if (filtered_mv <= BK_BATTERY_CUTOFF_MV) {
        return 0U;
    }
    if (filtered_mv >= BK_BATTERY_FULL_MV) {
        return 100U;
    }
    /* Piecewise Li-ion discharge approximation:
     * 3450..3650mV -> 0..20%
     * 3650..3950mV -> 20..80%
     * 3950..4200mV -> 80..100%
     */
    if (filtered_mv < 3650U) {
        return (uint8_t)(((filtered_mv - 3450U) * 20U) / 200U);
    }
    if (filtered_mv < 3950U) {
        return (uint8_t)(20U + (((filtered_mv - 3650U) * 60U) / 300U));
    }
    return (uint8_t)(80U + (((filtered_mv - 3950U) * 20U) / 250U));
}

bk_err_t bk_battery_monitor_feed_mv(bk_battery_monitor_t *mon,
                                    uint32_t raw_mv,
                                    bool is_charging,
                                    bk_battery_level_t *out_level,
                                    uint8_t *out_soc_percent,
                                    bool *out_allow_motion)
{
    if (mon == NULL || out_level == NULL || out_allow_motion == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_err_t cap_err = bk_board_require_cap(BK_BOARD_CAP_BATTERY_ADC, false);
    if (cap_err != BK_OK) {
        return cap_err;
    }
    if (!mon->initialized) {
        bk_battery_monitor_init(mon);
    }

    mon->samples_mv[mon->write_idx] = raw_mv;
    mon->write_idx = (uint8_t)((mon->write_idx + 1U) % BK_BATTERY_FILTER_WINDOW_LEN);
    if (mon->sample_count < BK_BATTERY_FILTER_WINDOW_LEN) {
        mon->sample_count += 1U;
    }

    uint32_t sum = 0U;
    for (uint8_t i = 0U; i < mon->sample_count; ++i) {
        sum += mon->samples_mv[i];
    }
    mon->filtered_mv = sum / mon->sample_count;
    mon->is_charging = is_charging;

    uint32_t v = mon->filtered_mv;
    switch (mon->current_level) {
    case BK_BATTERY_LEVEL_NORMAL:
        if (v < BK_BATTERY_CUTOFF_MV) {
            mon->current_level = BK_BATTERY_LEVEL_CRITICAL_CUTOFF;
        } else if (v < BK_BATTERY_WARNING_MV) {
            mon->current_level = BK_BATTERY_LEVEL_WARNING;
        }
        break;
    case BK_BATTERY_LEVEL_WARNING:
        if (v < BK_BATTERY_CUTOFF_MV) {
            mon->current_level = BK_BATTERY_LEVEL_CRITICAL_CUTOFF;
        } else if (v >= (BK_BATTERY_WARNING_MV + BK_BATTERY_HYSTERESIS_MV)) {
            mon->current_level = BK_BATTERY_LEVEL_NORMAL;
        }
        break;
    case BK_BATTERY_LEVEL_CRITICAL_CUTOFF:
        if (v >= (BK_BATTERY_WARNING_MV + BK_BATTERY_HYSTERESIS_MV)) {
            mon->current_level = BK_BATTERY_LEVEL_NORMAL;
        } else if (v >= (BK_BATTERY_CUTOFF_MV + BK_BATTERY_HYSTERESIS_MV)) {
            mon->current_level = BK_BATTERY_LEVEL_WARNING;
        }
        break;
    default:
        mon->current_level = BK_BATTERY_LEVEL_NORMAL;
        break;
    }

    *out_level = mon->current_level;
    *out_allow_motion = (mon->current_level != BK_BATTERY_LEVEL_CRITICAL_CUTOFF);
    if (out_soc_percent != NULL) {
        *out_soc_percent = bk_battery_estimate_soc_percent(mon->filtered_mv);
    }
    return BK_OK;
}
