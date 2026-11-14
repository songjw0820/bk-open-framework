/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_led_effect.h"
#include "bk_port_mem.h"

struct bk_led_ctrl {
    bool active[BK_LED_PRIO_MAX];
    bk_led_pattern_t patterns[BK_LED_PRIO_MAX];
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
    return BK_OK;
}

bk_err_t bk_led_clear_effect(bk_led_handle_t led, bk_led_priority_t prio)
{
    if (led == NULL || prio < 0 || prio >= BK_LED_PRIO_MAX) {
        return BK_ERR_INVALID_ARG;
    }
    led->active[prio] = false;
    led->patterns[prio] = BK_LED_PATTERN_OFF;
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
