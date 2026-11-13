#include "bk7259_robot.h"
#include "bk_board.h"
#include "bk_core.h"
#include "bk_lan.h"
#include "bk_media.h"
#include "bk_net.h"
#include "bk_peripheral.h"
#include "bk_ui.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const bk_net_config_t *net_cfg = bk_net_default_config();
    const bk_lan_config_t *lan_cfg = bk_lan_default_config();
    bk_media_config_t media_cfg = {.adapter = bk_media_default_adapter()};
    bk_ui_config_t ui_cfg = {.adapter = bk_ui_default_adapter()};
    int16_t audio_samples[80] = {0};
    const char *ui_frame = "robot:listening";
    bk_device_record_t robot_service = {
        .id = "bk7259-robot-ref",
        .name = "bk7259-robot",
        .address = "192.168.4.100",
        .port = 9000,
    };
    bk_ui_input_event_t input_ev = {
        .type = 1,
        .x = 0,
        .y = 0,
        .value = 1,
    };

    if (net_cfg == NULL) {
        net_cfg = bk_net_host_mock_config();
    }
    if (lan_cfg == NULL) {
        lan_cfg = bk_lan_host_mock_config();
    }
    if (media_cfg.adapter == NULL) {
        media_cfg.adapter = bk_media_host_mock_adapter();
    }
    if (ui_cfg.adapter == NULL) {
        ui_cfg.adapter = bk_ui_host_mock_adapter();
    }

    if (bk7259_robot_start(net_cfg, NULL) != BK_OK ||
        bk_net_get_state() != BK_NET_STATE_CONNECTED) {
        return 1;
    }

    if (bk_board_set_capability_enabled(BK_BOARD_CAPABILITY_AUDIO, true) != BK_OK ||
        bk_board_set_capability_enabled(BK_BOARD_CAPABILITY_DISPLAY, true) != BK_OK ||
        bk_board_set_capability_enabled(BK_BOARD_CAPABILITY_INPUT, true) != BK_OK ||
        bk_board_set_capability_enabled(BK_BOARD_CAPABILITY_LED, true) != BK_OK) {
        return 2;
    }

    if (bk_lan_start(lan_cfg) != BK_OK || bk_lan_publish(&robot_service) != BK_OK) {
        return 3;
    }
    if (bk_media_init(&media_cfg) != BK_OK || bk_media_start() != BK_OK ||
        bk_media_submit_audio(audio_samples, sizeof(audio_samples)) != BK_OK) {
        return 4;
    }
    if (bk_ui_init(&ui_cfg) != BK_OK || bk_ui_start() != BK_OK ||
        bk_ui_render(ui_frame, strlen(ui_frame)) != BK_OK ||
        bk_ui_dispatch_input(&input_ev) != BK_OK) {
        return 5;
    }
    if (bk_peripheral_set_led(BK_LED_STATE_ON) != BK_OK) {
        return 6;
    }

    printf("06_robot_reference: active id=%s ui=%s audio_bytes=%zu\n",
           robot_service.id,
           ui_frame,
           sizeof(audio_samples));

    if (bk_ui_stop() != BK_OK || bk_ui_deinit() != BK_OK ||
        bk_media_stop() != BK_OK || bk_media_deinit() != BK_OK ||
        bk_lan_stop() != BK_OK || bk7259_robot_stop() != BK_OK) {
        return 7;
    }
    return 0;
}
