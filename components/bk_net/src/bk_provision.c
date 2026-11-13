#include "bk_provision.h"

static bk_provision_config_t config;
static int started;

bk_err_t bk_provision_start(const bk_provision_config_t *new_config) {
    bk_err_t err;
    if (!new_config) return BK_ERR_INVALID_ARG;
    if (bk_net_get_state() == BK_NET_STATE_INITIAL || bk_net_get_state() == BK_NET_STATE_STOPPED)
        return BK_ERR_NOT_STARTED;
    if (started || bk_net_get_state() == BK_NET_STATE_PROVISIONING) return BK_ERR_ALREADY_STARTED;
    config = *new_config;
    if (config.start) {
        err = config.start(config.context);
        if (err != BK_OK) return err;
    }
    err = bk_net_reprovision();
    if (err == BK_OK) started = 1;
    return err;
}

bk_err_t bk_provision_stop(void) {
    bk_err_t err = BK_OK;
    if (!started) return BK_ERR_NOT_STARTED;
    if (config.stop) err = config.stop(config.context);
    if (err == BK_OK) { started = 0; (void)bk_net_notify_disconnected(); }
    return err;
}

void bk_provision_reset_for_net_stop(void) { started = 0; }
void bk_provision_reset_for_net_connected(void) { started = 0; }
