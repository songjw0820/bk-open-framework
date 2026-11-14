/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_motion.h"
#include "bk_board.h"

bk_err_t bk_motion_interpolate(const bk_motion_keyframe_t *kf_start,
                               const bk_motion_keyframe_t *kf_end,
                               uint32_t query_ms,
                               int16_t *out_yaw_deg,
                               int16_t *out_pitch_deg)
{
    if (kf_start == NULL || kf_end == NULL || out_yaw_deg == NULL || out_pitch_deg == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_err_t cap_err = bk_board_require_cap(BK_BOARD_CAP_SERVO_PWM, false);
    if (cap_err != BK_OK) {
        return cap_err;
    }
    if (query_ms <= kf_start->timestamp_ms || kf_end->timestamp_ms <= kf_start->timestamp_ms) {
        *out_yaw_deg = kf_start->yaw_deg;
        *out_pitch_deg = kf_start->pitch_deg;
        return BK_OK;
    }
    if (query_ms >= kf_end->timestamp_ms) {
        *out_yaw_deg = kf_end->yaw_deg;
        *out_pitch_deg = kf_end->pitch_deg;
        return BK_OK;
    }
    int32_t dt = (int32_t)(kf_end->timestamp_ms - kf_start->timestamp_ms);
    int32_t elapsed = (int32_t)(query_ms - kf_start->timestamp_ms);
    *out_yaw_deg = (int16_t)(kf_start->yaw_deg +
                             ((int32_t)(kf_end->yaw_deg - kf_start->yaw_deg) * elapsed) / dt);
    *out_pitch_deg = (int16_t)(kf_start->pitch_deg +
                               ((int32_t)(kf_end->pitch_deg - kf_start->pitch_deg) * elapsed) / dt);
    return BK_OK;
}
