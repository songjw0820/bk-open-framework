/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_ESP_EVENT_H
#define BK_OPEN_FRAMEWORK_ESP_EVENT_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef const char *esp_event_base_t;
typedef void *esp_event_loop_handle_t;

#define ESP_EVENT_ANY_BASE NULL
#define ESP_EVENT_ANY_ID   (-1)

#define ESP_EVENT_DECLARE_BASE(id) extern esp_event_base_t const id
#define ESP_EVENT_DEFINE_BASE(id)  esp_event_base_t const id = #id

typedef void (*esp_event_handler_t)(void *event_handler_arg,
                                    esp_event_base_t event_base,
                                    int32_t event_id,
                                    void *event_data);

typedef struct {
    int32_t queue_size;
    const char *task_name;
    uint32_t task_stack_size;
} esp_event_loop_args_t;

esp_err_t esp_event_loop_create(const esp_event_loop_args_t *event_loop_args,
                                esp_event_loop_handle_t *event_loop);
esp_err_t esp_event_loop_delete(esp_event_loop_handle_t event_loop);

esp_err_t esp_event_handler_register_with(esp_event_loop_handle_t event_loop,
                                          esp_event_base_t event_base,
                                          int32_t event_id,
                                          esp_event_handler_t event_handler,
                                          void *event_handler_arg);

esp_err_t esp_event_post_to(esp_event_loop_handle_t event_loop,
                            esp_event_base_t event_base,
                            int32_t event_id,
                            const void *event_data,
                            size_t event_data_size,
                            uint32_t ticks_to_wait);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_ESP_EVENT_H */
