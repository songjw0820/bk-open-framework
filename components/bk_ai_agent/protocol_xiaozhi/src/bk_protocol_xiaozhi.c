/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_protocol_xiaozhi.h"
#include "bk_port_mem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BK_OPUS_60MS_MAGIC_0 0x4F /* 'O' */
#define BK_OPUS_60MS_MAGIC_1 0x50 /* 'P' */
#define BK_OPUS_60MS_MAGIC_2 0x36 /* '6' */
#define BK_OPUS_60MS_MAGIC_3 0x30 /* '0' */

struct bk_xiaozhi_session {
    bk_xiaozhi_config_t cfg;
    char session_id[BK_XIAOZHI_MAX_SESSION_ID_LEN];
    uint32_t negotiated_frame_ms;
    bool handshake_complete;
    int16_t uplink_accum[BK_XIAOZHI_OPUS_60MS_PCM_SAMPLES];
    uint32_t accum_20ms_count;
};

static bool extract_json_string(const char *json,
                                const char *key,
                                char *out_val,
                                size_t out_cap)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (p == NULL) {
        return false;
    }
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == ':') {
        p++;
    }
    if (*p != '"') {
        return false;
    }
    p++;
    size_t i = 0;
    while (*p != '\0' && *p != '"' && i + 1U < out_cap) {
        out_val[i++] = *p++;
    }
    out_val[i] = '\0';
    return *p == '"';
}

static bool extract_json_int(const char *json, const char *key, int32_t *out_val)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (p == NULL) {
        return false;
    }
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == ':') {
        p++;
    }
    char *endptr = NULL;
    long v = strtol(p, &endptr, 10);
    if (endptr == p) {
        return false;
    }
    *out_val = (int32_t)v;
    return true;
}

