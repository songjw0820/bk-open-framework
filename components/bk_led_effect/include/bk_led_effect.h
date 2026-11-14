/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_LED_EFFECT_H
#define BK_OPEN_FRAMEWORK_BK_LED_EFFECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BK_LED_PRIO_LOW      = 0,
    BK_LED_PRIO_NORMAL   = 1,
    BK_LED_PRIO_HIGH     = 2,
    BK_LED_PRIO_CRITICAL = 3,
    BK_LED_PRIO_MAX      = 4,
} bk_led_priority_t;

typedef enum {
    BK_LED_PATTERN_OFF = 0,
    BK_LED_PATTERN_SOLID,
    BK_LED_PATTERN_SLOW_BLINK,
    BK_LED_PATTERN_FAST_BLINK,
    BK_LED_PATTERN_BREATHE,
} bk_led_pattern_t;

typedef struct bk_led_ctrl *bk_led_handle_t;

bk_err_t bk_led_create(bk_led_handle_t *out_led);
void bk_led_destroy(bk_led_handle_t led);

bk_err_t bk_led_set_effect(bk_led_handle_t led,
                           bk_led_priority_t prio,
                           bk_led_pattern_t pattern);

/*
 * Sets a finite-duration LED effect at the given priority.
 * When duration_ms elapses during bk_led_tick_ms(), this priority slot
 * automatically clears and restores the highest remaining lower-priority effect.
 */
bk_err_t bk_led_set_effect_timed(bk_led_handle_t led,
                                 bk_led_priority_t prio,
                                 bk_led_pattern_t pattern,
                                 uint64_t now_ms,
                                 uint32_t duration_ms);

bk_err_t bk_led_clear_effect(bk_led_handle_t led, bk_led_priority_t prio);

bk_led_pattern_t bk_led_get_active_pattern(bk_led_handle_t led);

/*
 * Advances the LED effect state machine at timestamp `now_ms` and outputs
 * instantaneous PWM brightness in permille (0..1000):
 * - OFF:        0
 * - SOLID:      1000
 * - SLOW_BLINK: 1000ms period (500ms ON = 1000, 500ms OFF = 0)
 * - FAST_BLINK: 250ms period  (125ms ON = 1000, 125ms OFF = 0)
 * - BREATHE:    2000ms triangle/gamma-shaped breathing wave (0..1000..0)
 */
bk_err_t bk_led_tick_ms(bk_led_handle_t led,
                        uint64_t now_ms,
                        uint16_t *out_brightness_permille);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_LED_EFFECT_H */
