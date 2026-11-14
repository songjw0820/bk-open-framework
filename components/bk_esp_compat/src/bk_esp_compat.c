/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_core_event.h"
#include "bk_port_mem.h"
#include "bk_port_os.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "iot_button.h"
#include "led_indicator.h"
#include "nvs_flash.h"

#include <string.h>

/* --- 1. esp_timer implementation --- */

struct bk_esp_timer {
    esp_timer_cb_t cb;
    void *arg;
    bool active;
    bool periodic;
    uint64_t period_us;
    uint64_t next_deadline_us;
};

int64_t esp_timer_get_time(void)
{
    return (int64_t)(bk_port_get_time_ms() * 1000ULL);
}

esp_err_t esp_timer_create(const esp_timer_create_args_t *create_args,
                           esp_timer_handle_t *out_handle)
{
    if (create_args == NULL || create_args->callback == NULL || out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    struct bk_esp_timer *t = (struct bk_esp_timer *)bk_port_calloc(
        1, sizeof(*t), BK_MEM_CAP_DEFAULT, "esp_timer");
    if (t == NULL) {
        return ESP_ERR_NO_MEM;
    }
    t->cb = create_args->callback;
    t->arg = create_args->arg;
    *out_handle = t;
    return ESP_OK;
}

esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us)
{
    if (timer == NULL || timeout_us == 0ULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (timer->active) {
        return ESP_ERR_INVALID_STATE;
    }
    timer->active = true;
    timer->periodic = false;
    timer->period_us = timeout_us;
    timer->next_deadline_us = (uint64_t)esp_timer_get_time() + timeout_us;
    return ESP_OK;
}

esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us)
{
    if (timer == NULL || period_us == 0ULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (timer->active) {
        return ESP_ERR_INVALID_STATE;
    }
    timer->active = true;
    timer->periodic = true;
    timer->period_us = period_us;
    timer->next_deadline_us = (uint64_t)esp_timer_get_time() + period_us;
    return ESP_OK;
}

esp_err_t esp_timer_stop(esp_timer_handle_t timer)
{
    if (timer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!timer->active) {
        return ESP_ERR_INVALID_STATE;
    }
    timer->active = false;
    return ESP_OK;
}

esp_err_t esp_timer_delete(esp_timer_handle_t timer)
{
    if (timer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (timer->active) {
        return ESP_ERR_INVALID_STATE;
    }
    bk_port_free(timer);
    return ESP_OK;
}

bool esp_timer_is_active(esp_timer_handle_t timer)
{
    return timer != NULL && timer->active;
}

void bk_esp_timer_step_us(esp_timer_handle_t timer, uint64_t now_us)
{
    if (timer == NULL || !timer->active) {
        return;
    }
    while (timer->active && now_us >= timer->next_deadline_us) {
        if (!timer->periodic) {
            timer->active = false;
            timer->cb(timer->arg);
            break;
        }
        timer->next_deadline_us += timer->period_us;
        timer->cb(timer->arg);
    }
}

/* --- 2. esp_event implementation (backed by bk_core_event) --- */

typedef struct bk_esp_evt_sub {
    esp_event_base_t base;
    int32_t event_id;
    esp_event_handler_t handler;
    void *arg;
    uint32_t bk_sub_id;
    struct bk_esp_evt_sub *next;
} bk_esp_evt_sub_t;

typedef struct {
    bk_event_loop_handle_t bk_loop;
    bk_esp_evt_sub_t *subs;
} bk_esp_evt_loop_t;

static int32_t hash_event_base(esp_event_base_t base)
{
    if (base == NULL) {
        return BK_EVENT_ANY_ID;
    }
    uint32_t h = 2166136261U;
    for (const char *p = base; *p != '\0'; ++p) {
        h ^= (uint8_t)*p;
        h *= 16777619U;
    }
    return (int32_t)(h & 0x7FFFFFFFU);
}

static void esp_evt_bridge_cb(const bk_event_msg_t *event, void *user_ctx)
{
    bk_esp_evt_sub_t *sub = (bk_esp_evt_sub_t *)user_ctx;
    if (sub != NULL && sub->handler != NULL) {
        sub->handler(sub->arg, sub->base, event->event_id, (void *)event->payload);
    }
}

esp_err_t esp_event_loop_create(const esp_event_loop_args_t *event_loop_args,
                                esp_event_loop_handle_t *event_loop)
{
    if (event_loop_args == NULL || event_loop == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    bk_esp_evt_loop_t *wrap = (bk_esp_evt_loop_t *)bk_port_calloc(
        1, sizeof(*wrap), BK_MEM_CAP_DEFAULT, "esp_evt_loop");
    if (wrap == NULL) {
        return ESP_ERR_NO_MEM;
    }
    bk_event_loop_config_t cfg = {
        .queue_depth = event_loop_args->queue_size > 0 ? (size_t)event_loop_args->queue_size : 16U,
        .worker_stack_size = event_loop_args->task_stack_size > 0 ? event_loop_args->task_stack_size : 4096U,
        .worker_name = event_loop_args->task_name != NULL ? event_loop_args->task_name : "esp_evt",
    };
    bk_err_t err = bk_event_loop_create(&cfg, &wrap->bk_loop);
    if (err != BK_OK) {
        bk_port_free(wrap);
        return err;
    }
    *event_loop = wrap;
    return ESP_OK;
}

esp_err_t esp_event_loop_delete(esp_event_loop_handle_t event_loop)
{
    if (event_loop == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    bk_esp_evt_loop_t *wrap = (bk_esp_evt_loop_t *)event_loop;
    (void)bk_event_loop_deinit(wrap->bk_loop);
    bk_esp_evt_sub_t *cur = wrap->subs;
    while (cur != NULL) {
        bk_esp_evt_sub_t *next = cur->next;
        bk_port_free(cur);
        cur = next;
    }
    bk_port_free(wrap);
    return ESP_OK;
}

esp_err_t esp_event_handler_register_with(esp_event_loop_handle_t event_loop,
                                          esp_event_base_t event_base,
                                          int32_t event_id,
                                          esp_event_handler_t event_handler,
                                          void *event_handler_arg)
{
    if (event_loop == NULL || event_handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    bk_esp_evt_loop_t *wrap = (bk_esp_evt_loop_t *)event_loop;
    bk_esp_evt_sub_t *sub = (bk_esp_evt_sub_t *)bk_port_calloc(
        1, sizeof(*sub), BK_MEM_CAP_DEFAULT, "esp_evt_sub");
    if (sub == NULL) {
        return ESP_ERR_NO_MEM;
    }
    sub->base = event_base;
    sub->event_id = event_id;
    sub->handler = event_handler;
    sub->arg = event_handler_arg;

    int32_t fam = hash_event_base(event_base);
    bk_err_t err = bk_event_subscribe(
        wrap->bk_loop, fam, event_id, esp_evt_bridge_cb, sub, &sub->bk_sub_id);
    if (err != BK_OK) {
        bk_port_free(sub);
        return err;
    }
    sub->next = wrap->subs;
    wrap->subs = sub;
    return ESP_OK;
}

esp_err_t esp_event_post_to(esp_event_loop_handle_t event_loop,
                            esp_event_base_t event_base,
                            int32_t event_id,
                            const void *event_data,
                            size_t event_data_size,
                            uint32_t ticks_to_wait)
{
    if (event_loop == NULL || event_base == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    bk_esp_evt_loop_t *wrap = (bk_esp_evt_loop_t *)event_loop;
    int32_t fam = hash_event_base(event_base);
    bk_err_t err = bk_of_event_post(
        wrap->bk_loop, fam, event_id, event_data, event_data_size, ticks_to_wait);
    if (err != BK_OK) {
        return err;
    }
    return bk_event_loop_flush(wrap->bk_loop, 500U);
}

/* --- 3. NVS implementation (backed by bk_storage) --- */

static bk_storage_handle_t g_bound_nvs_storage = NULL;

struct bk_esp_nvs_ctx {
    char ns[BK_STORAGE_MAX_NS_LEN + 1U];
    nvs_open_mode_t mode;
};

void bk_esp_nvs_bind_storage(bk_storage_handle_t storage)
{
    g_bound_nvs_storage = storage;
}

esp_err_t nvs_open(const char *namespace_name,
                   nvs_open_mode_t open_mode,
                   nvs_handle_t *out_handle)
{
    if (namespace_name == NULL || out_handle == NULL ||
        strlen(namespace_name) == 0U || strlen(namespace_name) > BK_STORAGE_MAX_NS_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    if (g_bound_nvs_storage == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    struct bk_esp_nvs_ctx *ctx = (struct bk_esp_nvs_ctx *)bk_port_calloc(
        1, sizeof(*ctx), BK_MEM_CAP_DEFAULT, "esp_nvs");
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }
    strncpy(ctx->ns, namespace_name, BK_STORAGE_MAX_NS_LEN);
    ctx->mode = open_mode;
    *out_handle = ctx;
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle)
{
    if (handle != NULL) {
        bk_port_free(handle);
    }
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length)
{
    if (handle == NULL || key == NULL || value == NULL || g_bound_nvs_storage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->mode == NVS_READONLY) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return bk_storage_set_blob(
        g_bound_nvs_storage, handle->ns, key, value, length, BK_STORAGE_FLAG_NONE);
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length)
{
    if (handle == NULL || key == NULL || length == NULL || g_bound_nvs_storage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    uint32_t flags = 0U;
    return bk_storage_get_blob(
        g_bound_nvs_storage, handle->ns, key, out_value, length, &flags);
}

esp_err_t nvs_set_u32(nvs_handle_t handle, const char *key, uint32_t value)
{
    return nvs_set_blob(handle, key, &value, sizeof(value));
}

esp_err_t nvs_get_u32(nvs_handle_t handle, const char *key, uint32_t *out_value)
{
    if (out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t len = sizeof(*out_value);
    esp_err_t err = nvs_get_blob(handle, key, out_value, &len);
    if (err == ESP_OK && len != sizeof(*out_value)) {
        return ESP_ERR_INVALID_SIZE;
    }
    return err;
}

esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value)
{
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return nvs_set_blob(handle, key, value, strlen(value) + 1U);
}

esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out_value, size_t *length)
{
    return nvs_get_blob(handle, key, out_value, length);
}

esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key)
{
    if (handle == NULL || key == NULL || g_bound_nvs_storage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->mode == NVS_READONLY) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return bk_storage_delete_key(g_bound_nvs_storage, handle->ns, key);
}

esp_err_t nvs_erase_all(nvs_handle_t handle)
{
    if (handle == NULL || g_bound_nvs_storage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (handle->mode == NVS_READONLY) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    return bk_storage_erase_ns(g_bound_nvs_storage, handle->ns);
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    return handle != NULL ? ESP_OK : ESP_ERR_INVALID_ARG;
}

/* --- 4. espressif/button (iot_button) adapter --- */

struct bk_iot_button_adapter {
    bk_button_handle_t bk_btn;
    button_event_t last_evt;
    button_cb_t cbs[BUTTON_EVENT_MAX];
    void *usr_data[BUTTON_EVENT_MAX];
};

static void on_bk_btn_evt(uint32_t button_id, bk_button_event_t evt, void *user_ctx)
{
    (void)button_id;
    struct bk_iot_button_adapter *ad = (struct bk_iot_button_adapter *)user_ctx;
    if (ad == NULL) {
        return;
    }
    button_event_t mapped = BUTTON_EVENT_MAX;
    switch (evt) {
    case BK_BUTTON_EVT_SINGLE_CLICK:    mapped = BUTTON_SINGLE_CLICK; break;
    case BK_BUTTON_EVT_DOUBLE_CLICK:    mapped = BUTTON_DOUBLE_CLICK; break;
    case BK_BUTTON_EVT_LONG_PRESS:      mapped = BUTTON_LONG_PRESS_START; break;
    case BK_BUTTON_EVT_LONG_PRESS_HOLD: mapped = BUTTON_LONG_PRESS_HOLD; break;
    case BK_BUTTON_EVT_PRESS_DOWN:      mapped = BUTTON_PRESS_DOWN; break;
    case BK_BUTTON_EVT_PRESS_UP:        mapped = BUTTON_PRESS_UP; break;
    default:                            break;
    }
    if (mapped < BUTTON_EVENT_MAX) {
        ad->last_evt = mapped;
        if (ad->cbs[mapped] != NULL) {
            ad->cbs[mapped](ad, ad->usr_data[mapped]);
        }
    }
}

button_handle_t iot_button_create(const button_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }
    struct bk_iot_button_adapter *ad = (struct bk_iot_button_adapter *)bk_port_calloc(
        1, sizeof(*ad), BK_MEM_CAP_DEFAULT, "iot_button");
    if (ad == NULL) {
        return NULL;
    }
    ad->last_evt = BUTTON_EVENT_MAX;

    uint32_t target_mv = 0U;
    uint32_t tol_mv = 80U;
    if (config->type == BUTTON_TYPE_ADC) {
        target_mv = ((uint32_t)config->adc_button_config.min + (uint32_t)config->adc_button_config.max) / 2U;
        if (config->adc_button_config.max > config->adc_button_config.min) {
            tol_mv = ((uint32_t)config->adc_button_config.max - (uint32_t)config->adc_button_config.min) / 2U;
        }
    }

    bk_button_config_t bcfg = {
        .button_id = (config->type == BUTTON_TYPE_ADC)
                         ? (uint32_t)config->adc_button_config.button_index
                         : (uint32_t)config->gpio_button_config.gpio_num,
        .debounce_ms = 20U,
        .double_click_window_ms = config->short_press_time > 0U ? config->short_press_time : 200U,
        .long_press_ms = config->long_press_time > 0U ? config->long_press_time : 1000U,
        .long_press_hold_interval_ms = 200U,
        .adc_target_mv = target_mv,
        .adc_tolerance_mv = tol_mv,
        .cb = on_bk_btn_evt,
        .user_ctx = ad,
    };
    if (bk_button_create(&bcfg, &ad->bk_btn) != BK_OK) {
        bk_port_free(ad);
        return NULL;
    }
    return ad;
}

esp_err_t iot_button_delete(button_handle_t btn_handle)
{
    if (btn_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    bk_button_destroy(btn_handle->bk_btn);
    bk_port_free(btn_handle);
    return ESP_OK;
}

esp_err_t iot_button_register_cb(button_handle_t btn_handle,
                                 button_event_t event,
                                 button_cb_t cb,
                                 void *usr_data)
{
    if (btn_handle == NULL || event < 0 || event >= BUTTON_EVENT_MAX || cb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    btn_handle->cbs[event] = cb;
    btn_handle->usr_data[event] = usr_data;
    return ESP_OK;
}

button_event_t iot_button_get_event(button_handle_t btn_handle)
{
    return btn_handle != NULL ? btn_handle->last_evt : BUTTON_EVENT_MAX;
}

uint8_t iot_button_get_repeat(button_handle_t btn_handle)
{
    return btn_handle != NULL ? bk_button_get_repeat_count(btn_handle->bk_btn) : 0U;
}

esp_err_t bk_iot_button_feed_gpio(button_handle_t btn_handle, bool level_active, uint64_t now_ms)
{
    if (btn_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return bk_button_feed_sample(btn_handle->bk_btn, level_active, now_ms, NULL);
}

esp_err_t bk_iot_button_feed_adc_mv(button_handle_t btn_handle, uint32_t mv, uint64_t now_ms)
{
    if (btn_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return bk_button_feed_adc_mv(btn_handle->bk_btn, mv, now_ms, NULL);
}

/* --- 5. espressif/led_indicator adapter --- */

led_indicator_handle_t led_indicator_create(const led_indicator_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }
    bk_led_handle_t led = NULL;
    if (bk_led_create(&led) != BK_OK) {
        return NULL;
    }
    return led;
}

esp_err_t led_indicator_delete(led_indicator_handle_t handle)
{
    if (handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    bk_led_destroy(handle);
    return ESP_OK;
}

esp_err_t led_indicator_start(led_indicator_handle_t handle,
                              bk_led_priority_t prio,
                              bk_led_pattern_t pattern)
{
    return bk_led_set_effect(handle, prio, pattern);
}

esp_err_t led_indicator_stop(led_indicator_handle_t handle, bk_led_priority_t prio)
{
    return bk_led_clear_effect(handle, prio);
}
