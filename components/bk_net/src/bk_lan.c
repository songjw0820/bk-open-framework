#include "bk_lan.h"

static bk_lan_config_t config;
static int started;

bk_err_t bk_lan_start(const bk_lan_config_t *new_config) {
    if (!new_config) return BK_ERR_INVALID_ARG;
    if (bk_net_get_state() != BK_NET_STATE_CONNECTED) return BK_ERR_NOT_STARTED;
    if (started) return BK_ERR_ALREADY_STARTED;
    config = *new_config;
    started = 1;
    return BK_OK;
}

bk_err_t bk_lan_stop(void) {
    if (!started) return BK_ERR_NOT_STARTED;
    started = 0;
    return BK_OK;
}

bk_err_t bk_lan_publish(const bk_device_record_t *record) {
    if (!record || !record->id || !record->name) return BK_ERR_INVALID_ARG;
    if (!started) return BK_ERR_NOT_STARTED;
    if (bk_net_get_state() != BK_NET_STATE_CONNECTED) return BK_ERR_BUSY;
    if (config.publish) return config.publish(config.context, record);
    return BK_OK;
}

void bk_lan_reset_for_net_stop(void) { started = 0; }
