/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_UI_CORE_H
#define BK_OPEN_FRAMEWORK_BK_UI_CORE_H

#include <stddef.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bk_ui_core *bk_ui_core_handle_t;

bk_err_t bk_ui_core_create(bk_ui_core_handle_t *out_ui);
void bk_ui_core_destroy(bk_ui_core_handle_t ui);

/* Recursive mutex lock/unlock around LVGL calls */
bk_err_t bk_ui_lock(bk_ui_core_handle_t ui, uint32_t timeout_ms);
bk_err_t bk_ui_unlock(bk_ui_core_handle_t ui);
uint32_t bk_ui_get_lock_depth(bk_ui_core_handle_t ui);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_UI_CORE_H */
