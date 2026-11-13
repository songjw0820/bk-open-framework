#include "bk_provision.h"

static bk_provision_config_t config;
static int started;
static int in_callback;

bk_err_t bk_provision_start(const bk_provision_config_t *new_config) {
    bk_err_t err;
    if (in_callback) return BK_ERR_REENTRANT;
    if (!new_config) return BK_ERR_INVALID_ARG;
    if (bk_net_get_state() == BK_NET_STATE_INITIAL || bk_net_get_state() == BK_NET_STATE_STOPPED) return BK_ERR_NOT_STARTED;
    if (started) return BK_ERR_ALREADY_STARTED;
    if (!new_config->start) return BK_ERR_NOT_FOUND;
    config = *new_config; in_callback = 1; err = config.start(config.context); in_callback = 0;
    if (err != BK_OK) return err;
    if (bk_net_get_state() == BK_NET_STATE_PROVISIONING) { started = 1; return BK_OK; }
    err = bk_net_reprovision(); if (err == BK_OK) started = 1; return err;
}

bk_err_t bk_provision_stop(void) {
    bk_err_t err = BK_OK;
    if (in_callback) return BK_ERR_REENTRANT;
    if (!started) return BK_ERR_NOT_STARTED;
    if (config.stop) { in_callback = 1; err = config.stop(config.context); in_callback = 0; }
    if (err == BK_OK) { started = 0; (void)bk_net_notify_disconnected(); }
    return err;
}

void bk_provision_reset_for_net_stop(void) {
    if (started && config.stop && !in_callback) { in_callback = 1; (void)config.stop(config.context); in_callback = 0; }
    started = 0;
}
void bk_provision_reset_for_net_connected(void) { started = 0; }
