/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_NVS_FLASH_H
#define BK_OPEN_FRAMEWORK_NVS_FLASH_H

#include "nvs.h"

static inline esp_err_t nvs_flash_init(void)
{
    return ESP_OK;
}

static inline esp_err_t nvs_flash_erase(void)
{
    return ESP_OK;
}

#endif /* BK_OPEN_FRAMEWORK_NVS_FLASH_H */
