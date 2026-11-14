/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_led_effect.h"
#include "bk_port_mem.h"

struct bk_led_ctrl {
    bool active[BK_LED_PRIO_MAX];
    bk_led_pattern_t patterns[BK_LED_PRIO_MAX];
    bool has_deadline[BK_LED_PRIO_MAX];
    uint64_t expire_ms[BK_LED_PRIO_MAX];
    uint64_t start_ms[BK_LED_PRIO_MAX];
};

bk_err_t bk_led_create(bk_led_handle_t *out_led)
{
    if (out_led == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    struct bk_led_ctrl *led = (struct bk_led_ctrl *)bk_port_calloc(
        1, sizeof(*led), BK_MEM_CAP_DEFAULT, "bk_led");
    if (led == NULL) {
        return BK_ERR_NO_MEM;
    }
    *out_led = led;
    return BK_OK;
}

void bk_led_destroy(bk_led_handle_t led)
{
    if (led != NULL) {
        bk_port_free(led);
    }
}

bk_err_t bk_led_set_effect(bk_led_handle_t led,
                           bk_led_priority_t prio,
                           bk_led_pattern_t pattern)
{
    if (led == NULL || prio < 0 || prio >= BK_LED_PRIO_MAX) {
        return BK_ERR_INVALID_ARG;
    }
    led->active[prio] = (pattern != BK_LED_PATTERN_OFF);
    led->patterns[prio] = pattern;
    led->has_deadline[prio] = false;
    led->expire_ms[prio] = 0U;
    led->start_ms[prio] = 0U;
    return BK_OK;
}

bk_err_t bk_led_set_effect_timed(bk_led_handle_t led,
                                 bk_led_priority_t prio,
                                 bk_led_pattern_t pattern,
                                 uint64_t now_ms,
                                 uint32_t duration_ms)
{
    if (led == NULL || prio < 0 || prio >= BK_LED_PRIO_MAX) {
        return BK_ERR_INVALID_ARG;
    }
    led->active[prio] = (pattern != BK_LED_PATTERN_OFF);
    led->patterns[prio] = pattern;
    led->start_ms[prio] = now_ms;
    if (duration_ms > 0U && pattern != BK_LED_PATTERN_OFF) {
        led->has_deadline[prio] = true;
        led->expire_ms[prio] = now_ms + duration_ms;
    } else {
        led->has_deadline[prio] = false;
        led->expire_ms[prio] = 0U;
    }
    return BK_OK;
}

bk_err_t bk_led_clear_effect(bk_led_handle_t led, bk_led_priority_t prio)
{
    if (led == NULL || prio < 0 || prio >= BK_LED_PRIO_MAX) {
        return BK_ERR_INVALID_ARG;
    }
    led->active[prio] = false;
    led->patterns[prio] = BK_LED_PATTERN_OFF;
    led->has_deadline[prio] = false;
    led->expire_ms[prio] = 0U;
    return BK_OK;
}

bk_led_pattern_t bk_led_get_active_pattern(bk_led_handle_t led)
{
    if (led == NULL) {
        return BK_LED_PATTERN_OFF;
    }
    for (int p = (int)BK_LED_PRIO_MAX - 1; p >= 0; --p) {
        if (led->active[p]) {
            return led->patterns[p];
        }
    }
    return BK_LED_PATTERN_OFF;
}

bk_err_t bk_led_tick_ms(bk_led_handle_t led,
                        uint64_t now_ms,
                        uint16_t *out_brightness_permille)
{
    if (led == NULL || out_brightness_permille == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    for (int p = 0; p < (int)BK_LED_PRIO_MAX; ++p) {
        if (led->active[p] && led->has_deadline[p] && now_ms >= led->expire_ms[p]) {
            led->active[p] = false;
            led->patterns[p] = BK_LED_PATTERN_OFF;
            led->has_deadline[p] = false;
        }
    }

    int active_prio = -1;
    for (int p = (int)BK_LED_PRIO_MAX - 1; p >= 0; --p) {
        if (led->active[p]) {
            active_prio = p;
            break;
        }
    }

    if (active_prio < 0) {
        *out_brightness_permille = 0U;
        return BK_OK;
    }

    uint64_t rel_ms = (now_ms >= led->start_ms[active_prio])
                          ? (now_ms - led->start_ms[active_prio])
                          : now_ms;
    bk_led_pattern_t pat = led->patterns[active_prio];
    switch (pat) {
    case BK_LED_PATTERN_SOLID:
        *out_brightness_permille = 1000U;
        break;
    case BK_LED_PATTERN_SLOW_BLINK:
        *out_brightness_permille = ((rel_ms % 1000ULL) < 500ULL) ? 1000U : 0U;
        break;
    case BK_LED_PATTERN_FAST_BLINK:
        *out_brightness_permille = ((rel_ms % 250ULL) < 125ULL) ? 1000U : 0U;
        break;
    case BK_LED_PATTERN_BREATHE: {
        uint64_t phase = rel_ms % 2000ULL;
        uint64_t linear = (phase < 1000ULL) ? phase : (2000ULL - phase);
        /* Quadratic gamma curve for natural visual breathing: (linear^2) / 1000 */
        *out_brightness_permille = (uint16_t)((linear * linear) / 1000ULL);
        break;
    }
    case BK_LED_PATTERN_OFF:
    default:
        *out_brightness_permille = 0U;
        break;
    }
    return BK_OK;
}
