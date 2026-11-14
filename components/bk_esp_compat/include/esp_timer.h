/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_ESP_TIMER_H
#define BK_OPEN_FRAMEWORK_ESP_TIMER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*esp_timer_cb_t)(void *arg);
typedef struct bk_esp_timer *esp_timer_handle_t;

typedef enum {
    ESP_TIMER_TASK = 0,
} esp_timer_dispatch_t;

typedef struct {
    esp_timer_cb_t callback;
    void *arg;
    esp_timer_dispatch_t dispatch_method;
    const char *name;
    bool skip_unhandled_events;
} esp_timer_create_args_t;

/* Returns monotonic time in microseconds (backed by bk_port_get_time_ms() * 1000) */
int64_t esp_timer_get_time(void);

esp_err_t esp_timer_create(const esp_timer_create_args_t *create_args,
                           esp_timer_handle_t *out_handle);
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us);
esp_err_t esp_timer_stop(esp_timer_handle_t timer);
esp_err_t esp_timer_delete(esp_timer_handle_t timer);
bool esp_timer_is_active(esp_timer_handle_t timer);

/* Deterministic step helper for driving software timers at `now_us` */
void bk_esp_timer_step_us(esp_timer_handle_t timer, uint64_t now_us);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_ESP_TIMER_H */
