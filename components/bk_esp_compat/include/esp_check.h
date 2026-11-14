/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_ESP_CHECK_H
#define BK_OPEN_FRAMEWORK_ESP_CHECK_H

#include "esp_err.h"
#include "esp_log.h"

#define ESP_RETURN_ON_ERROR(x, log_tag, format, ...)               \
    do {                                                           \
        esp_err_t __err_rc = (x);                                  \
        if (__err_rc != ESP_OK) {                                  \
            ESP_LOGE(log_tag, format, ##__VA_ARGS__);              \
            return __err_rc;                                       \
        }                                                          \
    } while (0)

#define ESP_GOTO_ON_ERROR(x, goto_tag, log_tag, format, ...)       \
    do {                                                           \
        ret = (x);                                                 \
        if (ret != ESP_OK) {                                       \
            ESP_LOGE(log_tag, format, ##__VA_ARGS__);              \
            goto goto_tag;                                         \
        }                                                          \
    } while (0)

#define ESP_RETURN_ON_FALSE(a, err_code, log_tag, format, ...)     \
    do {                                                           \
        if (!(a)) {                                                \
            ESP_LOGE(log_tag, format, ##__VA_ARGS__);              \
            return (err_code);                                     \
        }                                                          \
    } while (0)

#define ESP_GOTO_ON_FALSE(a, err_code, goto_tag, log_tag, format, ...) \
    do {                                                               \
        if (!(a)) {                                                    \
            ESP_LOGE(log_tag, format, ##__VA_ARGS__);                  \
            ret = (err_code);                                          \
            goto goto_tag;                                             \
        }                                                              \
    } while (0)

#endif /* BK_OPEN_FRAMEWORK_ESP_CHECK_H */
