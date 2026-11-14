/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_ESP_LOG_H
#define BK_OPEN_FRAMEWORK_ESP_LOG_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_LOG_NONE = 0,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO,
    ESP_LOG_DEBUG,
    ESP_LOG_VERBOSE,
} esp_log_level_t;

#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "E (%s): " fmt "\n", (tag), ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "W (%s): " fmt "\n", (tag), ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) fprintf(stdout, "I (%s): " fmt "\n", (tag), ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) do { (void)(tag); } while (0)
#define ESP_LOGV(tag, fmt, ...) do { (void)(tag); } while (0)

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_ESP_LOG_H */
