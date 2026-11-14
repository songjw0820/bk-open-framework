/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_PLUGIN_AGORA_H
#define BK_OPEN_FRAMEWORK_BK_PLUGIN_AGORA_H

#include <stdbool.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Commercial Plugin Boundary Stub (Proprietary Agora RTC SDK).
 *
 * NOTE: This header/module defines the compile-time isolation boundary only.
 * The open-source repository does NOT bundle or link libagora-rtc-sdk.a.
 * Unless an external commercial Agora SDK backend is linked and verified
 * (CONFIG_BK_PLUGIN_AGORA_SDK_LINKED == 1), bk_plugin_agora_start() fails
 * closed with BK_ERR_NOT_VERIFIED / BK_ERR_NOT_SUPPORTED.
 */
typedef struct {
    const char *app_id;
    const char *channel_name;
    const char *token;
} bk_plugin_agora_config_t;

bool bk_plugin_agora_is_compiled_in(void);
bool bk_plugin_agora_is_sdk_linked(void);
bk_err_t bk_plugin_agora_start(const bk_plugin_agora_config_t *cfg);
bk_err_t bk_plugin_agora_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_PLUGIN_AGORA_H */
