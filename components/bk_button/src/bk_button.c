/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_button.h"
#include "bk_port_mem.h"

struct bk_button {
    bk_button_config_t cfg;
    bool raw_last;
    uint64_t raw_change_ms;
    bool debounced_pressed;
    bool adc_in_band;
    uint64_t press_start_ms;
    uint64_t last_hold_ms;
    bool long_fired;
    bool waiting_second_click;
    uint64_t first_release_ms;
    uint8_t click_streak;
    uint8_t last_repeat_count;
};

static void emit_btn_evt(struct bk_button *btn,
                         bk_button_event_t evt,
                         bk_button_event_t *out_evt)
{
    if (out_evt != NULL) {
        *out_evt = evt;
    }
    if (evt != BK_BUTTON_EVT_NONE && btn->cfg.cb != NULL) {
        btn->cfg.cb(btn->cfg.button_id, evt, btn->cfg.user_ctx);
    }
}

bk_err_t bk_button_create(const bk_button_config_t *cfg, bk_button_handle_t *out_btn)
{
    if (cfg == NULL || out_btn == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    struct bk_button *btn = (struct bk_button *)bk_port_calloc(
        1, sizeof(*btn), BK_MEM_CAP_DEFAULT, "bk_button");
    if (btn == NULL) {
        return BK_ERR_NO_MEM;
    }
    btn->cfg = *cfg;
    if (btn->cfg.debounce_ms == 0U) {
        btn->cfg.debounce_ms = 20U;
    }
    if (btn->cfg.double_click_window_ms == 0U) {
        btn->cfg.double_click_window_ms = 250U;
    }
    if (btn->cfg.long_press_ms == 0U) {
        btn->cfg.long_press_ms = 1000U;
    }
    if (btn->cfg.adc_tolerance_mv == 0U) {
        btn->cfg.adc_tolerance_mv = 80U;
    }
    *out_btn = btn;
    return BK_OK;
}

void bk_button_destroy(bk_button_handle_t btn)
{
    if (btn != NULL) {
        bk_port_free(btn);
    }
}

bk_err_t bk_button_feed_sample(bk_button_handle_t btn,
                               bool raw_pressed,
                               uint64_t now_ms,
                               bk_button_event_t *out_evt)
{
    if (btn == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (out_evt != NULL) {
        *out_evt = BK_BUTTON_EVT_NONE;
    }

    if (raw_pressed != btn->raw_last) {
        btn->raw_last = raw_pressed;
        btn->raw_change_ms = now_ms;
    }

    bool prev_debounced = btn->debounced_pressed;
    if ((now_ms - btn->raw_change_ms) >= btn->cfg.debounce_ms) {
        btn->debounced_pressed = btn->raw_last;
    }

    /* Rising edge (button pressed) */
    if (!prev_debounced && btn->debounced_pressed) {
        btn->press_start_ms = now_ms;
        btn->last_hold_ms = now_ms;
        btn->long_fired = false;
        return BK_OK;
    }

    /* Held down -> check long press and periodic hold */
    if (btn->debounced_pressed) {
        if (!btn->long_fired && (now_ms - btn->press_start_ms) >= btn->cfg.long_press_ms) {
            btn->long_fired = true;
            btn->last_hold_ms = now_ms;
            btn->waiting_second_click = false;
            btn->click_streak = 0U;
            emit_btn_evt(btn, BK_BUTTON_EVT_LONG_PRESS, out_evt);
            return BK_OK;
        }
        if (btn->long_fired && btn->cfg.long_press_hold_interval_ms > 0U &&
            (now_ms - btn->last_hold_ms) >= btn->cfg.long_press_hold_interval_ms) {
            btn->last_hold_ms = now_ms;
            emit_btn_evt(btn, BK_BUTTON_EVT_LONG_PRESS_HOLD, out_evt);
            return BK_OK;
        }
    }

    /* Falling edge (button released) */
    if (prev_debounced && !btn->debounced_pressed) {
        if (btn->long_fired) {
            btn->long_fired = false;
            return BK_OK;
        }
        if (btn->waiting_second_click &&
            (now_ms - btn->first_release_ms) <= btn->cfg.double_click_window_ms) {
            btn->waiting_second_click = false;
            btn->click_streak += 1U;
            btn->last_repeat_count = btn->click_streak;
            btn->click_streak = 0U;
            emit_btn_evt(btn, BK_BUTTON_EVT_DOUBLE_CLICK, out_evt);
            return BK_OK;
        }
        btn->waiting_second_click = true;
        btn->click_streak = 1U;
        btn->first_release_ms = now_ms;
        return BK_OK;
    }

    /* Idle & double-click window elapsed -> emit single click */
    if (!btn->debounced_pressed && btn->waiting_second_click) {
        if ((now_ms - btn->first_release_ms) > btn->cfg.double_click_window_ms) {
            btn->waiting_second_click = false;
            btn->last_repeat_count = 1U;
            btn->click_streak = 0U;
            emit_btn_evt(btn, BK_BUTTON_EVT_SINGLE_CLICK, out_evt);
            return BK_OK;
        }
    }

    return BK_OK;
}

bk_err_t bk_button_feed_adc_mv(bk_button_handle_t btn,
                               uint32_t sample_mv,
                               uint64_t now_ms,
                               bk_button_event_t *out_evt)
{
    if (btn == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    uint32_t target = btn->cfg.adc_target_mv;
    uint32_t tol = btn->cfg.adc_tolerance_mv;
    /* Schmitt trigger: widen release tolerance by 25mV to prevent chatter */
    uint32_t active_tol = btn->adc_in_band ? (tol + 25U) : tol;
    uint32_t diff = (sample_mv >= target) ? (sample_mv - target) : (target - sample_mv);
    btn->adc_in_band = (diff <= active_tol);
    return bk_button_feed_sample(btn, btn->adc_in_band, now_ms, out_evt);
}

uint8_t bk_button_get_repeat_count(bk_button_handle_t btn)
{
    return btn != NULL ? btn->last_repeat_count : 0U;
}

bool bk_button_is_pressed(bk_button_handle_t btn)
{
    return btn != NULL && btn->debounced_pressed;
}
