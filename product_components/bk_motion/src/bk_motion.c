/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_motion.h"
#include "bk_board.h"

static int16_t clamp_i16(int32_t val, int16_t lo, int16_t hi)
{
    if (val < (int32_t)lo) {
        return lo;
    }
    if (val > (int32_t)hi) {
        return hi;
    }
    return (int16_t)val;
}

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

bk_err_t bk_motion_interpolate_smooth(const bk_motion_keyframe_t *kf_start,
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
        *out_yaw_deg = clamp_i16(kf_start->yaw_deg, BK_MOTION_YAW_MIN_DEG, BK_MOTION_YAW_MAX_DEG);
        *out_pitch_deg = clamp_i16(kf_start->pitch_deg, BK_MOTION_PITCH_MIN_DEG, BK_MOTION_PITCH_MAX_DEG);
        return BK_OK;
    }
    if (query_ms >= kf_end->timestamp_ms) {
        *out_yaw_deg = clamp_i16(kf_end->yaw_deg, BK_MOTION_YAW_MIN_DEG, BK_MOTION_YAW_MAX_DEG);
        *out_pitch_deg = clamp_i16(kf_end->pitch_deg, BK_MOTION_PITCH_MIN_DEG, BK_MOTION_PITCH_MAX_DEG);
        return BK_OK;
    }

    /* Fixed-point smoothstep S(u) = 3*u^2 - 2*u^3 where u in [0, 1000] */
    int64_t dt = (int64_t)(kf_end->timestamp_ms - kf_start->timestamp_ms);
    int64_t elapsed = (int64_t)(query_ms - kf_start->timestamp_ms);
    int64_t u = (elapsed * 1000LL) / dt;
    int64_t s_q30 = u * u * (3000LL - 2LL * u); /* in [0, 10^9] */

    int64_t dy = (int64_t)(kf_end->yaw_deg - kf_start->yaw_deg);
    int64_t dp = (int64_t)(kf_end->pitch_deg - kf_start->pitch_deg);

    int32_t yaw = (int32_t)kf_start->yaw_deg + (int32_t)((dy * s_q30) / 1000000000LL);
    int32_t pitch = (int32_t)kf_start->pitch_deg + (int32_t)((dp * s_q30) / 1000000000LL);

    *out_yaw_deg = clamp_i16(yaw, BK_MOTION_YAW_MIN_DEG, BK_MOTION_YAW_MAX_DEG);
    *out_pitch_deg = clamp_i16(pitch, BK_MOTION_PITCH_MIN_DEG, BK_MOTION_PITCH_MAX_DEG);
    return BK_OK;
}

bk_err_t bk_motion_eval_sequence(const bk_motion_keyframe_t *keyframes,
                                 size_t keyframe_count,
                                 uint32_t query_ms,
                                 int16_t *out_yaw_deg,
                                 int16_t *out_pitch_deg,
                                 bool *out_finished)
{
    if (keyframes == NULL || keyframe_count < 2U || out_yaw_deg == NULL || out_pitch_deg == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (out_finished != NULL) {
        *out_finished = (query_ms >= keyframes[keyframe_count - 1U].timestamp_ms);
    }
    if (query_ms <= keyframes[0].timestamp_ms) {
        return bk_motion_interpolate_smooth(
            &keyframes[0], &keyframes[1], keyframes[0].timestamp_ms, out_yaw_deg, out_pitch_deg);
    }
    for (size_t i = 0U; i + 1U < keyframe_count; ++i) {
        if (query_ms <= keyframes[i + 1U].timestamp_ms) {
            return bk_motion_interpolate_smooth(
                &keyframes[i], &keyframes[i + 1U], query_ms, out_yaw_deg, out_pitch_deg);
        }
    }
    const bk_motion_keyframe_t *last = &keyframes[keyframe_count - 1U];
    return bk_motion_interpolate_smooth(
        &keyframes[keyframe_count - 2U], last, last->timestamp_ms, out_yaw_deg, out_pitch_deg);
}

bk_err_t bk_motion_eval_preset(bk_motion_preset_t preset,
                               uint32_t elapsed_ms,
                               int16_t *out_yaw_deg,
                               int16_t *out_pitch_deg,
                               bool *out_finished)
{
    static const bk_motion_keyframe_t seq_reset[] = {
        {0U, 0, 0},
        {200U, 0, 0},
    };
    static const bk_motion_keyframe_t seq_nod[] = {
        {0U, 0, 0},
        {180U, 0, 20},
        {360U, 0, -10},
        {540U, 0, 15},
        {720U, 0, 0},
    };
    static const bk_motion_keyframe_t seq_shake[] = {
        {0U, 0, 0},
        {200U, -30, 0},
        {400U, 30, 0},
        {600U, -20, 0},
        {800U, 0, 0},
    };
    static const bk_motion_keyframe_t seq_look[] = {
        {0U, 0, 0},
        {350U, -45, 12},
        {750U, 45, 12},
        {1100U, 0, 0},
    };

    switch (preset) {
    case BK_MOTION_PRESET_RESET_CENTER:
        return bk_motion_eval_sequence(
            seq_reset, sizeof(seq_reset) / sizeof(seq_reset[0]), elapsed_ms, out_yaw_deg, out_pitch_deg, out_finished);
    case BK_MOTION_PRESET_NOD_YES:
        return bk_motion_eval_sequence(
            seq_nod, sizeof(seq_nod) / sizeof(seq_nod[0]), elapsed_ms, out_yaw_deg, out_pitch_deg, out_finished);
    case BK_MOTION_PRESET_SHAKE_NO:
        return bk_motion_eval_sequence(
            seq_shake, sizeof(seq_shake) / sizeof(seq_shake[0]), elapsed_ms, out_yaw_deg, out_pitch_deg, out_finished);
    case BK_MOTION_PRESET_LOOK_AROUND:
        return bk_motion_eval_sequence(
            seq_look, sizeof(seq_look) / sizeof(seq_look[0]), elapsed_ms, out_yaw_deg, out_pitch_deg, out_finished);
    default:
        return BK_ERR_INVALID_ARG;
    }
}
