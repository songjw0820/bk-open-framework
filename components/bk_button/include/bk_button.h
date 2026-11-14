/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_BUTTON_H
#define BK_OPEN_FRAMEWORK_BK_BUTTON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BK_BUTTON_EVT_NONE = 0,
    BK_BUTTON_EVT_SINGLE_CLICK,
    BK_BUTTON_EVT_DOUBLE_CLICK,
    BK_BUTTON_EVT_LONG_PRESS,
} bk_button_event_t;

typedef void (*bk_button_cb_t)(uint32_t button_id, bk_button_event_t evt, void *user_ctx);

typedef struct {
    uint32_t button_id;
    uint32_t debounce_ms;
    uint32_t double_click_window_ms;
    uint32_t long_press_ms;
    bk_button_cb_t cb;
    void *user_ctx;
} bk_button_config_t;

typedef struct bk_button *bk_button_handle_t;

bk_err_t bk_button_create(const bk_button_config_t *cfg, bk_button_handle_t *out_btn);
void bk_button_destroy(bk_button_handle_t btn);

bk_err_t bk_button_feed_sample(bk_button_handle_t btn,
                               bool raw_pressed,
                               uint64_t now_ms,
                               bk_button_event_t *out_evt);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_BUTTON_H */
