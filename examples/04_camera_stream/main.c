#include "bk_board.h"
#include "bk_core.h"
#include "bk_media.h"

#include <stdint.h>
#include <stdio.h>

int main(void)
{
    bk_core_config_t core_cfg = {.max_subscribers = BK_CORE_DEFAULT_MAX_SUBSCRIBERS};
    bk_board_config_t board_cfg = {.board_name = "bk7259_robot"};
    bk_media_config_t media_cfg = {0};
    uint8_t frame_buf[512];
    bk_media_stats_t stats = {0};

    for (size_t i = 0; i < sizeof(frame_buf); ++i) {
        frame_buf[i] = (uint8_t)(i & 0xFFU);
    }

    if (bk_core_init(&core_cfg) != BK_OK || bk_core_start() != BK_OK) {
        return 1;
    }
    if (bk_board_init(&board_cfg) != BK_OK) {
        return 2;
    }
    if (bk_board_set_capability_enabled(BK_BOARD_CAPABILITY_CAMERA, true) != BK_OK) {
        return 3;
    }

    media_cfg.adapter = bk_media_default_adapter();
    if (media_cfg.adapter == NULL) {
        media_cfg.adapter = bk_media_host_mock_adapter();
    }

    if (bk_media_init(&media_cfg) != BK_OK || bk_media_start() != BK_OK) {
        return 4;
    }

    for (int i = 0; i < 3; ++i) {
        if (bk_media_submit_video(frame_buf, sizeof(frame_buf)) != BK_OK) {
            return 5;
        }
    }

    if (bk_media_get_stats(&stats) != BK_OK ||
        stats.video_frames != 3 ||
        stats.video_bytes != 3 * sizeof(frame_buf)) {
        return 6;
    }

    printf("04_camera_stream: frames=%u bytes=%zu\n",
           (unsigned)stats.video_frames,
           stats.video_bytes);

    if (bk_media_stop() != BK_OK || bk_media_deinit() != BK_OK ||
        bk_board_deinit() != BK_OK || bk_core_deinit() != BK_OK) {
        return 7;
    }
    return 0;
}
