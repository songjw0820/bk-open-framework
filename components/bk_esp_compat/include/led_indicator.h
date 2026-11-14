/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Espressif Component Registry 'espressif/led_indicator' API adapter
 * backed directly by bk_led_effect (priority preemption & PWM breathing/blink engine).
 */
#ifndef BK_OPEN_FRAMEWORK_LED_INDICATOR_H
#define BK_OPEN_FRAMEWORK_LED_INDICATOR_H

#include <stdint.h>
#include "bk_led_effect.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bk_led_handle_t led_indicator_handle_t;

typedef struct {
    bk_led_priority_t priority;
    bk_led_pattern_t pattern;
} led_indicator_blink_type_t;

typedef struct {
    int32_t gpio_num;
} led_indicator_config_t;

led_indicator_handle_t led_indicator_create(const led_indicator_config_t *config);
esp_err_t led_indicator_delete(led_indicator_handle_t handle);
esp_err_t led_indicator_start(led_indicator_handle_t handle,
                              bk_led_priority_t prio,
                              bk_led_pattern_t pattern);
esp_err_t led_indicator_stop(led_indicator_handle_t handle, bk_led_priority_t prio);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_LED_INDICATOR_H */