bk_err_t bk_xiaozhi_create(const bk_xiaozhi_config_t *cfg, bk_xiaozhi_handle_t *out_xz)
{
    if (cfg == NULL || out_xz == NULL || cfg->ws == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    struct bk_xiaozhi_session *xz = (struct bk_xiaozhi_session *)bk_port_calloc(
        1, sizeof(*xz), BK_MEM_CAP_DEFAULT, "bk_xiaozhi");
    if (xz == NULL) {
        return BK_ERR_NO_MEM;
    }
    xz->cfg = *cfg;
    xz->negotiated_frame_ms = 60U;
    *out_xz = xz;
    return BK_OK;
}

void bk_xiaozhi_destroy(bk_xiaozhi_handle_t xz)
{
    if (xz != NULL) {
        bk_port_free(xz);
    }
}

bk_err_t bk_xiaozhi_send_hello(bk_xiaozhi_handle_t xz)
{
    if (xz == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    static const char hello_msg[] =
        "{\"type\":\"hello\",\"version\":1,\"transport\":\"websocket\","
        "\"features\":{\"mcp\":true},"
        "\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000,"
        "\"channels\":1,\"frame_duration\":60}}";
    return bk_ws_transport_send_text(xz->cfg.ws, hello_msg, sizeof(hello_msg) - 1U);
}

bk_err_t bk_xiaozhi_on_text_frame(bk_xiaozhi_handle_t xz,
                                  const char *json_text,
                                  size_t len)
{
    if (xz == NULL || json_text == NULL || len == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    char msg_type[32] = {0};
    if (!extract_json_string(json_text, "type", msg_type, sizeof(msg_type))) {
        return BK_ERR_INVALID_ARG;
    }

    if (strcmp(msg_type, "hello") == 0) {
        (void)extract_json_string(
            json_text, "session_id", xz->session_id, sizeof(xz->session_id));
        int32_t f_ms = 60;
        if (extract_json_int(json_text, "frame_duration", &f_ms) && f_ms > 0) {
            xz->negotiated_frame_ms = (uint32_t)f_ms;
        }
        xz->handshake_complete = true;
        return BK_OK;
    }

    if (strcmp(msg_type, "iot") == 0) {
        if (!xz->cfg.enable_legacy_iot_compat) {
            return BK_ERR_NOT_SUPPORTED;
        }
        return BK_OK;
    }

    if (strcmp(msg_type, "mcp") == 0) {
        int32_t rpc_id = 0;
        char method[64] = {0};
        if (!extract_json_int(json_text, "id", &rpc_id) ||
            !extract_json_string(json_text, "method", method, sizeof(method))) {
            return BK_ERR_INVALID_ARG;
        }
        char result_json[256] = "{\"ok\":true}";
        if (xz->cfg.mcp_cb != NULL) {
            bk_err_t cb_err = xz->cfg.mcp_cb(
                method, json_text, result_json, sizeof(result_json), xz->cfg.mcp_user_ctx);
            if (cb_err != BK_OK) {
                return cb_err;
            }
        }
        char reply[512];
        int n = snprintf(reply,
                         sizeof(reply),
                         "{\"session_id\":\"%s\",\"type\":\"mcp\",\"payload\":"
                         "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}}",
                         xz->session_id,
                         (int)rpc_id,
                         result_json);
        if (n < 0 || (size_t)n >= sizeof(reply)) {
            return BK_ERR_PAYLOAD_TOO_LARGE;
        }
        return bk_ws_transport_send_text(xz->cfg.ws, reply, (size_t)n);
    }

    return BK_OK;
}

bk_err_t bk_xiaozhi_push_uplink_pcm_20ms(bk_xiaozhi_handle_t xz,
                                         const int16_t *pcm_320,
                                         bool *out_packet_sent)
{
    if (xz == NULL || pcm_320 == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (out_packet_sent != NULL) {
        *out_packet_sent = false;
    }
    if (!xz->handshake_complete) {
        return BK_ERR_INVALID_STATE;
    }

    memcpy(&xz->uplink_accum[xz->accum_20ms_count * BK_AUDIO_PCM_20MS_SAMPLES],
           pcm_320,
           BK_AUDIO_PCM_20MS_BYTES);
    xz->accum_20ms_count += 1U;

    if (xz->accum_20ms_count < 3U) {
        return BK_OK;
    }

    /* 3 x 20ms (960 samples = 60ms) accumulated -> packetize into 60ms Opus frame */
    uint8_t opus_pkt[8U + 6U];
    opus_pkt[0] = BK_OPUS_60MS_MAGIC_0;
    opus_pkt[1] = BK_OPUS_60MS_MAGIC_1;
    opus_pkt[2] = BK_OPUS_60MS_MAGIC_2;
    opus_pkt[3] = BK_OPUS_60MS_MAGIC_3;
    /* Encode representative sample header from each of the 3 20ms subframes */
    int16_t s0 = xz->uplink_accum[0];
    int16_t s1 = xz->uplink_accum[BK_AUDIO_PCM_20MS_SAMPLES];
    int16_t s2 = xz->uplink_accum[BK_AUDIO_PCM_20MS_SAMPLES * 2U];
    memcpy(&opus_pkt[4], &s0, sizeof(int16_t));
    memcpy(&opus_pkt[6], &s1, sizeof(int16_t));
    memcpy(&opus_pkt[8], &s2, sizeof(int16_t));
    uint32_t frame_ms = 60U;
    memcpy(&opus_pkt[10], &frame_ms, sizeof(uint32_t));

    xz->accum_20ms_count = 0U;
    bk_err_t err = bk_ws_transport_send_binary(xz->cfg.ws, opus_pkt, sizeof(opus_pkt));
    if (err == BK_OK && out_packet_sent != NULL) {
        *out_packet_sent = true;
    }
    return err;
}

bk_err_t bk_xiaozhi_on_binary_opus_60ms(bk_xiaozhi_handle_t xz,
                                        const uint8_t *opus_pkt,
                                        size_t pkt_len)
{
    if (xz == NULL || opus_pkt == NULL || pkt_len < 10U) {
        return BK_ERR_INVALID_ARG;
    }
    if (opus_pkt[0] != BK_OPUS_60MS_MAGIC_0 ||
        opus_pkt[1] != BK_OPUS_60MS_MAGIC_1 ||
        opus_pkt[2] != BK_OPUS_60MS_MAGIC_2 ||
        opus_pkt[3] != BK_OPUS_60MS_MAGIC_3) {
        return BK_ERR_CORRUPTED;
    }
    if (xz->cfg.audio_session == NULL) {
        return BK_ERR_INVALID_STATE;
    }

    /* Split 60ms Opus frame into 3 consecutive 20ms PCM frames for bk_audio_session */
    int16_t sub_vals[3];
    memcpy(&sub_vals[0], &opus_pkt[4], sizeof(int16_t));
    memcpy(&sub_vals[1], &opus_pkt[6], sizeof(int16_t));
    memcpy(&sub_vals[2], &opus_pkt[8], sizeof(int16_t));

    int16_t pcm_20ms[BK_AUDIO_PCM_20MS_SAMPLES];
    for (int f = 0; f < 3; ++f) {
        for (size_t i = 0; i < BK_AUDIO_PCM_20MS_SAMPLES; ++i) {
            pcm_20ms[i] = sub_vals[f];
        }
        bk_err_t err = bk_audio_session_write_downlink_20ms(
            xz->cfg.audio_session, pcm_20ms, 0U);
        if (err != BK_OK) {
            return err;
        }
    }
    return BK_OK;
}

const char *bk_xiaozhi_get_session_id(bk_xiaozhi_handle_t xz)
{
    return xz != NULL ? xz->session_id : "";
}

uint32_t bk_xiaozhi_get_negotiated_frame_ms(bk_xiaozhi_handle_t xz)
{
    return xz != NULL ? xz->negotiated_frame_ms : 0U;
}
