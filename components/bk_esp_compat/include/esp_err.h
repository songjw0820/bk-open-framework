/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_ESP_ERR_H
#define BK_OPEN_FRAMEWORK_ESP_ERR_H

#include <assert.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef bk_of_err_t esp_err_t;

#define ESP_OK                    BK_OK
#define ESP_FAIL                  (-1)
#define ESP_ERR_NO_MEM            BK_ERR_NO_MEM
#define ESP_ERR_INVALID_ARG       BK_ERR_INVALID_ARG
#define ESP_ERR_INVALID_STATE     BK_ERR_INVALID_STATE
#define ESP_ERR_INVALID_SIZE      BK_ERR_PAYLOAD_TOO_LARGE
#define ESP_ERR_NOT_FOUND         BK_ERR_NOT_FOUND
#define ESP_ERR_NOT_SUPPORTED     BK_ERR_NOT_SUPPORTED
#define ESP_ERR_TIMEOUT           BK_ERR_TIMEOUT
#define ESP_ERR_INVALID_CRC       BK_ERR_CRC_MISMATCH

static inline const char *esp_err_to_name(esp_err_t code)
{
    if (code == ESP_FAIL) {
        return "ESP_FAIL";
    }
    return bk_of_err_to_name(code);
}

#ifndef ESP_ERROR_CHECK
#define ESP_ERROR_CHECK(x)        \
    do {                          \
        esp_err_t __err_rc = (x); \
        assert(__err_rc == ESP_OK); \
        (void)__err_rc;           \
    } while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_ESP_ERR_H */
