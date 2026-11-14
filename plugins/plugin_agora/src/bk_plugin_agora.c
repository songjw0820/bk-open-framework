/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_plugin_agora.h"

#include <stddef.h>

bool bk_plugin_agora_is_compiled_in(void)
{
#if defined(CONFIG_BK_PLUGIN_AGORA_ENABLE) && (CONFIG_BK_PLUGIN_AGORA_ENABLE == 1)
    return true;
#else
    return false;
#endif
}

bk_err_t bk_plugin_agora_start(const bk_plugin_agora_config_t *cfg)
{
    if (cfg == NULL || cfg->app_id == NULL || cfg->channel_name == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (!bk_plugin_agora_is_compiled_in()) {
        return BK_ERR_NOT_SUPPORTED;
    }
    return BK_OK;
}

bk_err_t bk_plugin_agora_stop(void)
{
    if (!bk_plugin_agora_is_compiled_in()) {
        return BK_ERR_NOT_SUPPORTED;
    }
    return BK_OK;
}
