#include "bk_board.h"
#include "bk_core.h"
#include "bk_ui.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    bk_core_config_t core_cfg = {.max_subscribers = BK_CORE_DEFAULT_MAX_SUBSCRIBERS};
    bk_board_config_t board_cfg = {.board_name = "bk7259_robot"};
    bk_ui_config_t ui_cfg = {0};
    bk_ui_stats_t stats = {0};
    const char *home_frame = "screen:home";
    const char *settings_frame = "screen:settings";
    bk_ui_input_event_t btn_press = {
        .type = 1,
        .x = 0,
        .y = 0,
        .value = 1,
    };
    bk_ui_input_event_t touch_tap = {
        .type = 2,
        .x = 120,
        .y = 240,
        .value = 1,
    };

    if (bk_core_init(&core_cfg) != BK_OK || bk_core_start() != BK_OK) {
        return 1;
    }
    if (bk_board_init(&board_cfg) != BK_OK) {
        return 2;
    }
    if (bk_board_set_capability_enabled(BK_BOARD_CAPABILITY_DISPLAY, true) != BK_OK ||
        bk_board_set_capability_enabled(BK_BOARD_CAPABILITY_INPUT, true) != BK_OK) {
        return 3;
    }

    ui_cfg.adapter = bk_ui_default_adapter();
    if (ui_cfg.adapter == NULL) {
        ui_cfg.adapter = bk_ui_host_mock_adapter();
    }

    if (bk_ui_init(&ui_cfg) != BK_OK || bk_ui_start() != BK_OK) {
        return 4;
    }
    if (bk_ui_render(home_frame, strlen(home_frame)) != BK_OK ||
        bk_ui_dispatch_input(&btn_press) != BK_OK ||
        bk_ui_render(settings_frame, strlen(settings_frame)) != BK_OK ||
        bk_ui_dispatch_input(&touch_tap) != BK_OK) {
        return 5;
    }

    if (bk_ui_get_stats(&stats) != BK_OK ||
        stats.rendered_frames != 2 ||
        stats.input_events != 2 ||
        stats.last_event.x != 120) {
        return 6;
    }

    printf("07_ui_control: frames=%u bytes=%zu inputs=%u last_xy=(%d,%d)\n",
           (unsigned)stats.rendered_frames,
           stats.rendered_bytes,
           (unsigned)stats.input_events,
           (int)stats.last_event.x,
           (int)stats.last_event.y);

    if (bk_ui_stop() != BK_OK || bk_ui_deinit() != BK_OK ||
        bk_board_deinit() != BK_OK || bk_core_deinit() != BK_OK) {
        return 7;
    }
    return 0;
}
