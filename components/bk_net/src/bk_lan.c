#include "bk_lan.h"

static bk_lan_config_t config;
static int started;
static int in_callback;

bk_err_t bk_lan_start(const bk_lan_config_t *new_config) {
    if (in_callback) return BK_ERR_REENTRANT;
    if (!new_config) return BK_ERR_INVALID_ARG;
    if (bk_net_get_state() != BK_NET_STATE_CONNECTED) return BK_ERR_NOT_STARTED;
    if (started) return BK_ERR_ALREADY_STARTED;
    config = *new_config; started = 1; return BK_OK;
}

bk_err_t bk_lan_stop(void) {
    bk_err_t err = BK_OK;
    if (in_callback) return BK_ERR_REENTRANT;
    if (!started) return BK_ERR_NOT_STARTED;
    if (config.stop) { in_callback = 1; err = config.stop(config.context); in_callback = 0; }
    if (err == BK_OK) started = 0;
    return err;
}

bk_err_t bk_lan_publish(const bk_device_record_t *record) {
    bk_err_t err;
    if (in_callback) return BK_ERR_REENTRANT;
    if (!record || !record->id || !record->name) return BK_ERR_INVALID_ARG;
    if (!started) return BK_ERR_NOT_STARTED;
    if (bk_net_get_state() != BK_NET_STATE_CONNECTED) return BK_ERR_BUSY;
    if (!config.publish) return BK_ERR_NOT_FOUND;
    in_callback = 1; err = config.publish(config.context, record); in_callback = 0; return err;
}

void bk_lan_reset_for_net_stop(void) {
    if (started && config.stop && !in_callback) { in_callback = 1; (void)config.stop(config.context); in_callback = 0; }
    started = 0;
}
