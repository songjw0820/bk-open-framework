#include "bk_net.h"
#include "bk_net_internal.h"

#include <stddef.h>
#include <string.h>

#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
#include <components/event.h>
#include <components/netif.h>
#include <modules/wifi.h>
#include <os/os.h>
static beken_mutex_t s_net_lock;
#endif

static bk_net_state_t state = BK_NET_STATE_INITIAL;
static bk_net_config_t config;
static unsigned retries;
static int configured;
static int in_callback;
static char s_ssid[BK_NET_SSID_MAX_LEN + 1U];
static char s_password[BK_NET_PASSWORD_MAX_LEN + 1U];

void bk_net_lock(void)
{
#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
    if (s_net_lock == NULL) {
        (void)rtos_init_mutex(&s_net_lock);
    }
    if (s_net_lock != NULL) {
        (void)rtos_lock_mutex(&s_net_lock);
    }
#endif
}

void bk_net_unlock(void)
{
#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
    if (s_net_lock != NULL) {
        (void)rtos_unlock_mutex(&s_net_lock);
    }
#endif
}

static bk_err_t host_mock_net_action(void *context)
{
    (void)context;
    return BK_OK;
}

static const bk_net_config_t g_host_mock_net_config = {
    .context = NULL,
    .start = host_mock_net_action,
    .connect = host_mock_net_action,
    .disconnect = host_mock_net_action,
    .max_retries = 3U,
    .async_connect = false,
};

#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
static int s_armino_events_registered;

static bk_err_t armino_netif_event_cb(void *arg, event_module_t event_module,
                                      int event_id, void *event_data)
{
    (void)arg;
    (void)event_module;
    (void)event_data;
    if (event_id == EVENT_NETIF_GOT_IP4) {
        (void)bk_net_notify_connected();
    }
    return BK_OK;
}

static bk_err_t armino_wifi_event_cb(void *arg, event_module_t event_module,
                                     int event_id, void *event_data)
{
    (void)arg;
    (void)event_module;
    (void)event_data;
    if (event_id == EVENT_WIFI_STA_DISCONNECTED) {
        (void)bk_net_notify_disconnected();
    }
    return BK_OK;
}

static bk_err_t armino_net_start(void *context)
{
    bk_err_t err;
    (void)context;
    if (!s_armino_events_registered) {
        err = bk_event_register_cb(EVENT_MOD_NETIF, EVENT_NETIF_GOT_IP4,
                                   armino_netif_event_cb, NULL);
        if (err != BK_OK && err != BK_ERR_EVENT_CB_EXIST) {
            return err;
        }
        err = bk_event_register_cb(EVENT_MOD_WIFI, EVENT_WIFI_STA_DISCONNECTED,
                                   armino_wifi_event_cb, NULL);
        if (err != BK_OK && err != BK_ERR_EVENT_CB_EXIST) {
            return err;
        }
        s_armino_events_registered = 1;
    }
    return bk_wifi_sta_start();
}

static bk_err_t armino_net_connect(void *context)
{
    (void)context;
    return bk_wifi_sta_connect();
}

static bk_err_t armino_net_disconnect(void *context)
{
    (void)context;
    return bk_wifi_sta_disconnect();
}

static const bk_net_config_t g_armino_net_config = {
    .context = NULL,
    .start = armino_net_start,
    .connect = armino_net_connect,
    .disconnect = armino_net_disconnect,
    .max_retries = 3U,
    .async_connect = true,
};
#endif

const bk_net_config_t *bk_net_default_config(void)
{
#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
    return &g_armino_net_config;
#else
    return NULL;
#endif
}

const bk_net_config_t *bk_net_host_mock_config(void)
{
    return &g_host_mock_net_config;
}

bk_err_t bk_net_set_credentials(const char *ssid, const char *password)
{
    size_t ssid_len;
    size_t pwd_len = 0U;
    if (ssid == NULL || ssid[0] == '\0') {
        return BK_ERR_INVALID_ARG;
    }
    ssid_len = strlen(ssid);
    if (ssid_len > BK_NET_SSID_MAX_LEN) {
        return BK_ERR_INVALID_ARG;
    }
    if (password != NULL) {
        pwd_len = strlen(password);
        if (pwd_len > BK_NET_PASSWORD_MAX_LEN) {
            return BK_ERR_INVALID_ARG;
        }
    }
    memcpy(s_ssid, ssid, ssid_len + 1U);
    if (password != NULL) {
        memcpy(s_password, password, pwd_len + 1U);
    } else {
        s_password[0] = '\0';
    }
#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
    {
        wifi_sta_config_t sta_cfg;
        memset(&sta_cfg, 0, sizeof(sta_cfg));
        memcpy(sta_cfg.ssid, s_ssid, ssid_len + 1U);
        if (pwd_len > 0U) {
            memcpy(sta_cfg.password, s_password, pwd_len + 1U);
        }
        return bk_wifi_sta_set_config(&sta_cfg);
    }
#else
    return BK_OK;
#endif
}

