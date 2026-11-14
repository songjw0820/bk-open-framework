/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_video_frame_pool.h"
#include "bk_port_cache.h"
#include "bk_port_mem.h"
#include "bk_port_os.h"

#include <stdatomic.h>
#include <string.h>

typedef struct {
    bk_video_frame_t pub;
    atomic_uint refcount;
} bk_frame_slot_t;

struct bk_video_frame_pool {
    bk_port_mutex_t lock;
    bk_frame_slot_t *slots;
    size_t frame_count;
    size_t frame_size_bytes;
};

bk_err_t bk_video_frame_pool_create(const bk_video_frame_pool_config_t *cfg,
                                    bk_video_frame_pool_handle_t *out_pool)
{
    if (cfg == NULL || out_pool == NULL || cfg->frame_count == 0U || cfg->frame_size_bytes == 0U) {
        return BK_ERR_INVALID_ARG;
    }

    struct bk_video_frame_pool *pool = (struct bk_video_frame_pool *)bk_port_calloc(
        1, sizeof(*pool), BK_MEM_CAP_DEFAULT, "bk_vpool");
    if (pool == NULL) {
        return BK_ERR_NO_MEM;
    }

    pool->slots = (bk_frame_slot_t *)bk_port_calloc(
        cfg->frame_count, sizeof(bk_frame_slot_t), BK_MEM_CAP_DEFAULT, "bk_vpool_slots");
    if (pool->slots == NULL) {
        bk_port_free(pool);
        return BK_ERR_NO_MEM;
    }

    bk_err_t err = bk_port_mutex_create(false, &pool->lock);
    if (err != BK_OK) {
        bk_port_free(pool->slots);
        bk_port_free(pool);
        return err;
    }

    pool->frame_count = cfg->frame_count;
    /* Round capacity up to a multiple of 64 bytes (BK_FRAME_QUEUE_ALIGN_BYTES) */
    size_t aligned_sz = (cfg->frame_size_bytes + (BK_FRAME_QUEUE_ALIGN_BYTES - 1U)) &
                        ~(size_t)(BK_FRAME_QUEUE_ALIGN_BYTES - 1U);
    pool->frame_size_bytes = aligned_sz;

    for (size_t i = 0; i < cfg->frame_count; ++i) {
        uint8_t *buf = (uint8_t *)bk_port_malloc(
            aligned_sz,
            BK_MEM_CAP_PSRAM_CACHEABLE | BK_MEM_CAP_DMA_ALIGN64,
            "bk_vframe_psram");
        if (buf == NULL) {
            bk_video_frame_pool_destroy(pool);
            return BK_ERR_NO_MEM;
        }
        memset(buf, 0, aligned_sz);
        pool->slots[i].pub.data = buf;
        pool->slots[i].pub.capacity = aligned_sz;
        pool->slots[i].pub.valid_len = 0U;
        pool->slots[i].pub.width = cfg->width;
        pool->slots[i].pub.height = cfg->height;
        pool->slots[i].pub.owner_pool = pool;
        pool->slots[i].pub.slot_index = (uint32_t)i;
        atomic_init(&pool->slots[i].refcount, 0U);
    }

    *out_pool = pool;
    return BK_OK;
}

void bk_video_frame_pool_destroy(bk_video_frame_pool_handle_t pool)
{
    if (pool == NULL) {
        return;
    }
    if (pool->slots != NULL) {
        for (size_t i = 0; i < pool->frame_count; ++i) {
            bk_port_free(pool->slots[i].pub.data);
        }
        bk_port_free(pool->slots);
    }
    if (pool->lock != NULL) {
        bk_port_mutex_destroy(pool->lock);
    }
    bk_port_free(pool);
}

bk_err_t bk_video_frame_acquire(bk_video_frame_pool_handle_t pool,
                                bk_video_frame_t **out_frame)
{
    if (pool == NULL || out_frame == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_port_mutex_lock(pool->lock, UINT32_MAX);
    for (size_t i = 0; i < pool->frame_count; ++i) {
        unsigned int expected = 0U;
        if (atomic_compare_exchange_strong(&pool->slots[i].refcount, &expected, 1U)) {
            pool->slots[i].pub.valid_len = 0U;
            pool->slots[i].pub.timestamp_ms = bk_port_get_time_ms();
            *out_frame = &pool->slots[i].pub;
            bk_port_mutex_unlock(pool->lock);
            return BK_OK;
        }
    }
    bk_port_mutex_unlock(pool->lock);
    return BK_ERR_NO_MEM;
}

static bk_frame_slot_t *slot_from_frame(const bk_video_frame_t *frame)
{
    if (frame == NULL || frame->owner_pool == NULL ||
        frame->slot_index >= frame->owner_pool->frame_count) {
        return NULL;
    }
    return &frame->owner_pool->slots[frame->slot_index];
}

bk_err_t bk_frame_retain(bk_video_frame_t *frame)
{
    bk_frame_slot_t *slot = slot_from_frame(frame);
    if (slot == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    unsigned int prev = atomic_fetch_add(&slot->refcount, 1U);
    if (prev == 0U) {
        atomic_store(&slot->refcount, 0U);
        return BK_ERR_INVALID_STATE;
    }
    return BK_OK;
}

bk_err_t bk_frame_release(bk_video_frame_t *frame)
{
    bk_frame_slot_t *slot = slot_from_frame(frame);
    if (slot == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    unsigned int prev = atomic_load(&slot->refcount);
    while (prev > 0U) {
        if (atomic_compare_exchange_weak(&slot->refcount, &prev, prev - 1U)) {
            return BK_OK;
        }
    }
    return BK_ERR_INVALID_STATE;
}

uint32_t bk_frame_get_refcount(const bk_video_frame_t *frame)
{
    bk_frame_slot_t *slot = slot_from_frame(frame);
    if (slot == NULL) {
        return 0U;
    }
    return (uint32_t)atomic_load(&slot->refcount);
}

bk_err_t bk_frame_sync_for_device(bk_video_frame_t *frame)
{
    if (frame == NULL || frame->data == NULL || frame->capacity == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    size_t len = frame->valid_len > 0U ? frame->valid_len : frame->capacity;
    return bk_port_dcache_clean(frame->data, len);
}

bk_err_t bk_frame_sync_for_cpu(bk_video_frame_t *frame)
{
    if (frame == NULL || frame->data == NULL || frame->capacity == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    return bk_port_dcache_invalidate(frame->data, frame->capacity);
}

size_t bk_video_frame_pool_get_free_count(bk_video_frame_pool_handle_t pool)
{
    if (pool == NULL) {
        return 0U;
    }
    size_t free_cnt = 0U;
    for (size_t i = 0; i < pool->frame_count; ++i) {
        if (atomic_load(&pool->slots[i].refcount) == 0U) {
            free_cnt++;
        }
    }
    return free_cnt;
}
