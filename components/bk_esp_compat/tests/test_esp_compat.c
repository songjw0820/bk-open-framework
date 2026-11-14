/* SPDX-License-Identifier: Apache-2.0 */
#include "esp_check.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "iot_button.h"
#include "led_indicator.h"
#include "mock_esp_comp.h"
#include "nvs_flash.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

ESP_EVENT_DEFINE_BASE(TEST_APP_EVENTS);

static void timer_tick_cb(void *arg)
{
    int *cnt = (int *)arg;
    (*cnt)++;
}

static void on_esp_evt_rx(void *arg,
                          esp_event_base_t base,
                          int32_t event_id,
                          void *event_data)
{
    assert(strcmp(base, "TEST_APP_EVENTS") == 0);
    assert(event_id == 42);
    uint32_t *val = (uint32_t *)event_data;
    int *out = (int *)arg;
    *out = (int)(*val);
}

static void on_iot_btn_click(void *btn, void *usr_data)
{
    int *clicks = (int *)usr_data;
    (*clicks)++;
    assert(iot_button_get_event((button_handle_t)btn) == BUTTON_DOUBLE_CLICK ||
           iot_button_get_event((button_handle_t)btn) == BUTTON_SINGLE_CLICK);
}

static void test_esp_heap_and_timer(void)
{
    uint8_t *psram_dma_buf = (uint8_t *)heap_caps_malloc(
        1024U, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    assert(psram_dma_buf != NULL);
    assert(((uintptr_t)psram_dma_buf % BK_FRAME_QUEUE_ALIGN_BYTES) == 0U);
    heap_caps_free(psram_dma_buf);

    int ticks = 0;
    esp_timer_create_args_t targs = {
        .callback = timer_tick_cb,
        .arg = &ticks,
        .name = "periodic_test",
    };
    esp_timer_handle_t th = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&targs, &th));
    ESP_ERROR_CHECK(esp_timer_start_periodic(th, 10000ULL)); /* 10ms */
    assert(esp_timer_is_active(th));

    uint64_t t0 = (uint64_t)esp_timer_get_time();
    bk_esp_timer_step_us(th, t0 + 35000ULL);
    assert(ticks >= 3);

    ESP_ERROR_CHECK(esp_timer_stop(th));
    ESP_ERROR_CHECK(esp_timer_delete(th));
}

