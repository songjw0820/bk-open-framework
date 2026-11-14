/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_ROBOT_FACE_H
#define BK_OPEN_FRAMEWORK_BK_ROBOT_FACE_H

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
} bk_robot_emotion_t;

bk_err_t bk_robot_face_set_emotion(bk_ui_core_handle_t ui, bk_robot_emotion_t emotion);
bk_robot_emotion_t bk_robot_face_get_emotion(void);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_ROBOT_FACE_H */
