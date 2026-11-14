/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_WS_TRANSPORT_H
#define BK_OPEN_FRAMEWORK_BK_WS_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_err.h"
#include "bk_net.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BK_WS_FRAME_CONTINUATION = 0x0,
    BK_WS_FRAME_TEXT         = 0x1,
    BK_WS_FRAME_BINARY       = 0x2,
    BK_WS_FRAME_CLOSE        = 0x8,
    BK_WS_FRAME_PING         = 0x9,
    BK_WS_FRAME_PONG         = 0xA,
} bk_ws_frame_type_t;

typedef enum {
    BK_WS_STATE_DISCONNECTED = 0,
    BK_WS_STATE_CONNECTING,
    BK_WS_STATE_CONNECTED,
    BK_WS_STATE_RECONNECTING,
} bk_ws_state_t;

typedef struct bk_ws_transport *bk_ws_transport_handle_t;

typedef bk_err_t (*bk_ws_frame_cb_t)(bk_ws_frame_type_t type,
                                     const uint8_t *data,
                                     size_t len,
                                     void *user_ctx);

typedef void (*bk_ws_disconnect_cb_t)(bk_ws_transport_handle_t ws,
                                      int reason_code,
                                      void *user_ctx);

/*
 * Optional custom byte-stream I/O backend (e.g., custom TLS wrapper or test harness).
 * When NULL:
 * - On BK7259 target (BK_OF_BUILDING_TARGET_BK7259), bk_ws_transport uses native
 *   Armino mbedTLS + lwIP sockets for wss:// (verifying CA certificate chain and SNI)
 *   and lwIP sockets for ws:// (when allow_plain_ws == true).
 * - On POSIX host (posix_sim), bk_ws_transport uses non-blocking POSIX TCP sockets
 *   for ws:// (when allow_plain_ws == true) and requires io_ops for wss://.
 */
typedef struct {
    bk_err_t (*connect)(const char *host, uint16_t port, uint32_t timeout_ms, void *io_ctx);
    int (*send)(const uint8_t *buf, size_t len, void *io_ctx);
    int (*recv)(uint8_t *buf, size_t len, uint32_t timeout_ms, void *io_ctx);
    void (*close)(void *io_ctx);
    void *io_ctx;
} bk_ws_io_ops_t;

typedef struct {
    const char *uri;
    bk_net_handle_t net;
    bool verify_cert_chain;
    bool verify_sni;
    const uint8_t *ca_cert_pem;  /* Root CA PEM buffer (including trailing NUL for mbedTLS) */
    size_t ca_cert_len;          /* Length of ca_cert_pem in bytes */
    const char *sni_hostname;    /* Optional SNI override; defaults to URI host when NULL */
    bool allow_plain_ws;         /* Must be true to permit ws:// (e.g., localhost loopback test) */

    /* Xiaozhi WebSocket handshake headers */
    const char *auth_token;      /* Emits 'Authorization: Bearer <auth_token>' */
    uint32_t protocol_version;   /* Emits 'Protocol-Version: <N>' (defaults to 1 if 0) */
    const char *device_id;       /* Emits 'Device-Id: <device_id>' */
    const char *client_id;       /* Emits 'Client-Id: <client_id>' */
    bool require_xiaozhi_headers;/* If true, requires non-empty auth_token, device_id, client_id */

    uint32_t connect_timeout_ms; /* Default 5000ms if 0 */

    /* Optional custom stream I/O ops */
    const bk_ws_io_ops_t *io_ops;

    /* Frame receive & disconnect callbacks */
    bk_ws_frame_cb_t rx_cb;
    void *rx_cb_ctx;
    bk_ws_disconnect_cb_t disconnect_cb;
    void *disconnect_cb_ctx;

    /* Optional wire tap hook called after sending unmasked payload */
    bk_ws_frame_cb_t tx_hook;
    void *tx_hook_ctx;
} bk_ws_transport_config_t;

bk_err_t bk_ws_transport_create(const bk_ws_transport_config_t *cfg,
                                bk_ws_transport_handle_t *out_ws);
void bk_ws_transport_destroy(bk_ws_transport_handle_t ws);

bk_err_t bk_ws_transport_connect(bk_ws_transport_handle_t ws);
bk_err_t bk_ws_transport_disconnect(bk_ws_transport_handle_t ws);
bk_err_t bk_ws_transport_reconnect(bk_ws_transport_handle_t ws);
bool bk_ws_transport_is_connected(bk_ws_transport_handle_t ws);
bk_ws_state_t bk_ws_transport_get_state(bk_ws_transport_handle_t ws);
uint32_t bk_ws_transport_get_disconnect_count(bk_ws_transport_handle_t ws);

bk_err_t bk_ws_transport_send_text(bk_ws_transport_handle_t ws,
                                   const char *text,
                                   size_t len);
bk_err_t bk_ws_transport_send_binary(bk_ws_transport_handle_t ws,
                                     const uint8_t *data,
                                     size_t len);
bk_err_t bk_ws_transport_send_ping(bk_ws_transport_handle_t ws,
                                   const uint8_t *payload,
                                   size_t len);
bk_err_t bk_ws_transport_send_pong(bk_ws_transport_handle_t ws,
                                   const uint8_t *payload,
                                   size_t len);

/*
 * Polls and processes one incoming RFC 6455 WebSocket frame from the underlying
 * socket/TLS/io_ops:
 * - Enforces RFC 6455 RSV1/2/3 == 0, server MASK == 0, opcode legality,
 *   control frame rules (FIN == 1, len <= 125, CLOSE len != 1), and
 *   multi-frame fragmentation reassembly (FIN == 0 + CONTINUATION 0x0 frames)
 * - Automatically replies with a masked PONG frame when a PING (0x9) is received
 * - Transitions to DISCONNECTED and fires disconnect_cb when CLOSE (0x8) or protocol error arrives
 * - Dispatches complete TEXT (0x1) and BINARY (0x2) messages to rx_cb
 */
bk_err_t bk_ws_transport_poll(bk_ws_transport_handle_t ws, uint32_t timeout_ms);

/*
 * RFC 6455 helper: computes Base64(SHA-1(sec_ws_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")).
 */
bk_err_t bk_ws_compute_accept_key(const char *sec_ws_key,
                                  char *out_accept,
                                  size_t out_cap);

/*
 * Formats the HTTP/1.1 WebSocket Upgrade request including Xiaozhi headers.
 */
bk_err_t bk_ws_transport_build_upgrade_request(bk_ws_transport_handle_t ws,
                                               const char *sec_ws_key,
                                               char *out_req,
                                               size_t out_cap,
                                               size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_WS_TRANSPORT_H */
