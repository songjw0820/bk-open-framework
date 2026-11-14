/* SPDX-License-Identifier: Apache-2.0 */
#include "mock_esp_comp.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

static const char *TAG = "mock_esp_comp";

esp_err_t mock_esp_comp_ping(void)
{
    int64_t t0 = esp_timer_get_time();
    ESP_RETURN_ON_FALSE(t0 >= 0, ESP_ERR_INVALID_STATE, TAG, "invalid timer");

    nvs_handle_t nvs = 0;
    ESP_RETURN_ON_ERROR(nvs_open("mock_esp", NVS_READWRITE, &nvs), TAG, "nvs_open failed");
    ESP_RETURN_ON_ERROR(nvs_set_u32(nvs, "ping", 42U), TAG, "nvs_set_u32 failed");
    uint32_t val = 0U;
    ESP_RETURN_ON_ERROR(nvs_get_u32(nvs, "ping", &val), TAG, "nvs_get_u32 failed");
    nvs_close(nvs);

    ESP_RETURN_ON_FALSE(val == 42U, ESP_FAIL, TAG, "unexpected nvs value");
    ESP_LOGI(TAG, "mock_esp_comp_ping ok");
    return ESP_OK;
}
