#include "bk_core_internal.h"

bk_core_state_t g_bk_core;

bk_err_t bk_core_init(const bk_core_config_t *config)
{
    if (config == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (g_bk_core.initialized) {
        return BK_ERR_ALREADY_INITIALIZED;
    }
    if (config->max_subscribers > BK_CORE_MAX_SUBSCRIBERS) {
        return BK_ERR_LIMIT;
    }

    g_bk_core.initialized = 1;
    g_bk_core.started = 0;
    g_bk_core.publishing = 0;
    g_bk_core.max_subscribers = config->max_subscribers == 0U
        ? BK_CORE_MAX_SUBSCRIBERS : config->max_subscribers;
    g_bk_core.subscription_count = 0U;
    return BK_OK;
}

bk_err_t bk_core_start(void)
{
    if (!g_bk_core.initialized) {
        return BK_ERR_NOT_INITIALIZED;
    }
    if (g_bk_core.started) {
        return BK_ERR_ALREADY_STARTED;
    }
    g_bk_core.started = 1;
    return BK_OK;
}

bk_err_t bk_core_deinit(void)
{
    if (!g_bk_core.initialized) {
        return BK_ERR_NOT_INITIALIZED;
    }
    if (g_bk_core.publishing) {
        return BK_ERR_BUSY;
    }
    g_bk_core.initialized = 0;
    g_bk_core.started = 0;
    g_bk_core.max_subscribers = 0U;
    g_bk_core.subscription_count = 0U;
    return BK_OK;
}
