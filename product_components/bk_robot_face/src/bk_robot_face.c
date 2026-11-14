/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_robot_face.h"

#include <string.h>

static bk_robot_emotion_t g_emotion = BK_ROBOT_EMOTION_NEUTRAL;

bk_err_t bk_robot_face_set_emotion(bk_ui_core_handle_t ui, bk_robot_emotion_t emotion)
{
    if (ui == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_err_t err = bk_ui_lock(ui, 100U);
    if (err != BK_OK) {
        return err;
    }
    g_emotion = emotion;
    return bk_ui_unlock(ui);
}

bk_robot_emotion_t bk_robot_face_get_emotion(void)
{
    return g_emotion;
}

bk_err_t bk_robot_face_set_emotion_from_str(bk_ui_core_handle_t ui, const char *emotion_str)
{
    if (ui == NULL || emotion_str == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_robot_emotion_t mapped = BK_ROBOT_EMOTION_NEUTRAL;
    if (strcmp(emotion_str, "happy") == 0 || strcmp(emotion_str, "laughing") == 0 ||
        strcmp(emotion_str, "funny") == 0 || strcmp(emotion_str, "loving") == 0 ||
        strcmp(emotion_str, "confident") == 0 || strcmp(emotion_str, "cool") == 0) {
        mapped = BK_ROBOT_EMOTION_HAPPY;
    } else if (strcmp(emotion_str, "thinking") == 0 || strcmp(emotion_str, "confused") == 0) {
        mapped = BK_ROBOT_EMOTION_THINKING;
    } else if (strcmp(emotion_str, "listening") == 0) {
        mapped = BK_ROBOT_EMOTION_LISTENING;
    } else if (strcmp(emotion_str, "speaking") == 0) {
        mapped = BK_ROBOT_EMOTION_SPEAKING;
    } else if (strcmp(emotion_str, "sad") == 0 || strcmp(emotion_str, "crying") == 0) {
        mapped = BK_ROBOT_EMOTION_SAD;
    } else if (strcmp(emotion_str, "surprised") == 0 || strcmp(emotion_str, "shocked") == 0) {
        mapped = BK_ROBOT_EMOTION_SURPRISED;
    } else if (strcmp(emotion_str, "sleepy") == 0 || strcmp(emotion_str, "relaxed") == 0) {
        mapped = BK_ROBOT_EMOTION_SLEEPY;
    } else if (strcmp(emotion_str, "low_battery") == 0) {
        mapped = BK_ROBOT_EMOTION_LOW_BATTERY;
    }
    return bk_robot_face_set_emotion(ui, mapped);
}

bk_err_t bk_robot_face_tick_ms(bk_ui_core_handle_t ui,
                               uint64_t now_ms,
                               bool tts_speaking,
                               bk_robot_face_frame_t *out_frame)
{
    if (ui == NULL || out_frame == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_err_t err = bk_ui_lock(ui, 100U);
    if (err != BK_OK) {
        return err;
    }

    memset(out_frame, 0, sizeof(*out_frame));
    out_frame->emotion = g_emotion;
    out_frame->left_eye_open_pct = 100U;
    out_frame->right_eye_open_pct = 100U;

    switch (g_emotion) {
    case BK_ROBOT_EMOTION_HAPPY:
        out_frame->left_eye_open_pct = 75U;
        out_frame->right_eye_open_pct = 75U;
        out_frame->mouth_open_pct = 45U;
        out_frame->brow_tilt_deg = 10;
        break;
    case BK_ROBOT_EMOTION_THINKING:
        out_frame->gaze_x_pct = 35;
        out_frame->gaze_y_pct = 30;
        out_frame->brow_tilt_deg = 15;
        break;
    case BK_ROBOT_EMOTION_LISTENING:
        out_frame->left_eye_open_pct = 100U;
        out_frame->right_eye_open_pct = 100U;
        out_frame->brow_tilt_deg = 5;
        break;
    case BK_ROBOT_EMOTION_SAD:
        out_frame->left_eye_open_pct = 60U;
        out_frame->right_eye_open_pct = 60U;
        out_frame->gaze_y_pct = -25;
        out_frame->brow_tilt_deg = -20;
        break;
    case BK_ROBOT_EMOTION_SURPRISED:
        out_frame->left_eye_open_pct = 100U;
        out_frame->right_eye_open_pct = 100U;
        out_frame->mouth_open_pct = 80U;
        out_frame->brow_tilt_deg = 25;
        break;
    case BK_ROBOT_EMOTION_SLEEPY:
    case BK_ROBOT_EMOTION_LOW_BATTERY:
        out_frame->left_eye_open_pct = 25U;
        out_frame->right_eye_open_pct = 25U;
        out_frame->gaze_y_pct = -30;
        break;
    case BK_ROBOT_EMOTION_SPEAKING:
    case BK_ROBOT_EMOTION_NEUTRAL:
    default:
        break;
    }

    /* Natural periodic blink: 150ms blink every 4000ms */
    uint64_t blink_phase = now_ms % 4000ULL;
    if (blink_phase >= 3850ULL) {
        out_frame->left_eye_open_pct = 0U;
        out_frame->right_eye_open_pct = 0U;
    }

    /* Lip-sync mouth modulation during TTS playback */
    if (tts_speaking || g_emotion == BK_ROBOT_EMOTION_SPEAKING) {
        uint64_t syllable_phase = (now_ms / 90ULL) % 4ULL;
        static const uint8_t lip_table[4] = {20U, 65U, 90U, 40U};
        out_frame->mouth_open_pct = lip_table[syllable_phase];
    }

    return bk_ui_unlock(ui);
}
