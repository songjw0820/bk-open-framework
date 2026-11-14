/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_BOARD_H
#define BK_OPEN_FRAMEWORK_BK_BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_caps.h"
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BK_BOARD_PROFILE_MINIMAL = 0,
    BK_BOARD_PROFILE_ROBOT   = 1,
} bk_board_profile_t;

typedef enum {
    BK_BOARD_CAP_WIFI_STA = 0,
    BK_BOARD_CAP_BLE_PROV,
    BK_BOARD_CAP_AUDIO_CODEC,
    BK_BOARD_CAP_CAMERA_DVP_CSI,
    BK_BOARD_CAP_DISPLAY_LVGL,
    BK_BOARD_CAP_BUTTON_GPIO,
    BK_BOARD_CAP_LED_GPIO,
    BK_BOARD_CAP_SERVO_PWM,
    BK_BOARD_CAP_BATTERY_ADC,
    BK_BOARD_CAP_PSRAM_PERIPHERAL_DMA_DIRECT,
    BK_BOARD_CAP_SECURE_OTA_8MB_DEFAULT,
    BK_BOARD_CAP_MAX
} bk_board_cap_id_t;

typedef struct {
    bk_board_profile_t profile;
    const char *cp_template_source;
    const char *cp_template_path;
} bk_board_cp_template_info_t;

bk_err_t bk_board_init(bk_board_profile_t profile);
void bk_board_deinit(void);

bk_err_t bk_board_get_cp_template_info(bk_board_profile_t profile,
                                       bk_board_cp_template_info_t *out_info);

bk_err_t bk_board_query_cap(bk_board_cap_id_t cap_id,
                            bk_cap_descriptor_t *out_desc);

/*
 * Enforces capability verification contract:
 * - Returns BK_ERR_NOT_VERIFIED if capability is BK_CAP_STATE_UNVERIFIED
 *   (or if require_hardware_verified == true and capability is not BK_CAP_STATE_HARDWARE_VERIFIED).
 * - Returns BK_ERR_NOT_SUPPORTED if robot-only peripheral (e.g., SERVO_PWM) is requested under minimal profile.
 */
bk_err_t bk_board_require_cap(bk_board_cap_id_t cap_id, bool require_hardware_verified);

/* Allows Tier 4 hardware runner to promote a capability after physical verification */
bk_err_t bk_board_mark_cap_hardware_verified(bk_board_cap_id_t cap_id);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_BOARD_H */
