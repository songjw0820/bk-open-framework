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

bool bk_plugin_agora_is_sdk_linked(void)
{
#if defined(CONFIG_BK_PLUGIN_AGORA_ENABLE) && (CONFIG_BK_PLUGIN_AGORA_ENABLE == 1) && \
    defined(CONFIG_BK_PLUGIN_AGORA_SDK_LINKED) && (CONFIG_BK_PLUGIN_AGORA_SDK_LINKED == 1)
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
    /*
     * Fail closed: never return BK_OK unless a real external Agora RTC SDK
     * backend is linked and verified on target.
     */
    return BK_ERR_NOT_VERIFIED;
}

bk_err_t bk_plugin_agora_stop(void)
{
    if (!bk_plugin_agora_is_compiled_in()) {
        return BK_ERR_NOT_SUPPORTED;
    }
    return BK_ERR_NOT_VERIFIED;
}
