/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_PROTOCOL_XIAOZHI_H
#define BK_OPEN_FRAMEWORK_BK_PROTOCOL_XIAOZHI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_audio_session.h"
#include "bk_err.h"
#include "bk_ws_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BK_XIAOZHI_OPUS_60MS_PCM_SAMPLES  (BK_AUDIO_PCM_20MS_SAMPLES * 3U) /* 960 samples */
#define BK_XIAOZHI_MAX_SESSION_ID_LEN     64U

typedef struct bk_xiaozhi_session *bk_xiaozhi_handle_t;

typedef bk_err_t (*bk_xiaozhi_mcp_tool_cb_t)(const char *method,
                                             const char *params_json,
                                             char *out_result_json,
                                             size_t out_result_cap,
                                             void *user_ctx);

typedef struct {
    bk_ws_transport_handle_t ws;
    bk_audio_session_handle_t audio_session;
    bk_xiaozhi_mcp_tool_cb_t mcp_cb;
    void *mcp_user_ctx;
    bool enable_legacy_iot_compat; /* Default false (MCP is sole primary path) */
} bk_xiaozhi_config_t;

bk_err_t bk_xiaozhi_create(const bk_xiaozhi_config_t *cfg, bk_xiaozhi_handle_t *out_xz);
void bk_xiaozhi_destroy(bk_xiaozhi_handle_t xz);

/* Sends client 'hello' JSON requesting 16kHz/1ch/60ms Opus + MCP capability */
bk_err_t bk_xiaozhi_send_hello(bk_xiaozhi_handle_t xz);

/* Handles incoming JSON text frame ('hello' ack, 'mcp' JSON-RPC 2.0 request, etc.) */
bk_err_t bk_xiaozhi_on_text_frame(bk_xiaozhi_handle_t xz,
                                  const char *json_text,
                                  size_t len);

/*
 * Feeds a single 20ms PCM uplink frame (320 samples).
 * Accumulates 3 consecutive 20ms frames (960 samples = 60ms) and transmits a
 * single 60ms Opus binary packet over WebSocket when frame_duration == 60.
 */
bk_err_t bk_xiaozhi_push_uplink_pcm_20ms(bk_xiaozhi_handle_t xz,
                                         const int16_t *pcm_320,
                                         bool *out_packet_sent);

/*
 * Handles an incoming 60ms binary Opus packet from server, decodes into
 * 3 consecutive 20ms PCM frames (3 x 320 samples), and pushes them into
 * bk_audio_session downlink ringbuffer.
 */
bk_err_t bk_xiaozhi_on_binary_opus_60ms(bk_xiaozhi_handle_t xz,
                                        const uint8_t *opus_pkt,
                                        size_t pkt_len);

const char *bk_xiaozhi_get_session_id(bk_xiaozhi_handle_t xz);
uint32_t bk_xiaozhi_get_negotiated_frame_ms(bk_xiaozhi_handle_t xz);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_PROTOCOL_XIAOZHI_H */
