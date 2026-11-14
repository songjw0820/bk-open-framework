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
    BK_BUTTON_EVT_PRESS_DOWN,
    BK_BUTTON_EVT_PRESS_UP,
    BK_BUTTON_EVT_LONG_PRESS_HOLD,
    BK_BUTTON_EVT_MULTIPLE_CLICK,
} bk_button_event_t;

typedef void (*bk_button_cb_t)(uint32_t button_id, bk_button_event_t evt, void *user_ctx);

typedef struct {
    uint32_t button_id;
    uint32_t debounce_ms;
    uint32_t double_click_window_ms;
    uint32_t long_press_ms;
    uint32_t long_press_hold_interval_ms; /* Optional periodic hold interval (e.g. 200ms) */
    uint32_t adc_target_mv;               /* Optional ADC resistor-ladder center voltage (mV) */
    uint32_t adc_tolerance_mv;            /* Optional ADC tolerance +/- mV (default 80mV) */
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

/*
 * Feeds a raw ADC voltage reading (in millivolts) for ADC ladder keys.
 * Uses Schmitt-trigger hysteresis (+/- tolerance_mv on press, +25mV wider on release)
 * to prevent boundary oscillation, then runs the full debounce & click FSM.
 */
bk_err_t bk_button_feed_adc_mv(bk_button_handle_t btn,
                               uint32_t sample_mv,
                               uint64_t now_ms,
                               bk_button_event_t *out_evt);

uint8_t bk_button_get_repeat_count(bk_button_handle_t btn);
bool bk_button_is_pressed(bk_button_handle_t btn);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_BUTTON_H */
