/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_robot_product.h"
#include "bk_board.h"
#include "bk_port_os.h"

#include <stdio.h>
#include <string.h>

static bool extract_json_str_field(const char *json,
                                   const char *key,
                                   char *out_buf,
                                   size_t out_cap)
{
    if (json == NULL || key == NULL || out_buf == NULL || out_cap < 2U) {
        return false;
    }
    char pattern[48];
    int pn = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (pn <= 0 || (size_t)pn >= sizeof(pattern)) {
        return false;
    }
    const char *pos = strstr(json, pattern);
    if (pos == NULL) {
        return false;
    }
    pos += pn;
    while (*pos == ' ' || *pos == '\t' || *pos == ':') {
        pos++;
    }
    if (*pos != '"') {
        return false;
    }
    pos++;
    const char *end = strchr(pos, '"');
    if (end == NULL || end == pos) {
        return false;
    }
    size_t len = (size_t)(end - pos);
    if (len >= out_cap) {
        len = out_cap - 1U;
    }
    memcpy(out_buf, pos, len);
    out_buf[len] = '\0';
    return true;
}

bk_err_t bk_robot_product_mcp_dispatch(const char *method,
                                       const char *params_json,
                                       char *out_result_json,
                                       size_t out_result_cap,
                                       void *user_ctx)
{
    bk_robot_product_app_t *app = (bk_robot_product_app_t *)user_ctx;
    if (app == NULL || method == NULL || params_json == NULL ||
        out_result_json == NULL || out_result_cap == 0U) {
        return BK_ERR_INVALID_ARG;
    }

    if (strcmp(method, "tools/list") == 0) {
        snprintf(out_result_json,
                 out_result_cap,
                 "{\"tools\":[\"nod_head\",\"shake_head\",\"look_around\","
                 "\"reset_head\",\"set_emotion\",\"get_battery_status\"]}");
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

    if (strstr(params_json, "\"set_emotion\"") != NULL) {
        char emotion[32] = {0};
        if (!extract_json_str_field(params_json, "emotion", emotion, sizeof(emotion))) {
            snprintf(out_result_json, out_result_cap, "{\"status\":\"invalid_emotion\"}");
            return BK_ERR_INVALID_ARG;
        }
        bk_err_t eerr = bk_robot_face_set_emotion_from_str(app->ui, emotion);
        if (eerr != BK_OK) {
            return eerr;
        }
        snprintf(out_result_json,
                 out_result_cap,
                 "{\"status\":\"ok\",\"emotion\":\"%s\"}",
                 emotion);
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
    } else if (strstr(params_json, "\"reset_head\"") != NULL) {
        app->active_preset = BK_MOTION_PRESET_RESET_CENTER;
        (void)bk_robot_face_set_emotion(app->ui, BK_ROBOT_EMOTION_NEUTRAL);
    } else {
        snprintf(out_result_json, out_result_cap, "{\"status\":\"unknown_tool\"}");
        return BK_OK;
    }

    app->preset_start_ms = bk_port_get_time_ms();
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

    if (app->motion_allowed && app->preset_start_ms > 0ULL && now_ms >= app->preset_start_ms) {
        uint64_t delta_ms = now_ms - app->preset_start_ms;
        uint32_t elapsed_ms = delta_ms > UINT32_MAX ? UINT32_MAX : (uint32_t)delta_ms;
        bool finished = false;
        (void)bk_motion_eval_preset(
            app->active_preset, elapsed_ms, &app->current_yaw_deg, &app->current_pitch_deg, &finished);
        if (finished) {
            app->preset_start_ms = 0ULL;
        }
    }

    bk_xiaozhi_tts_state_t tts_st = bk_xiaozhi_get_tts_state(app->xz);
    bool speaking = (tts_st == BK_XIAOZHI_TTS_START || tts_st == BK_XIAOZHI_TTS_SENTENCE_START);
    return bk_robot_face_tick_ms(app->ui, now_ms, speaking, &app->last_face_frame);
}
