/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_MOTION_H
#define BK_OPEN_FRAMEWORK_BK_MOTION_H

#include <stddef.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t timestamp_ms;
    int16_t yaw_deg;
    int16_t pitch_deg;
} bk_motion_keyframe_t;

bk_err_t bk_motion_interpolate(const bk_motion_keyframe_t *kf_start,
                               const bk_motion_keyframe_t *kf_end,
                               uint32_t query_ms,
                               int16_t *out_yaw_deg,
                               int16_t *out_pitch_deg);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_MOTION_H */
