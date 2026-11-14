/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_NVS_H
#define BK_OPEN_FRAMEWORK_NVS_H

#include <stddef.h>
#include <stdint.h>
#include "bk_storage.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bk_esp_nvs_ctx *nvs_handle_t;

typedef enum {
    NVS_READONLY = 0,
    NVS_READWRITE = 1,
} nvs_open_mode_t;

#define ESP_ERR_NVS_NOT_FOUND BK_ERR_NOT_FOUND

/* Binds an underlying bk_storage_handle_t as the backing store for NVS APIs */
void bk_esp_nvs_bind_storage(bk_storage_handle_t storage);

esp_err_t nvs_open(const char *namespace_name,
                   nvs_open_mode_t open_mode,
                   nvs_handle_t *out_handle);
void nvs_close(nvs_handle_t handle);

esp_err_t nvs_set_u32(nvs_handle_t handle, const char *key, uint32_t value);
esp_err_t nvs_get_u32(nvs_handle_t handle, const char *key, uint32_t *out_value);
esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value);
esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out_value, size_t *length);
esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length);
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length);
esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key);
esp_err_t nvs_erase_all(nvs_handle_t handle);
esp_err_t nvs_commit(nvs_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_NVS_H */
