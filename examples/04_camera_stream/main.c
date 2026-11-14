/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_board.h"
#include "bk_ui_core.h"
#include "bk_video_frame_pool.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(bk_board_init(BK_BOARD_PROFILE_ROBOT) == BK_OK);
    assert(bk_board_require_cap(BK_BOARD_CAP_CAMERA_DVP_CSI, false) == BK_OK);

    bk_video_frame_pool_config_t vcfg = {
        .frame_count = 3,
        .frame_size_bytes = 320U * 240U * 2U,
        .width = 320,
        .height = 240,
    };
    bk_video_frame_pool_handle_t pool = NULL;
    assert(bk_video_frame_pool_create(&vcfg, &pool) == BK_OK);

    bk_ui_core_handle_t ui = NULL;
    assert(bk_ui_core_create(&ui) == BK_OK);

    bk_video_frame_t *frame = NULL;
    assert(bk_video_frame_acquire(pool, &frame) == BK_OK);
    assert(bk_frame_sync_for_cpu(frame) == BK_OK);

    assert(bk_ui_lock(ui, 100) == BK_OK);
    assert(bk_frame_sync_for_device(frame) == BK_OK);
    assert(bk_ui_unlock(ui) == BK_OK);

    assert(bk_frame_release(frame) == BK_OK);
    bk_ui_core_destroy(ui);
    bk_video_frame_pool_destroy(pool);
    bk_board_deinit();

    printf("PASS: example 04_camera_stream (64B PSRAM frame pool + D-Cache sync + LVGL lock)\n");
    return 0;
}
