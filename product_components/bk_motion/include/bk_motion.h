/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_MOTION_H
#define BK_OPEN_FRAMEWORK_BK_MOTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BK_MOTION_YAW_MIN_DEG   (-90)
#define BK_MOTION_YAW_MAX_DEG   (90)
#define BK_MOTION_PITCH_MIN_DEG (-35)
#define BK_MOTION_PITCH_MAX_DEG (35)

typedef enum {
    BK_MOTION_PRESET_RESET_CENTER = 0,
    BK_MOTION_PRESET_NOD_YES,
    BK_MOTION_PRESET_SHAKE_NO,
    BK_MOTION_PRESET_LOOK_AROUND,
} bk_motion_preset_t;

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

/*
 * Cubic Hermite S-curve ease-in / ease-out interpolation with mechanical
 * safety clamping (yaw [-90, +90], pitch [-35, +35]) to eliminate servo jerk.
 */
bk_err_t bk_motion_interpolate_smooth(const bk_motion_keyframe_t *kf_start,
                                      const bk_motion_keyframe_t *kf_end,
                                      uint32_t query_ms,
                                      int16_t *out_yaw_deg,
                                      int16_t *out_pitch_deg);

/*
 * Evaluates a multi-keyframe trajectory sequence at `query_ms` using smooth
 * S-curve interpolation between consecutive keyframes.
 */
bk_err_t bk_motion_eval_sequence(const bk_motion_keyframe_t *keyframes,
                                 size_t keyframe_count,
                                 uint32_t query_ms,
                                 int16_t *out_yaw_deg,
                                 int16_t *out_pitch_deg,
                                 bool *out_finished);

bk_err_t bk_motion_eval_preset(bk_motion_preset_t preset,
                               uint32_t elapsed_ms,
                               int16_t *out_yaw_deg,
                               int16_t *out_pitch_deg,
                               bool *out_finished);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_MOTION_H */