static void test_esp_nvs_and_event_loop(void)
{
    uint8_t flash_buf[2048];
    memset(flash_buf, 0xFF, sizeof(flash_buf));
    bk_storage_config_t scfg = {
        .flash_sim_buf = flash_buf,
        .flash_sim_size = sizeof(flash_buf),
    };
    bk_storage_handle_t store = NULL;
    assert(bk_storage_open(&scfg, &store) == BK_OK);
    bk_esp_nvs_bind_storage(store);

    ESP_ERROR_CHECK(nvs_flash_init());
    nvs_handle_t nvs = NULL;
    ESP_ERROR_CHECK(nvs_open("sys_cfg", NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_u32(nvs, "boot_cnt", 17U));
    ESP_ERROR_CHECK(nvs_set_str(nvs, "board", "bk7259_robot"));
    ESP_ERROR_CHECK(nvs_commit(nvs));

    uint32_t boot_cnt = 0U;
    ESP_ERROR_CHECK(nvs_get_u32(nvs, "boot_cnt", &boot_cnt));
    assert(boot_cnt == 17U);

    char board_name[32] = {0};
    size_t blen = sizeof(board_name);
    ESP_ERROR_CHECK(nvs_get_str(nvs, "board", board_name, &blen));
    assert(strcmp(board_name, "bk7259_robot") == 0);
    nvs_close(nvs);
    bk_storage_close(store);

    esp_event_loop_args_t largs = {
        .queue_size = 8,
        .task_name = "test_esp_evt",
        .task_stack_size = 4096,
    };
    esp_event_loop_handle_t loop = NULL;
    ESP_ERROR_CHECK(esp_event_loop_create(&largs, &loop));
    int rx_val = 0;
    ESP_ERROR_CHECK(esp_event_handler_register_with(
        loop, TEST_APP_EVENTS, 42, on_esp_evt_rx, &rx_val));
    uint32_t payload = 2026U;
    ESP_ERROR_CHECK(esp_event_post_to(
        loop, TEST_APP_EVENTS, 42, &payload, sizeof(payload), 100U));
    assert(rx_val == 2026);
    ESP_ERROR_CHECK(esp_event_loop_delete(loop));
}

static void test_esp_iot_button_and_led_indicator(void)
{
    button_config_t bcfg = {
        .type = BUTTON_TYPE_ADC,
        .long_press_time = 800,
        .short_press_time = 200,
        .adc_button_config = {
            .adc_channel = 1,
            .button_index = 0,
            .min = 1100,
            .max = 1300,
        },
    };
    button_handle_t btn = iot_button_create(&bcfg);
    assert(btn != NULL);
    int clicks = 0;
    ESP_ERROR_CHECK(iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, on_iot_btn_click, &clicks));

    /* Feed ADC double-click around 1200mV (idle at 3300mV) */
    ESP_ERROR_CHECK(bk_iot_button_feed_adc_mv(btn, 1200, 100));
    ESP_ERROR_CHECK(bk_iot_button_feed_adc_mv(btn, 1210, 125));
    ESP_ERROR_CHECK(bk_iot_button_feed_adc_mv(btn, 3300, 160));
    ESP_ERROR_CHECK(bk_iot_button_feed_adc_mv(btn, 3300, 185));
    ESP_ERROR_CHECK(bk_iot_button_feed_adc_mv(btn, 1190, 220));
    ESP_ERROR_CHECK(bk_iot_button_feed_adc_mv(btn, 1195, 245));
    ESP_ERROR_CHECK(bk_iot_button_feed_adc_mv(btn, 3300, 280));
    ESP_ERROR_CHECK(bk_iot_button_feed_adc_mv(btn, 3300, 305));
    assert(clicks == 1);
    assert(iot_button_get_repeat(btn) == 2U);
    ESP_ERROR_CHECK(iot_button_delete(btn));

    led_indicator_config_t lcfg = {.gpio_num = 12};
    led_indicator_handle_t led = led_indicator_create(&lcfg);
    assert(led != NULL);
    ESP_ERROR_CHECK(led_indicator_start(led, BK_LED_PRIO_LOW, BK_LED_PATTERN_BREATHE));
    ESP_ERROR_CHECK(led_indicator_start(led, BK_LED_PRIO_HIGH, BK_LED_PATTERN_FAST_BLINK));
    assert(bk_led_get_active_pattern(led) == BK_LED_PATTERN_FAST_BLINK);
    ESP_ERROR_CHECK(led_indicator_stop(led, BK_LED_PRIO_HIGH));
    assert(bk_led_get_active_pattern(led) == BK_LED_PATTERN_BREATHE);
    ESP_ERROR_CHECK(led_indicator_delete(led));
}

int main(void)
{
    bk_port_reset_mem_stats();
    test_esp_heap_and_timer();
    test_esp_nvs_and_event_loop();
    test_esp_iot_button_and_led_indicator();

    bk_port_mem_stats_t mstats;
    bk_port_get_mem_stats(&mstats);
    assert(mstats.current_allocated_bytes == 0U);
    assert(mstats.active_allocations == 0U);

    uint8_t mock_flash[4096] = {0};
    bk_storage_config_t mcfg = {
        .flash_sim_buf = mock_flash,
        .flash_sim_size = sizeof(mock_flash),
    };
    bk_storage_handle_t mstore = NULL;
    assert(bk_storage_open(&mcfg, &mstore) == BK_OK);
    bk_esp_nvs_bind_storage(mstore);
    assert(mock_esp_comp_ping() == ESP_OK);
    bk_esp_nvs_bind_storage(NULL);
    bk_storage_close(mstore);

    bk_port_get_mem_stats(&mstats);
    assert(mstats.current_allocated_bytes == 0U);
    assert(mstats.active_allocations == 0U);

    printf("PASS: test_esp_compat (esp_heap_caps, esp_timer, nvs, esp_event, iot_button ADC, led_indicator, idf_component_register)\n");
    return 0;
}
