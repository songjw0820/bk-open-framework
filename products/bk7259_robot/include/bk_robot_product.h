/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_ROBOT_PRODUCT_H
#define BK_OPEN_FRAMEWORK_BK_ROBOT_PRODUCT_H

#include <stdbool.h>
#include <stdint.h>
#include "bk_audio_session.h"
#include "bk_battery_policy.h"
#include "bk_err.h"
#include "bk_motion.h"
#include "bk_protocol_xiaozhi.h"
#include "bk_robot_face.h"
#include "bk_ui_core.h"
#include "bk_ws_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bk_ui_core_handle_t ui;
    bk_audio_session_handle_t audio;
    bk_xiaozhi_handle_t xz;
    bk_battery_monitor_t battery_mon;
    bk_battery_level_t battery_level;
    uint8_t battery_soc_pct;
    bool motion_allowed;
    bk_motion_preset_t active_preset;
    uint64_t preset_start_ms;
    int16_t current_yaw_deg;
    int16_t current_pitch_deg;
    bk_robot_face_frame_t last_face_frame;
} bk_robot_product_app_t;

bk_err_t bk_robot_product_init(bk_robot_product_app_t *app,
                               bk_ws_transport_handle_t ws);
void bk_robot_product_deinit(bk_robot_product_app_t *app);

/*
 * Built-in MCP JSON-RPC 2.0 tool dispatcher for bk7259_robot:
 * Supported tools:
 * - "nod_head" / "shake_head" / "look_around" / "reset_head"
 * - "set_emotion"
 * - "get_battery_status"
 */
bk_err_t bk_robot_product_mcp_dispatch(const char *method,
                                       const char *params_json,
                                       char *out_result_json,
                                       size_t out_result_cap,
                                       void *user_ctx);

/*
 * Periodic product tick (evaluates battery filter, syncs Xiaozhi emotion/TTS
 * state to bk_robot_face, and steps servo S-curve trajectory if motion is allowed).
 */
bk_err_t bk_robot_product_tick(bk_robot_product_app_t *app,
                               uint32_t raw_battery_mv,
                               bool is_charging,
                               uint64_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_ROBOT_PRODUCT_H */
