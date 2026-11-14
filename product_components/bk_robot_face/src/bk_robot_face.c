/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_robot_face.h"

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
