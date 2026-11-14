/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_BATTERY_POLICY_H
#define BK_OPEN_FRAMEWORK_BK_BATTERY_POLICY_H

#include <stdbool.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BK_BATTERY_LEVEL_NORMAL = 0,
    BK_BATTERY_LEVEL_WARNING,
    BK_BATTERY_LEVEL_CRITICAL_CUTOFF,
} bk_battery_level_t;

bk_err_t bk_battery_evaluate_mv(uint32_t millivolts,
                                bk_battery_level_t *out_level,
                                bool *out_allow_motion);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_BATTERY_POLICY_H */
