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
    int16_t pcm_samples[160];
    bk_media_stats_t stats = {0};

    for (size_t i = 0; i < 160; ++i) {
        pcm_samples[i] = (int16_t)((i % 32) * 512 - 4096);
    }

    if (bk_core_init(&core_cfg) != BK_OK || bk_core_start() != BK_OK) {
        return 1;
    }
    if (bk_board_init(&board_cfg) != BK_OK) {
        return 2;
    }
    if (bk_board_set_capability_enabled(BK_BOARD_CAPABILITY_AUDIO, true) != BK_OK) {
        return 3;
    }

    media_cfg.adapter = bk_media_default_adapter();
    if (media_cfg.adapter == NULL) {
        media_cfg.adapter = bk_media_host_mock_adapter();
    }

    if (bk_media_init(&media_cfg) != BK_OK || bk_media_start() != BK_OK) {
        return 4;
    }

    for (int frame_idx = 0; frame_idx < 4; ++frame_idx) {
        if (bk_media_submit_audio(pcm_samples, sizeof(pcm_samples)) != BK_OK) {
            return 5;
        }
    }

    if (bk_media_get_stats(&stats) != BK_OK ||
        stats.audio_frames != 4 ||
        stats.audio_bytes != 4 * sizeof(pcm_samples)) {
        return 6;
    }

    printf("03_audio_player: frames=%u bytes=%zu\n",
           (unsigned)stats.audio_frames,
           stats.audio_bytes);

    if (bk_media_stop() != BK_OK || bk_media_deinit() != BK_OK ||
        bk_board_deinit() != BK_OK || bk_core_deinit() != BK_OK) {
        return 7;
    }
    return 0;
}
