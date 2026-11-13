#include "bk_lan.h"
#include "bk_net_internal.h"

#include <stddef.h>

#if (defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)) && defined(CONFIG_BK_ROBOT_LAN_NET)
extern bk_err_t robot_lan_net_init(void);
extern bk_err_t robot_lan_net_start_discovery(void);
extern bk_err_t robot_lan_net_stop_all(void);

static int s_lan_hw_inited;

static bk_err_t armino_lan_publish(void *context, const bk_device_record_t *record)
{
    bk_err_t err;
    (void)context;
    (void)record;
    if (!s_lan_hw_inited) {
        err = robot_lan_net_init();
        if (err != BK_OK) {
            return err;
        }
        s_lan_hw_inited = 1;
    }
    return robot_lan_net_start_discovery();
}

static bk_err_t armino_lan_stop(void *context)
{
    (void)context;
    return robot_lan_net_stop_all();
}

static const bk_lan_config_t g_armino_lan_config = {
    .context = NULL,
    .publish = armino_lan_publish,
    .stop = armino_lan_stop,
};
#endif

const bk_lan_config_t *bk_lan_default_config(void)
{
#if (defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)) && defined(CONFIG_BK_ROBOT_LAN_NET)
    return &g_armino_lan_config;
#else
    return NULL;
#endif
}

static bk_lan_config_t config;
static int started;
bk_err_t bk_lan_start(const bk_lan_config_t *new_config) {
    if (bk_net_callback_active()) return BK_ERR_REENTRANT;
    if (!new_config) return BK_ERR_INVALID_ARG;
    if (bk_net_get_state() != BK_NET_STATE_CONNECTED) return BK_ERR_NOT_STARTED;
    if (started) return BK_ERR_ALREADY_STARTED;
    config = *new_config; started = 1; return BK_OK;
}

bk_err_t bk_lan_stop(void) {
    bk_err_t err = BK_OK;
    if (bk_net_callback_active()) return BK_ERR_REENTRANT;
    if (!started) return BK_ERR_NOT_STARTED;
    if (config.stop) { bk_net_callback_enter(); err = config.stop(config.context); bk_net_callback_leave(); }
    if (err == BK_OK) started = 0;
    return err;
}

bk_err_t bk_lan_publish(const bk_device_record_t *record) {
    bk_err_t err;
    if (bk_net_callback_active()) return BK_ERR_REENTRANT;
    if (!record || !record->id || !record->name) return BK_ERR_INVALID_ARG;
    if (!started) return BK_ERR_NOT_STARTED;
    if (bk_net_get_state() != BK_NET_STATE_CONNECTED) return BK_ERR_BUSY;
    if (!config.publish) return BK_ERR_NOT_FOUND;
    bk_net_callback_enter(); err = config.publish(config.context, record); bk_net_callback_leave(); return err;
}

void bk_lan_reset_for_net_stop(void) {
    if (started && config.stop && !bk_net_callback_active()) { bk_net_callback_enter(); (void)config.stop(config.context); bk_net_callback_leave(); }
    started = 0;
}
