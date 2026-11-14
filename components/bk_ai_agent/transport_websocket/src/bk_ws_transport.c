/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_ws_transport.h"
#include "bk_port_mem.h"

#include <string.h>

struct bk_ws_transport {
    bk_ws_transport_config_t cfg;
    bool connected;
};

bk_err_t bk_ws_transport_create(const bk_ws_transport_config_t *cfg,
                                bk_ws_transport_handle_t *out_ws)
{
    if (cfg == NULL || out_ws == NULL || cfg->uri == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    struct bk_ws_transport *ws = (struct bk_ws_transport *)bk_port_calloc(
        1, sizeof(*ws), BK_MEM_CAP_DEFAULT, "bk_ws");
    if (ws == NULL) {
        return BK_ERR_NO_MEM;
    }
    ws->cfg = *cfg;
    *out_ws = ws;
    return BK_OK;
}

void bk_ws_transport_destroy(bk_ws_transport_handle_t ws)
{
    if (ws != NULL) {
        bk_port_free(ws);
    }
}

bk_err_t bk_ws_transport_connect(bk_ws_transport_handle_t ws)
{
    if (ws == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (ws->cfg.net != NULL) {
        bk_err_t tls_err = bk_net_require_tls_ready(
            ws->cfg.net, ws->cfg.verify_cert_chain, ws->cfg.verify_sni);
        if (tls_err != BK_OK) {
            return tls_err;
        }
    } else if (!ws->cfg.verify_cert_chain || !ws->cfg.verify_sni) {
        return BK_ERR_NET_TLS_VERIFY_FAILED;
    }
    ws->connected = true;
    return BK_OK;
}

bk_err_t bk_ws_transport_disconnect(bk_ws_transport_handle_t ws)
{
    if (ws == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    ws->connected = false;
    return BK_OK;
}

bool bk_ws_transport_is_connected(bk_ws_transport_handle_t ws)
{
    return ws != NULL && ws->connected;
}

bk_err_t bk_ws_transport_send_text(bk_ws_transport_handle_t ws,
                                   const char *text,
                                   size_t len)
{
    if (ws == NULL || text == NULL || len == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    if (!ws->connected) {
        return BK_ERR_INVALID_STATE;
    }
    if (ws->cfg.tx_hook != NULL) {
        return ws->cfg.tx_hook(
            BK_WS_FRAME_TEXT, (const uint8_t *)text, len, ws->cfg.tx_hook_ctx);
    }
    return BK_OK;
}

bk_err_t bk_ws_transport_send_binary(bk_ws_transport_handle_t ws,
                                     const uint8_t *data,
                                     size_t len)
{
    if (ws == NULL || data == NULL || len == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    if (!ws->connected) {
        return BK_ERR_INVALID_STATE;
    }
    if (ws->cfg.tx_hook != NULL) {
        return ws->cfg.tx_hook(BK_WS_FRAME_BINARY, data, len, ws->cfg.tx_hook_ctx);
    }
    return BK_OK;
}
