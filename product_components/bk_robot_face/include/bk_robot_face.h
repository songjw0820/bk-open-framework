/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_ROBOT_FACE_H
#define BK_OPEN_FRAMEWORK_BK_ROBOT_FACE_H

#include <stdbool.h>
#include <stdint.h>
#include "bk_err.h"
#include "bk_ui_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BK_ROBOT_EMOTION_NEUTRAL = 0,
    BK_ROBOT_EMOTION_LISTENING,
    BK_ROBOT_EMOTION_THINKING,
    BK_ROBOT_EMOTION_SPEAKING,
    BK_ROBOT_EMOTION_HAPPY,
    BK_ROBOT_EMOTION_LOW_BATTERY,
    BK_ROBOT_EMOTION_SAD,
    BK_ROBOT_EMOTION_SURPRISED,
    BK_ROBOT_EMOTION_SLEEPY,
} bk_robot_emotion_t;

typedef struct {
    bk_robot_emotion_t emotion;
    uint8_t left_eye_open_pct;  /* 0 (closed blink) .. 100 (wide open) */
    uint8_t right_eye_open_pct;
    int8_t gaze_x_pct;          /* -100 (left) .. +100 (right) */
    int8_t gaze_y_pct;          /* -100 (down) .. +100 (up) */
    uint8_t mouth_open_pct;     /* 0 (closed) .. 100 (speaking open) */
    int8_t brow_tilt_deg;       /* -30 .. +30 */
} bk_robot_face_frame_t;

bk_err_t bk_robot_face_set_emotion(bk_ui_core_handle_t ui, bk_robot_emotion_t emotion);
bk_robot_emotion_t bk_robot_face_get_emotion(void);

/*
 * Maps Xiaozhi protocol emotion strings ("neutral", "happy", "laughing", "funny",
 * "sad", "crying", "thinking", "confused", "shocked", "surprised", "sleepy", etc.)
 * to bk_robot_emotion_t and updates the UI under the LVGL recursive lock.
 */
bk_err_t bk_robot_face_set_emotion_from_str(bk_ui_core_handle_t ui, const char *emotion_str);

/*
 * Computes the parameterized dual-eye & mouth animation frame at `now_ms`
 * (including periodic natural 150ms eye blinks every 4000ms and TTS lip-sync).
 */
bk_err_t bk_robot_face_tick_ms(bk_ui_core_handle_t ui,
                               uint64_t now_ms,
                               bool tts_speaking,
                               bk_robot_face_frame_t *out_frame);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_ROBOT_FACE_H */
