/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_VIDEO_FRAME_POOL_H
#define BK_OPEN_FRAMEWORK_BK_VIDEO_FRAME_POOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_caps.h"
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bk_video_frame_pool *bk_video_frame_pool_handle_t;

typedef struct bk_video_frame {
    uint8_t *data;
    size_t capacity;
    size_t valid_len;
    uint32_t width;
    uint32_t height;
    uint64_t timestamp_ms;
    bk_video_frame_pool_handle_t owner_pool;
    uint32_t slot_index;
} bk_video_frame_t;

typedef struct {
    size_t frame_count;
    size_t frame_size_bytes;
    uint32_t width;
    uint32_t height;
} bk_video_frame_pool_config_t;

bk_err_t bk_video_frame_pool_create(const bk_video_frame_pool_config_t *cfg,
                                    bk_video_frame_pool_handle_t *out_pool);
void bk_video_frame_pool_destroy(bk_video_frame_pool_handle_t pool);

bk_err_t bk_video_frame_acquire(bk_video_frame_pool_handle_t pool,
                                bk_video_frame_t **out_frame);

bk_err_t bk_frame_retain(bk_video_frame_t *frame);
bk_err_t bk_frame_release(bk_video_frame_t *frame);
uint32_t bk_frame_get_refcount(const bk_video_frame_t *frame);

/*
 * Cache synchronization wrappers:
 * - bk_frame_sync_for_device(frame): D-Cache clean after CPU writes before DMA reads
 * - bk_frame_sync_for_cpu(frame): D-Cache invalidate before CPU reads after DMA writes
 */
bk_err_t bk_frame_sync_for_device(bk_video_frame_t *frame);
bk_err_t bk_frame_sync_for_cpu(bk_video_frame_t *frame);

size_t bk_video_frame_pool_get_free_count(bk_video_frame_pool_handle_t pool);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_VIDEO_FRAME_POOL_H */
