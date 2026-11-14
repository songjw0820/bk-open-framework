/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Espressif Component Registry 'espressif/button' (iot_button) API adapter
 * backed directly by bk_button (GPIO & ADC ladder button state machine).
 */
#ifndef BK_OPEN_FRAMEWORK_IOT_BUTTON_H
#define BK_OPEN_FRAMEWORK_IOT_BUTTON_H

#include <stdbool.h>
#include <stdint.h>
#include "bk_button.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bk_iot_button_adapter *button_handle_t;
typedef void (*button_cb_t)(void *button_handle, void *usr_data);

typedef enum {
    BUTTON_PRESS_DOWN = 0,
    BUTTON_PRESS_UP,
    BUTTON_SINGLE_CLICK,
    BUTTON_DOUBLE_CLICK,
    BUTTON_LONG_PRESS_START,
    BUTTON_LONG_PRESS_HOLD,
    BUTTON_EVENT_MAX,
} button_event_t;

typedef enum {
    BUTTON_TYPE_GPIO = 0,
    BUTTON_TYPE_ADC,
} button_type_t;

typedef struct {
    int32_t gpio_num;
    uint8_t active_level;
} button_gpio_config_t;

typedef struct {
    uint8_t adc_channel;
    uint8_t button_index;
    uint16_t min; /* min mV */
    uint16_t max; /* max mV */
} button_adc_config_t;

typedef struct {
    button_type_t type;
    uint16_t long_press_time;
    uint16_t short_press_time;
    union {
        button_gpio_config_t gpio_button_config;
        button_adc_config_t adc_button_config;
    };
} button_config_t;

button_handle_t iot_button_create(const button_config_t *config);
esp_err_t iot_button_delete(button_handle_t btn_handle);
esp_err_t iot_button_register_cb(button_handle_t btn_handle,
                                 button_event_t event,
                                 button_cb_t cb,
                                 void *usr_data);
button_event_t iot_button_get_event(button_handle_t btn_handle);
uint8_t iot_button_get_repeat(button_handle_t btn_handle);

/* Feeds GPIO level or ADC mV sample into the adapted iot_button instance */
esp_err_t bk_iot_button_feed_gpio(button_handle_t btn_handle, bool level_active, uint64_t now_ms);
esp_err_t bk_iot_button_feed_adc_mv(button_handle_t btn_handle, uint32_t mv, uint64_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_IOT_BUTTON_H */
