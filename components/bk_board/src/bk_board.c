/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_board.h"

#include <string.h>

static bool g_board_initialized = false;
static bk_board_profile_t g_active_profile = BK_BOARD_PROFILE_MINIMAL;

static bk_cap_descriptor_t g_board_caps[BK_BOARD_CAP_MAX] = {
    [BK_BOARD_CAP_WIFI_STA] = {
        .name = "wifi_sta",
        .verify_state = BK_CAP_STATE_STATIC_VERIFIED,
        .flags = 0U,
    },
    [BK_BOARD_CAP_BLE_PROV] = {
        .name = "ble_prov",
        .verify_state = BK_CAP_STATE_STATIC_VERIFIED,
        .flags = 0U,
    },
    [BK_BOARD_CAP_AUDIO_CODEC] = {
        .name = "audio_codec",
        .verify_state = BK_CAP_STATE_STATIC_VERIFIED,
        .flags = 0U,
    },
    [BK_BOARD_CAP_CAMERA_DVP_CSI] = {
        .name = "camera_dvp_csi",
        .verify_state = BK_CAP_STATE_STATIC_VERIFIED,
        .flags = 0U,
    },
    [BK_BOARD_CAP_DISPLAY_LVGL] = {
        .name = "display_lvgl",
        .verify_state = BK_CAP_STATE_STATIC_VERIFIED,
        .flags = 0U,
    },
    [BK_BOARD_CAP_BUTTON_GPIO] = {
        .name = "button_gpio",
        .verify_state = BK_CAP_STATE_STATIC_VERIFIED,
        .flags = 0U,
    },
    [BK_BOARD_CAP_LED_GPIO] = {
        .name = "led_gpio",
        .verify_state = BK_CAP_STATE_STATIC_VERIFIED,
        .flags = 0U,
    },
    [BK_BOARD_CAP_SERVO_PWM] = {
        .name = "servo_pwm",
        .verify_state = BK_CAP_STATE_STATIC_VERIFIED,
        .flags = 1U, /* robot profile peripheral */
    },
    [BK_BOARD_CAP_BATTERY_ADC] = {
        .name = "battery_adc",
        .verify_state = BK_CAP_STATE_STATIC_VERIFIED,
        .flags = 1U, /* robot profile peripheral */
    },
    [BK_BOARD_CAP_PSRAM_PERIPHERAL_DMA_DIRECT] = {
        .name = "psram_peripheral_dma_direct",
        .verify_state = BK_CAP_STATE_UNVERIFIED,
        .flags = 0U,
    },
    [BK_BOARD_CAP_SECURE_OTA_8MB_DEFAULT] = {
        .name = "secure_ota_8mb_default",
        .verify_state = BK_CAP_STATE_UNVERIFIED,
        .flags = 0U,
    },
};

bk_err_t bk_board_init(bk_board_profile_t profile)
{
    if (profile != BK_BOARD_PROFILE_MINIMAL && profile != BK_BOARD_PROFILE_ROBOT) {
        return BK_ERR_INVALID_ARG;
    }
    g_active_profile = profile;
    g_board_initialized = true;
    /* Reset dynamic promotions to static baseline on init */
    g_board_caps[BK_BOARD_CAP_PSRAM_PERIPHERAL_DMA_DIRECT].verify_state = BK_CAP_STATE_UNVERIFIED;
    g_board_caps[BK_BOARD_CAP_SECURE_OTA_8MB_DEFAULT].verify_state = BK_CAP_STATE_UNVERIFIED;
    return BK_OK;
}

void bk_board_deinit(void)
{
    g_board_initialized = false;
}

bk_err_t bk_board_get_cp_template_info(bk_board_profile_t profile,
                                       bk_board_cp_template_info_t *out_info)
{
    if (out_info == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (profile == BK_BOARD_PROFILE_MINIMAL) {
        out_info->profile = BK_BOARD_PROFILE_MINIMAL;
        out_info->cp_template_source = "sdk";
        out_info->cp_template_path = "projects/app/cp";
        return BK_OK;
    }
    if (profile == BK_BOARD_PROFILE_ROBOT) {
        out_info->profile = BK_BOARD_PROFILE_ROBOT;
        out_info->cp_template_source = "solution";
        out_info->cp_template_path = "projects/beken_robot/cp";
        return BK_OK;
    }
    return BK_ERR_INVALID_ARG;
}

bk_err_t bk_board_query_cap(bk_board_cap_id_t cap_id,
                            bk_cap_descriptor_t *out_desc)
{
    if (!g_board_initialized) {
        return BK_ERR_INVALID_STATE;
    }
    if (cap_id < 0 || cap_id >= BK_BOARD_CAP_MAX || out_desc == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    *out_desc = g_board_caps[cap_id];
    return BK_OK;
}

bk_err_t bk_board_require_cap(bk_board_cap_id_t cap_id, bool require_hardware_verified)
{
    if (!g_board_initialized) {
        return BK_ERR_INVALID_STATE;
    }
    if (cap_id < 0 || cap_id >= BK_BOARD_CAP_MAX) {
        return BK_ERR_INVALID_ARG;
    }
    const bk_cap_descriptor_t *cap = &g_board_caps[cap_id];
    if ((cap->flags & 1U) != 0U && g_active_profile == BK_BOARD_PROFILE_MINIMAL) {
        return BK_ERR_NOT_SUPPORTED;
    }
    if (!bk_cap_is_usable(cap, require_hardware_verified)) {
        return BK_ERR_NOT_VERIFIED;
    }
    return BK_OK;
}

bk_err_t bk_board_mark_cap_hardware_verified(bk_board_cap_id_t cap_id)
{
    if (!g_board_initialized) {
        return BK_ERR_INVALID_STATE;
    }
    if (cap_id < 0 || cap_id >= BK_BOARD_CAP_MAX) {
        return BK_ERR_INVALID_ARG;
    }
    g_board_caps[cap_id].verify_state = BK_CAP_STATE_HARDWARE_VERIFIED;
    return BK_OK;
}
