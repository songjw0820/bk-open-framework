#include "bk_provision.h"
#include "bk_net_internal.h"

static bk_provision_config_t config;
static int started;
bk_err_t bk_provision_start(const bk_provision_config_t *new_config) {
    bk_err_t err;
    if (bk_net_callback_active()) return BK_ERR_REENTRANT;
    if (!new_config) return BK_ERR_INVALID_ARG;
    if (bk_net_get_state() == BK_NET_STATE_INITIAL || bk_net_get_state() == BK_NET_STATE_STOPPED) return BK_ERR_NOT_STARTED;
    if (started) return BK_ERR_ALREADY_STARTED;
    if (!new_config->start) return BK_ERR_NOT_FOUND;
    config = *new_config; bk_net_callback_enter(); err = config.start(config.context); bk_net_callback_leave();
    if (err != BK_OK) return err;
    if (bk_net_get_state() == BK_NET_STATE_PROVISIONING) { started = 1; return BK_OK; }
    err = bk_net_reprovision(); if (err == BK_OK) started = 1; return err;
}

bk_err_t bk_provision_stop(void) {
    bk_err_t err = BK_OK;
    if (bk_net_callback_active()) return BK_ERR_REENTRANT;
    if (!started) return BK_ERR_NOT_STARTED;
    if (config.stop) { bk_net_callback_enter(); err = config.stop(config.context); bk_net_callback_leave(); }
    if (err == BK_OK) { started = 0; (void)bk_net_notify_disconnected(); }
    return err;
}

void bk_provision_reset_for_net_stop(void) {
    if (started && config.stop && !bk_net_callback_active()) { bk_net_callback_enter(); (void)config.stop(config.context); bk_net_callback_leave(); }
    started = 0;
}
bk_err_t bk_provision_reset_for_net_connected(void) {
    bk_err_t err = BK_OK;
    if (started && config.stop) { bk_net_callback_enter(); err = config.stop(config.context); bk_net_callback_leave(); }
    if (err == BK_OK) started = 0;
    return err;
}
