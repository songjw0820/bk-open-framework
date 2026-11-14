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
    BK_WS_FRAME_TEXT   = 1,
    BK_WS_FRAME_BINARY = 2,
    BK_WS_FRAME_PING   = 9,
    BK_WS_FRAME_PONG   = 10,
} bk_ws_frame_type_t;

typedef struct bk_ws_transport *bk_ws_transport_handle_t;

typedef bk_err_t (*bk_ws_tx_hook_t)(bk_ws_frame_type_t type,
                                    const uint8_t *data,
                                    size_t len,
                                    void *user_ctx);

typedef struct {
    const char *uri;
    bk_net_handle_t net;
    bool verify_cert_chain;
    bool verify_sni;
    bk_ws_tx_hook_t tx_hook;
    void *tx_hook_ctx;
} bk_ws_transport_config_t;

bk_err_t bk_ws_transport_create(const bk_ws_transport_config_t *cfg,
                                bk_ws_transport_handle_t *out_ws);
void bk_ws_transport_destroy(bk_ws_transport_handle_t ws);

bk_err_t bk_ws_transport_connect(bk_ws_transport_handle_t ws);
bk_err_t bk_ws_transport_disconnect(bk_ws_transport_handle_t ws);
bool bk_ws_transport_is_connected(bk_ws_transport_handle_t ws);

bk_err_t bk_ws_transport_send_text(bk_ws_transport_handle_t ws,
                                   const char *text,
                                   size_t len);
bk_err_t bk_ws_transport_send_binary(bk_ws_transport_handle_t ws,
                                     const uint8_t *data,
                                     size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_WS_TRANSPORT_H */