const char *bk_net_get_ssid(void)
{
    return s_ssid[0] != '\0' ? s_ssid : NULL;
}

bk_err_t bk_net_get_ip_info(bk_net_ip_info_t *info)
{
    if (info == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (state != BK_NET_STATE_CONNECTED) {
        return BK_ERR_NOT_STARTED;
    }
    memset(info, 0, sizeof(*info));
#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
    {
        netif_ip4_config_t ip4_cfg;
        bk_err_t err;
        memset(&ip4_cfg, 0, sizeof(ip4_cfg));
        err = bk_netif_get_ip4_config(NETIF_IF_STA, &ip4_cfg);
        if (err != BK_OK) {
            return err;
        }
        memcpy(info->ip, ip4_cfg.ip, sizeof(info->ip) - 1U);
        memcpy(info->mask, ip4_cfg.mask, sizeof(info->mask) - 1U);
        memcpy(info->gateway, ip4_cfg.gateway, sizeof(info->gateway) - 1U);
        memcpy(info->dns, ip4_cfg.dns, sizeof(info->dns) - 1U);
        return BK_OK;
    }
#else
    memcpy(info->ip, "192.168.4.100", sizeof("192.168.4.100"));
    memcpy(info->mask, "255.255.255.0", sizeof("255.255.255.0"));
    memcpy(info->gateway, "192.168.4.1", sizeof("192.168.4.1"));
    memcpy(info->dns, "8.8.8.8", sizeof("8.8.8.8"));
    return BK_OK;
#endif
}

bk_err_t bk_net_callback_enter(void) { if (in_callback) return BK_ERR_REENTRANT; in_callback = 1; return BK_OK; }
void bk_net_callback_leave(void) { in_callback = 0; }
int bk_net_callback_active(void) { return in_callback; }

static int active(void) { return state != BK_NET_STATE_INITIAL && state != BK_NET_STATE_STOPPED; }

bk_err_t bk_net_start(const bk_net_config_t *new_config) {
    bk_err_t err;
    bk_net_action_fn start_fn;
    bk_net_action_fn connect_fn;
    void *ctx;
    bool async_mode;

    if (!new_config) return BK_ERR_INVALID_ARG;
    bk_net_lock();
    if (in_callback) { bk_net_unlock(); return BK_ERR_REENTRANT; }
    if (active()) { bk_net_unlock(); return BK_ERR_ALREADY_STARTED; }
    if (!new_config->connect) { bk_net_unlock(); return BK_ERR_NOT_FOUND; }
    config = *new_config; configured = 1; retries = 0; state = BK_NET_STATE_STARTING;
    start_fn = config.start;
    connect_fn = config.connect;
    ctx = config.context;
    async_mode = config.async_connect;
    bk_net_unlock();

    if (start_fn) {
        bk_net_callback_enter(); err = start_fn(ctx); bk_net_callback_leave();
        if (err != BK_OK) {
            bk_net_lock(); state = BK_NET_STATE_DISCONNECTED; bk_net_unlock();
            return err;
        }
    }
    bk_net_lock();
    state = BK_NET_STATE_CONNECTING;
    bk_net_unlock();

    bk_net_callback_enter(); err = connect_fn(ctx); bk_net_callback_leave();
    bk_net_lock();
    if (err == BK_OK) {
        if (!async_mode) {
            state = BK_NET_STATE_CONNECTED;
            retries = 0;
        }
    } else {
        state = BK_NET_STATE_RETRYING;
    }
    bk_net_unlock();
    return err;
}

bk_net_state_t bk_net_get_state(void) {
    bk_net_state_t current;
    bk_net_lock();
    current = state;
    bk_net_unlock();
    return current;
}

bk_err_t bk_net_notify_connected(void) {
    bk_net_lock();
    if (in_callback) { bk_net_unlock(); return BK_ERR_REENTRANT; }
    if (!active()) { bk_net_unlock(); return BK_ERR_NOT_STARTED; }
    if (state != BK_NET_STATE_CONNECTING && state != BK_NET_STATE_RETRYING && state != BK_NET_STATE_PROVISIONING) {
        bk_net_unlock();
        return BK_ERR_BUSY;
    }
    if (bk_provision_reset_for_net_connected() != BK_OK) {
        bk_net_unlock();
        return BK_ERR_BUSY;
    }
    state = BK_NET_STATE_CONNECTED; retries = 0;
    bk_net_unlock();
    return BK_OK;
}

bk_err_t bk_net_notify_disconnected(void) {
    bk_net_lock();
    if (in_callback) { bk_net_unlock(); return BK_ERR_REENTRANT; }
    if (!active()) { bk_net_unlock(); return BK_ERR_NOT_STARTED; }
    if (state != BK_NET_STATE_CONNECTED && state != BK_NET_STATE_CONNECTING && state != BK_NET_STATE_RETRYING && state != BK_NET_STATE_PROVISIONING) {
        bk_err_t res = state == BK_NET_STATE_DISCONNECTED ? BK_OK : BK_ERR_BUSY;
        bk_net_unlock();
        return res;
    }
    bk_lan_reset_for_net_stop(); bk_provision_reset_for_net_stop(); state = BK_NET_STATE_DISCONNECTED;
    bk_net_unlock();
    return BK_OK;
}

bk_err_t bk_net_disconnect(void) {
    bk_err_t err;
    bk_net_action_fn disconnect_fn;
    void *ctx;

    bk_net_lock();
    if (in_callback) { bk_net_unlock(); return BK_ERR_REENTRANT; }
    if (!active()) { bk_net_unlock(); return BK_ERR_NOT_STARTED; }
    if (state == BK_NET_STATE_DISCONNECTED) { bk_net_unlock(); return BK_OK; }
    disconnect_fn = config.disconnect;
    ctx = config.context;
    bk_net_unlock();

    if (disconnect_fn) {
        bk_net_callback_enter(); err = disconnect_fn(ctx); bk_net_callback_leave();
        if (err != BK_OK) return err;
    }
    return bk_net_notify_disconnected();
}

bk_err_t bk_net_retry(void) {
    bk_err_t err; unsigned limit;
    bk_net_action_fn connect_fn;
    void *ctx;
    bool async_mode;

    bk_net_lock();
    if (in_callback) { bk_net_unlock(); return BK_ERR_REENTRANT; }
    if (!active()) { bk_net_unlock(); return BK_ERR_NOT_STARTED; }
    if (state != BK_NET_STATE_DISCONNECTED && state != BK_NET_STATE_RETRYING) { bk_net_unlock(); return BK_ERR_BUSY; }
    limit = config.max_retries ? config.max_retries : 3U;
    if (retries >= limit) { bk_net_unlock(); return BK_ERR_LIMIT; }
    retries++; state = BK_NET_STATE_RETRYING;
    connect_fn = config.connect;
    ctx = config.context;
    async_mode = config.async_connect;
    bk_net_unlock();

    bk_net_callback_enter(); err = connect_fn(ctx); bk_net_callback_leave();
    bk_net_lock();
    if (err == BK_OK && !async_mode) { state = BK_NET_STATE_CONNECTED; retries = 0; }
    bk_net_unlock();
    return err;
}

bk_err_t bk_net_reprovision(void) {
    bk_net_lock();
    if (in_callback) { bk_net_unlock(); return BK_ERR_REENTRANT; }
    if (!active()) { bk_net_unlock(); return BK_ERR_NOT_STARTED; }
    if (state != BK_NET_STATE_CONNECTED && state != BK_NET_STATE_DISCONNECTED) { bk_net_unlock(); return BK_ERR_BUSY; }
    bk_lan_reset_for_net_stop(); state = BK_NET_STATE_PROVISIONING;
    bk_net_unlock();
    return BK_OK;
}

bk_err_t bk_net_stop(void) {
    bk_err_t err;
    bk_net_action_fn disconnect_fn;
    void *ctx;

    bk_net_lock();
    if (in_callback) { bk_net_unlock(); return BK_ERR_REENTRANT; }
    if (!active()) { bk_net_unlock(); return BK_ERR_NOT_STARTED; }
    disconnect_fn = config.disconnect;
    ctx = config.context;
    bk_net_unlock();

    if (disconnect_fn) {
        bk_net_callback_enter(); err = disconnect_fn(ctx); bk_net_callback_leave();
        if (err != BK_OK) return err;
    }
    bk_net_lock();
    bk_lan_reset_for_net_stop(); bk_provision_reset_for_net_stop(); state = BK_NET_STATE_STOPPED;
    bk_net_unlock();
    return BK_OK;
}

bk_err_t bk_net_restart(void) {
    bk_net_config_t saved; bk_err_t err;
    bk_net_lock();
    if (in_callback) { bk_net_unlock(); return BK_ERR_REENTRANT; }
    if (!configured) { bk_net_unlock(); return BK_ERR_NOT_STARTED; }
    saved = config;
    bk_net_unlock();
    if (active()) { err = bk_net_stop(); if (err != BK_OK) return err; }
    return bk_net_start(&saved);
}
