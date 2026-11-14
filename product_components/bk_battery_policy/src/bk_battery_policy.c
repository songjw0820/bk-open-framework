/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_battery_policy.h"
#include "bk_board.h"

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
    if (millivolts < 3450U) {
        *out_level = BK_BATTERY_LEVEL_CRITICAL_CUTOFF;
        *out_allow_motion = false;
    } else if (millivolts < 3600U) {
        *out_level = BK_BATTERY_LEVEL_WARNING;
        *out_allow_motion = true;
    } else {
        *out_level = BK_BATTERY_LEVEL_NORMAL;
        *out_allow_motion = true;
    }
    return BK_OK;
}
