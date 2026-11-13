#include <string.h>
#include "bk_net.h"

void bk_lan_reset_for_net_stop(void);
void bk_provision_reset_for_net_stop(void);
void bk_provision_reset_for_net_connected(void);

static bk_net_state_t state = BK_NET_STATE_INITIAL;
static bk_net_config_t config;
static unsigned retries;
static int configured;

static int active(void) {
    return state != BK_NET_STATE_INITIAL && state != BK_NET_STATE_STOPPED;
}

bk_err_t bk_net_start(const bk_net_config_t *new_config) {
    bk_err_t err;
    if (!new_config) return BK_ERR_INVALID_ARG;
    if (active()) return BK_ERR_ALREADY_STARTED;
    config = *new_config;
    configured = 1;
    retries = 0;
    state = BK_NET_STATE_STARTING;
    if (config.start) {
        err = config.start(config.context);
        if (err != BK_OK) { state = BK_NET_STATE_DISCONNECTED; return err; }
    }
    if (!config.connect) { state = BK_NET_STATE_DISCONNECTED; return BK_OK; }
    state = BK_NET_STATE_CONNECTING;
    err = config.connect(config.context);
    if (err == BK_OK) { state = BK_NET_STATE_CONNECTED; retries = 0; }
    else { state = BK_NET_STATE_RETRYING; }
    return err;
}

bk_net_state_t bk_net_get_state(void) { return state; }

bk_err_t bk_net_notify_connected(void) {
    if (!active()) return BK_ERR_NOT_STARTED;
    bk_provision_reset_for_net_connected();
    state = BK_NET_STATE_CONNECTED;
    retries = 0;
    return BK_OK;
}

bk_err_t bk_net_notify_disconnected(void) {
    if (!active()) return BK_ERR_NOT_STARTED;
    state = BK_NET_STATE_DISCONNECTED;
    return BK_OK;
}

bk_err_t bk_net_disconnect(void) {
    bk_err_t err;
    if (!active()) return BK_ERR_NOT_STARTED;
    if (state == BK_NET_STATE_DISCONNECTED) return BK_OK;
    if (config.disconnect) {
        err = config.disconnect(config.context);
        if (err != BK_OK) return err;
    }
    state = BK_NET_STATE_DISCONNECTED;
    return BK_OK;
}

bk_err_t bk_net_retry(void) {
    bk_err_t err;
    unsigned limit;
    if (!active()) return BK_ERR_NOT_STARTED;
    if (state != BK_NET_STATE_DISCONNECTED && state != BK_NET_STATE_RETRYING) return BK_ERR_BUSY;
    limit = config.max_retries ? config.max_retries : 3U;
    if (!config.connect) return BK_ERR_NOT_FOUND;
    if (retries >= limit) return BK_ERR_LIMIT;
    state = BK_NET_STATE_RETRYING;
    retries++;
    err = config.connect(config.context);
    if (err == BK_OK) { state = BK_NET_STATE_CONNECTED; retries = 0; }
    return err;
}

bk_err_t bk_net_reprovision(void) {
    if (!active()) return BK_ERR_NOT_STARTED;
    if (state == BK_NET_STATE_PROVISIONING) return BK_ERR_ALREADY_STARTED;
    state = BK_NET_STATE_PROVISIONING;
    return BK_OK;
}

bk_err_t bk_net_stop(void) {
    bk_err_t err;
    if (!active()) return BK_ERR_NOT_STARTED;
    if (config.disconnect) {
        err = config.disconnect(config.context);
        if (err != BK_OK) return err;
    }
    bk_lan_reset_for_net_stop();
    bk_provision_reset_for_net_stop();
    state = BK_NET_STATE_STOPPED;
    return BK_OK;
}

bk_err_t bk_net_restart(void) {
    bk_net_config_t saved;
    bk_err_t err;
    if (!configured) return BK_ERR_NOT_STARTED;
    if (active()) {
        err = bk_net_stop();
        if (err != BK_OK) return err;
    }
    saved = config;
    return bk_net_start(&saved);
}
