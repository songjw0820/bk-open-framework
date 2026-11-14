/* SPDX-License-Identifier: Apache-2.0 */
#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L

#include "bk_audio_session.h"
#include "bk_battery_policy.h"
#include "bk_board.h"
#include "bk_core_event.h"
#include "bk_motion.h"
#include "bk_of_net.h"
#include "bk_plugin_agora.h"
#include "bk_protocol_xiaozhi.h"
#include "bk_robot_face.h"
#include "bk_storage.h"
#include "bk_ui_core.h"
#include "bk_ws_transport.h"

#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int send_all_fd(int fd, const uint8_t *buf, size_t len)
{
    size_t sent = 0U;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n <= 0) {
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

static int recv_all_fd(int fd, uint8_t *buf, size_t len)
{
    size_t got = 0U;
    while (got < len) {
        ssize_t n = recv(fd, buf + got, len - got, 0);
        if (n <= 0) {
            return -1;
        }
        got += (size_t)n;
    }
    return 0;
}

static int srv_send_ws_frame(int fd, uint8_t opcode, const uint8_t *payload, size_t len)
{
    uint8_t hdr[4];
    size_t hlen = 0U;
    hdr[hlen++] = (uint8_t)(0x80U | (opcode & 0x0FU));
    if (len <= 125U) {
        hdr[hlen++] = (uint8_t)len;
    } else {
        hdr[hlen++] = 126U;
        hdr[hlen++] = (uint8_t)((len >> 8) & 0xFFU);
        hdr[hlen++] = (uint8_t)(len & 0xFFU);
    }
    if (send_all_fd(fd, hdr, hlen) != 0) {
        return -1;
    }
    return (len > 0U) ? send_all_fd(fd, payload, len) : 0;
}

static int srv_recv_masked_ws_frame(int fd,
                                    uint8_t *out_opcode,
                                    uint8_t *out_buf,
                                    size_t cap,
                                    size_t *out_len)
{
    uint8_t hdr2[2];
    if (recv_all_fd(fd, hdr2, 2U) != 0) {
        return -1;
    }
    *out_opcode = (uint8_t)(hdr2[0] & 0x0FU);
    if ((hdr2[1] & 0x80U) == 0U) {
        return -1;
    }
    size_t len = (size_t)(hdr2[1] & 0x7FU);
    if (len == 126U) {
        uint8_t ext[2];
        if (recv_all_fd(fd, ext, 2U) != 0) {
            return -1;
        }
        len = ((size_t)ext[0] << 8) | (size_t)ext[1];
    }
    if (len + 1U > cap) {
        return -1;
    }
    uint8_t mask[4];
    if (recv_all_fd(fd, mask, 4U) != 0) {
        return -1;
    }
    if (len > 0U && recv_all_fd(fd, out_buf, len) != 0) {
        return -1;
    }
    for (size_t i = 0U; i < len; ++i) {
        out_buf[i] ^= mask[i & 3U];
    }
    out_buf[len] = 0U;
    *out_len = len;
    return 0;
}

static void *robot_loopback_server(void *arg)
{
    int listen_fd = *(int *)arg;
    int cfd = accept(listen_fd, NULL, NULL);
    if (cfd < 0) {
        return NULL;
    }

    char req[1024];
    size_t req_len = 0U;
    while (req_len + 1U < sizeof(req)) {
        uint8_t ch = 0;
        if (recv_all_fd(cfd, &ch, 1U) != 0) {
            close(cfd);
            return NULL;
        }
        req[req_len++] = (char)ch;
        req[req_len] = '\0';
        if (req_len >= 4U && memcmp(&req[req_len - 4U], "\r\n\r\n", 4U) == 0) {
            break;
        }
    }

    const char *k = strstr(req, "Sec-WebSocket-Key: ");
    assert(k != NULL);
    k += strlen("Sec-WebSocket-Key: ");
    const char *ke = strstr(k, "\r\n");
    assert(ke != NULL);
    char sec_key[64] = {0};
    memcpy(sec_key, k, (size_t)(ke - k));

    char accept_key[64] = {0};
    assert(bk_ws_compute_accept_key(sec_key, accept_key, sizeof(accept_key)) == BK_OK);

    char resp[512];
    int n = snprintf(resp,
                     sizeof(resp),
                     "HTTP/1.1 101 Switching Protocols\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "Sec-WebSocket-Accept: %s\r\n\r\n",
                     accept_key);
    assert(send_all_fd(cfd, (const uint8_t *)resp, (size_t)n) == 0);

    uint8_t op = 0;
    uint8_t buf[1024];
    size_t flen = 0U;
    /* Receive client hello */
    assert(srv_recv_masked_ws_frame(cfd, &op, buf, sizeof(buf), &flen) == 0);
    assert(op == BK_WS_FRAME_TEXT);

    /* Send server hello ack */
    const char *srv_hello =
        "{\"type\":\"hello\",\"session_id\":\"robot-ref-06\",\"audio_params\":{\"frame_duration\":60}}";
    assert(srv_send_ws_frame(cfd, BK_WS_FRAME_TEXT, (const uint8_t *)srv_hello, strlen(srv_hello)) == 0);

    /* Send MCP JSON-RPC 2.0 tool call to nod head */
    const char *mcp_call =
        "{\"session_id\":\"robot-ref-06\",\"type\":\"mcp\",\"payload\":"
        "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"nod_head\"}}}";
    assert(srv_send_ws_frame(cfd, BK_WS_FRAME_TEXT, (const uint8_t *)mcp_call, strlen(mcp_call)) == 0);

    /* Receive client MCP JSON-RPC 2.0 response */
    assert(srv_recv_masked_ws_frame(cfd, &op, buf, sizeof(buf), &flen) == 0);
    assert(op == BK_WS_FRAME_TEXT);
    assert(strstr((const char *)buf, "\"status\":\"ok\"") != NULL);

    /* Receive client 60ms Opus frame and echo it back */
    assert(srv_recv_masked_ws_frame(cfd, &op, buf, sizeof(buf), &flen) == 0);
    assert(op == BK_WS_FRAME_BINARY && flen > 2U);
    assert(srv_send_ws_frame(cfd, BK_WS_FRAME_BINARY, buf, flen) == 0);

    close(cfd);
    return NULL;
}

static bk_err_t robot_mcp_handler(const char *method,
                                  const char *params_json,
                                  char *out_result_json,
                                  size_t out_result_cap,
                                  void *user_ctx)
{
    assert(strcmp(method, "tools/call") == 0);
    assert(strcmp(params_json, "{\"name\":\"nod_head\"}") == 0);
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

static bk_err_t on_robot_ws_rx(bk_ws_frame_type_t type,
                               const uint8_t *data,
                               size_t len,
                               void *user_ctx)
{
    bk_xiaozhi_handle_t *xz_ptr = (bk_xiaozhi_handle_t *)user_ctx;
    if (xz_ptr == NULL || *xz_ptr == NULL) {
        return BK_ERR_INVALID_STATE;
    }
    if (type == BK_WS_FRAME_TEXT) {
        return bk_xiaozhi_on_text_frame(*xz_ptr, (const char *)data, len);
    }
    if (type == BK_WS_FRAME_BINARY) {
        return bk_xiaozhi_on_binary_opus_60ms(*xz_ptr, data, len);
    }
    return BK_OK;
}

int main(void)
{
    assert(bk_board_init(BK_BOARD_PROFILE_ROBOT) == BK_OK);

    /* Open-source default build must NOT require proprietary Agora plugin */
    assert(!bk_plugin_agora_is_compiled_in());
    assert(!bk_plugin_agora_is_sdk_linked());

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

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listen_fd >= 0);
    int opt = 1;
    (void)setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    assert(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);
    addr.sin_port = 0;
    assert(bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    assert(listen(listen_fd, 1) == 0);
    socklen_t alen = sizeof(addr);
    assert(getsockname(listen_fd, (struct sockaddr *)&addr, &alen) == 0);
    uint16_t port = ntohs(addr.sin_port);

    pthread_t srv_tid;
    assert(pthread_create(&srv_tid, NULL, robot_loopback_server, &listen_fd) == 0);

    char uri[128];
    snprintf(uri, sizeof(uri), "ws://127.0.0.1:%u/xiaozhi/v1/", (unsigned int)port);

    bk_xiaozhi_handle_t xz = NULL;
    bk_ws_transport_config_t wcfg = {
        .uri = uri,
        .allow_plain_ws = true,
        .auth_token = "robot-ref-token",
        .protocol_version = 1U,
        .device_id = "c8:47:8c:72:59:06",
        .client_id = "06000000-0000-4000-8000-000000000006",
        .require_xiaozhi_headers = true,
        .connect_timeout_ms = 3000U,
        .rx_cb = on_robot_ws_rx,
        .rx_cb_ctx = &xz,
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
    assert(bk_xiaozhi_create(&xcfg, &xz) == BK_OK);
    assert(bk_xiaozhi_is_native_libopus_loaded(xz));

    /* 1. Send client hello and receive server hello ack */
    assert(bk_xiaozhi_send_hello(xz) == BK_OK);
    assert(bk_ws_transport_poll(ws, 2000U) == BK_OK);
    assert(strcmp(bk_xiaozhi_get_session_id(xz), "robot-ref-06") == 0);

    /* 2. Receive MCP JSON-RPC 2.0 tool call and verify emotion + motion interpolation */
    assert(bk_ws_transport_poll(ws, 2000U) == BK_OK);
    assert(bk_robot_face_get_emotion() == BK_ROBOT_EMOTION_HAPPY);

    /* 3. Push 3 x 20ms PCM frames -> encode 60ms Opus frame -> echo from server -> decode 3 x 20ms PCM */
    int16_t pcm[BK_AUDIO_PCM_20MS_SAMPLES];
    for (size_t i = 0U; i < BK_AUDIO_PCM_20MS_SAMPLES; ++i) {
        pcm[i] = (int16_t)((i % 32U) < 16U ? 4000 : -4000);
    }
    bool sent = false;
    assert(bk_xiaozhi_push_uplink_pcm_20ms(xz, pcm, &sent) == BK_OK && !sent);
    assert(bk_xiaozhi_push_uplink_pcm_20ms(xz, pcm, &sent) == BK_OK && !sent);
    assert(bk_xiaozhi_push_uplink_pcm_20ms(xz, pcm, &sent) == BK_OK && sent);

    assert(bk_ws_transport_poll(ws, 2000U) == BK_OK);
    assert(bk_audio_session_get_queued_frames(audio) == 3U);

    /* 4. Verify robot product components: 4-sample anti-droop battery filter, S-curve preset, and face tick */
    bk_battery_monitor_t bmon;
    bk_battery_monitor_init(&bmon);
    uint8_t soc = 0U;
    assert(bk_battery_monitor_feed_mv(&bmon, 3900U, false, &blevel, &soc, &allow_motion) == BK_OK);
    assert(bk_battery_monitor_feed_mv(&bmon, 3900U, false, &blevel, &soc, &allow_motion) == BK_OK);
    assert(bk_battery_monitor_feed_mv(&bmon, 3900U, false, &blevel, &soc, &allow_motion) == BK_OK);
    /* Single transient servo-start droop to 3400mV must NOT trigger CRITICAL_CUTOFF (average = 3775mV) */
    assert(bk_battery_monitor_feed_mv(&bmon, 3400U, false, &blevel, &soc, &allow_motion) == BK_OK);
    assert(blevel == BK_BATTERY_LEVEL_NORMAL && allow_motion && soc > 35U);

    int16_t s_yaw = 0;
    int16_t s_pitch = 0;
    bool seq_done = false;
    assert(bk_motion_eval_preset(BK_MOTION_PRESET_NOD_YES, 180U, &s_yaw, &s_pitch, &seq_done) == BK_OK);
    assert(s_pitch == 20 && !seq_done);

    bk_robot_face_frame_t fframe;
    assert(bk_robot_face_set_emotion_from_str(ui, "thinking") == BK_OK);
    assert(bk_robot_face_tick_ms(ui, 1000ULL, true, &fframe) == BK_OK);
    assert(fframe.emotion == BK_ROBOT_EMOTION_THINKING && fframe.mouth_open_pct > 0U);

    pthread_join(srv_tid, NULL);
    close(listen_fd);

    bk_xiaozhi_destroy(xz);
    bk_ws_transport_destroy(ws);
    bk_audio_session_destroy(audio);
    bk_ui_core_destroy(ui);
    bk_board_deinit();

    printf("PASS: example 06_robot_reference [Tier 1 Host Loopback WS + Native libopus + MCP + Robot Face/Motion/Battery]\n");
    return 0;
}
