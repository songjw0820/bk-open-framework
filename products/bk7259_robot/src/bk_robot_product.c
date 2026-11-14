/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_robot_product.h"
#include "bk_board.h"

#include <stdio.h>
#include <string.h>

bk_err_t bk_robot_product_mcp_dispatch(const char *method,
                                       const char *params_json,
                                       char *out_result_json,
                                       size_t out_result_cap,
                                       void *user_ctx)
{
    bk_robot_product_app_t *app = (bk_robot_product_app_t *)user_ctx;
    if (app == NULL || method == NULL || params_json == NULL || out_result_json == NULL) {
        return BK_ERR_INVALID_ARG;
    }

    if (strcmp(method, "tools/list") == 0) {
        snprintf(out_result_json,
                 out_result_cap,
                 "{\"tools\":[\"nod_head\",\"shake_head\",\"look_around\",\"get_battery_status\"]}");
        return BK_OK;
    }

    if (strcmp(method, "tools/call") != 0) {
        snprintf(out_result_json, out_result_cap, "{\"status\":\"unsupported_method\"}");
        return BK_OK;
    }

    if (strstr(params_json, "\"get_battery_status\"") != NULL) {
        snprintf(out_result_json,
                 out_result_cap,
                 "{\"status\":\"ok\",\"soc_pct\":%u,\"filtered_mv\":%u,\"allow_motion\":%s}",
                 (unsigned int)app->battery_soc_pct,
                 (unsigned int)app->battery_mon.filtered_mv,
                 app->motion_allowed ? "true" : "false");
        return BK_OK;
    }

    if (!app->motion_allowed) {
        snprintf(out_result_json, out_result_cap, "{\"status\":\"blocked_low_battery\"}");
        return BK_OK;
    }

    if (strstr(params_json, "\"nod_head\"") != NULL) {
        app->active_preset = BK_MOTION_PRESET_NOD_YES;
        (void)bk_robot_face_set_emotion(app->ui, BK_ROBOT_EMOTION_HAPPY);
    } else if (strstr(params_json, "\"shake_head\"") != NULL) {
        app->active_preset = BK_MOTION_PRESET_SHAKE_NO;
        (void)bk_robot_face_set_emotion(app->ui, BK_ROBOT_EMOTION_THINKING);
    } else if (strstr(params_json, "\"look_around\"") != NULL) {
        app->active_preset = BK_MOTION_PRESET_LOOK_AROUND;
        (void)bk_robot_face_set_emotion(app->ui, BK_ROBOT_EMOTION_SURPRISED);
    } else {
        app->active_preset = BK_MOTION_PRESET_RESET_CENTER;
    }

    bool finished = false;
    bk_err_t merr = bk_motion_eval_preset(
        app->active_preset, 180U, &app->current_yaw_deg, &app->current_pitch_deg, &finished);
    if (merr != BK_OK) {
        return merr;
    }

    snprintf(out_result_json,
             out_result_cap,
             "{\"status\":\"ok\",\"yaw\":%d,\"pitch\":%d}",
             (int)app->current_yaw_deg,
             (int)app->current_pitch_deg);
    return BK_OK;
}

bk_err_t bk_robot_product_init(bk_robot_product_app_t *app,
                               bk_ws_transport_handle_t ws)
{
    if (app == NULL || ws == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    memset(app, 0, sizeof(*app));
    bk_battery_monitor_init(&app->battery_mon);
    app->motion_allowed = true;

    bk_err_t err = bk_ui_core_create(&app->ui);
    if (err != BK_OK) {
        return err;
    }

    bk_audio_session_config_t acfg = {
        .downlink_ring_frames = 6U,
        .enable_aec = true,
    };
    err = bk_audio_session_create(&acfg, &app->audio);
    if (err != BK_OK) {
        bk_ui_core_destroy(app->ui);
        app->ui = NULL;
        return err;
    }

    bk_xiaozhi_config_t xcfg = {
        .ws = ws,
        .audio_session = app->audio,
        .mcp_cb = bk_robot_product_mcp_dispatch,
        .mcp_user_ctx = app,
        .enable_legacy_iot_compat = false,
    };
    err = bk_xiaozhi_create(&xcfg, &app->xz);
    if (err != BK_OK) {
        bk_audio_session_destroy(app->audio);
        bk_ui_core_destroy(app->ui);
        app->audio = NULL;
        app->ui = NULL;
        return err;
    }

    (void)bk_robot_face_set_emotion(app->ui, BK_ROBOT_EMOTION_LISTENING);
    return BK_OK;
}

void bk_robot_product_deinit(bk_robot_product_app_t *app)
{
    if (app == NULL) {
        return;
    }
    if (app->xz != NULL) {
        bk_xiaozhi_destroy(app->xz);
        app->xz = NULL;
    }
    if (app->audio != NULL) {
        bk_audio_session_destroy(app->audio);
        app->audio = NULL;
    }
    if (app->ui != NULL) {
        bk_ui_core_destroy(app->ui);
        app->ui = NULL;
    }
}

bk_err_t bk_robot_product_tick(bk_robot_product_app_t *app,
                               uint32_t raw_battery_mv,
                               bool is_charging,
                               uint64_t now_ms)
{
    if (app == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_err_t err = bk_battery_monitor_feed_mv(&app->battery_mon,
                                              raw_battery_mv,
                                              is_charging,
                                              &app->battery_level,
                                              &app->battery_soc_pct,
                                              &app->motion_allowed);
    if (err != BK_OK) {
        return err;
    }

    if (app->battery_level == BK_BATTERY_LEVEL_CRITICAL_CUTOFF) {
        (void)bk_robot_face_set_emotion(app->ui, BK_ROBOT_EMOTION_LOW_BATTERY);
    } else {
        const char *em = bk_xiaozhi_get_last_emotion(app->xz);
        if (em != NULL && em[0] != '\0' && strcmp(em, "neutral") != 0) {
            (void)bk_robot_face_set_emotion_from_str(app->ui, em);
        }
    }

    bk_xiaozhi_tts_state_t tts_st = bk_xiaozhi_get_tts_state(app->xz);
    bool speaking = (tts_st == BK_XIAOZHI_TTS_START || tts_st == BK_XIAOZHI_TTS_SENTENCE_START);
    return bk_robot_face_tick_ms(app->ui, now_ms, speaking, &app->last_face_frame);
}
