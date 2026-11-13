#include "bk_provision.h"
#include "bk_net_internal.h"

#include <stddef.h>

static bk_err_t host_mock_provision_action(void *context)
{
    (void)context;
    return BK_OK;
}

static const bk_provision_config_t g_host_mock_provision_config = {
    .context = NULL,
    .start = host_mock_provision_action,
    .stop = host_mock_provision_action,
};

#if (defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)) && defined(CONFIG_BK_SMART_CONFIG)
extern int bk_sconf_init(void);
extern void bk_sconf_prepare_for_smart_config(void);

static bk_err_t armino_provision_start(void *context)
{
    (void)context;
    bk_sconf_prepare_for_smart_config();
    return (bk_err_t)bk_sconf_init();
}

static bk_err_t armino_provision_stop(void *context)
{
    (void)context;
    return BK_OK;
}

static const bk_provision_config_t g_armino_provision_config = {
    .context = NULL,
    .start = armino_provision_start,
    .stop = armino_provision_stop,
};
#elif defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
#include <modules/wifi.h>

static bk_err_t armino_sdk_provision_start(void *context)
{
    (void)context;
    return bk_wifi_sta_start();
}

static bk_err_t armino_sdk_provision_stop(void *context)
{
    (void)context;
    return BK_OK;
}

static const bk_provision_config_t g_armino_sdk_provision_config = {
    .context = NULL,
    .start = armino_sdk_provision_start,
    .stop = armino_sdk_provision_stop,
};
#endif

const bk_provision_config_t *bk_provision_default_config(void)
{
#if (defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)) && defined(CONFIG_BK_SMART_CONFIG)
    return &g_armino_provision_config;
#elif defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
    return &g_armino_sdk_provision_config;
#else
    return NULL;
#endif
}

const bk_provision_config_t *bk_provision_host_mock_config(void)
{
    return &g_host_mock_provision_config;
}

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
