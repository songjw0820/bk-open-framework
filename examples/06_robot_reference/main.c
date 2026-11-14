/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_audio_session.h"
#include "bk_battery_policy.h"
#include "bk_board.h"
#include "bk_core_event.h"
#include "bk_motion.h"
#include "bk_net.h"
#include "bk_plugin_agora.h"
#include "bk_protocol_xiaozhi.h"
#include "bk_robot_face.h"
#include "bk_storage.h"
#include "bk_ui_core.h"
#include "bk_ws_transport.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static bk_err_t robot_mcp_handler(const char *method,
                                  const char *params_json,
                                  char *out_result_json,
                                  size_t out_result_cap,
                                  void *user_ctx)
{
    (void)method;
    (void)params_json;
    bk_ui_core_handle_t ui = (bk_ui_core_handle_t)user_ctx;
    (void)bk_robot_face_set_emotion(ui, BK_ROBOT_EMOTION_HAPPY);

    bk_motion_keyframe_t kf0 = {.timestamp_ms = 0, .yaw_deg = 0, .pitch_deg = 0};
    bk_motion_keyframe_t kf1 = {.timestamp_ms = 100, .yaw_deg = 30, .pitch_deg = 10};
    int16_t yaw = 0;
    int16_t pitch = 0;
    bk_err_t err = bk_motion_interpolate(&kf0, &kf1, 50, &yaw, &pitch);
    if (err != BK_OK) {
        return err;
    }
    snprintf(out_result_json,
             out_result_cap,
             "{\"status\":\"ok\",\"yaw\":%d,\"pitch\":%d}",
             (int)yaw,
             (int)pitch);
    return BK_OK;
}

int main(void)
{
    assert(bk_board_init(BK_BOARD_PROFILE_ROBOT) == BK_OK);

    /* Open-source default build must NOT require proprietary Agora plugin */
    assert(!bk_plugin_agora_is_compiled_in());

    bk_battery_level_t blevel = BK_BATTERY_LEVEL_NORMAL;
    bool allow_motion = false;
    assert(bk_battery_evaluate_mv(3850U, &blevel, &allow_motion) == BK_OK);
    assert(blevel == BK_BATTERY_LEVEL_NORMAL && allow_motion);

    bk_ui_core_handle_t ui = NULL;
    assert(bk_ui_core_create(&ui) == BK_OK);
    assert(bk_robot_face_set_emotion(ui, BK_ROBOT_EMOTION_LISTENING) == BK_OK);

    bk_audio_session_config_t acfg = {
        .downlink_ring_frames = 6,
        .enable_aec = true,
    };
    bk_audio_session_handle_t audio = NULL;
    assert(bk_audio_session_create(&acfg, &audio) == BK_OK);

    bk_ws_transport_config_t wcfg = {
        .uri = "wss://api.tenclass.net/xiaozhi/v1/",
        .net = NULL,
        .verify_cert_chain = true,
        .verify_sni = true,
    };
    bk_ws_transport_handle_t ws = NULL;
    assert(bk_ws_transport_create(&wcfg, &ws) == BK_OK);
    assert(bk_ws_transport_connect(ws) == BK_OK);

    bk_xiaozhi_config_t xcfg = {
        .ws = ws,
        .audio_session = audio,
        .mcp_cb = robot_mcp_handler,
        .mcp_user_ctx = ui,
        .enable_legacy_iot_compat = false,
    };
    bk_xiaozhi_handle_t xz = NULL;
    assert(bk_xiaozhi_create(&xcfg, &xz) == BK_OK);

    assert(bk_xiaozhi_send_hello(xz) == BK_OK);
    const char *srv_hello =
        "{\"type\":\"hello\",\"session_id\":\"robot-ref-06\",\"audio_params\":{\"frame_duration\":60}}";
    assert(bk_xiaozhi_on_text_frame(xz, srv_hello, strlen(srv_hello)) == BK_OK);

    const char *mcp_call =
        "{\"session_id\":\"robot-ref-06\",\"type\":\"mcp\",\"payload\":"
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"nod_head\"}}}";
    assert(bk_xiaozhi_on_text_frame(xz, mcp_call, strlen(mcp_call)) == BK_OK);
    assert(bk_robot_face_get_emotion( ) == BK_ROBOT_EMOTION_HAPPY);

    bk_xiaozhi_destroy(xz);
    bk_ws_transport_destroy(ws);
    bk_audio_session_destroy(audio);
    bk_ui_core_destroy(ui);
    bk_board_deinit();

    printf("PASS: example 06_robot_reference (open-source Xiaozhi MCP + motion + face + battery policy)\n");
    return 0;
}
