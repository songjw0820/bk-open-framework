/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_PLUGIN_AGORA_H
#define BK_OPEN_FRAMEWORK_BK_PLUGIN_AGORA_H

#include <stdbool.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *app_id;
    const char *channel_name;
    const char *token;
} bk_plugin_agora_config_t;

bool bk_plugin_agora_is_compiled_in(void);
bk_err_t bk_plugin_agora_start(const bk_plugin_agora_config_t *cfg);
bk_err_t bk_plugin_agora_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_PLUGIN_AGORA_H */
