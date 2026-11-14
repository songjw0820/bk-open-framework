/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_core_event.h"
#include "bk_port_cache.h"
#include "bk_port_mem.h"
#include "bk_port_os.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int received_ids[32];
    int received_values[32];
    size_t count;
} test_collector_t;

static void on_test_event(const bk_event_msg_t *event, void *user_ctx)
{
    test_collector_t *col = (test_collector_t *)user_ctx;
    if (col->count < 32U) {
        col->received_ids[col->count] = event->event_id;
        int val = 0;
        if (event->payload != NULL && event->payload_len == sizeof(int)) {
            memcpy(&val, event->payload, sizeof(int));
        }
        col->received_values[col->count] = val;
        col->count++;
    }
}

static void test_port_mem_64b_alignment_and_cache_stats(void)
{
    bk_port_reset_mem_stats();
    bk_port_reset_cache_stats();

    for (size_t i = 1; i <= 16; ++i) {
        void *buf = bk_port_malloc(i * 73U, BK_MEM_CAP_PSRAM_CACHEABLE | BK_MEM_CAP_DMA_ALIGN64, "test_dma");
        assert(buf != NULL);
        assert(((uintptr_t)buf % BK_FRAME_QUEUE_ALIGN_BYTES) == 0U);
        assert(bk_port_dcache_clean(buf, i * 73U) == BK_OK);
        assert(bk_port_dcache_invalidate(buf, i * 73U) == BK_OK);
        bk_port_free(buf);
    }

    bk_port_mem_stats_t mstats;
    bk_port_get_mem_stats(&mstats);
    assert(mstats.current_allocated_bytes == 0U);
    assert(mstats.active_allocations == 0U);
    assert(mstats.total_allocations == 16U);

    bk_port_cache_stats_t cstats;
    bk_port_get_cache_stats(&cstats);
    assert(cstats.clean_count == 16U);
    assert(cstats.invalidate_count == 16U);
}

static void test_event_ordering_deepcopy_and_deinit(void)
{
    bk_port_reset_mem_stats();

    bk_event_loop_config_t cfg = {
        .queue_depth = 8,
        .worker_stack_size = 4096,
        .worker_name = "test_evt",
    };
    bk_event_loop_handle_t loop = NULL;
    assert(bk_event_loop_create(&cfg, &loop) == BK_OK);

    test_collector_t col = {0};
    uint32_t sub_id = 0;
    assert(bk_event_subscribe(loop, 10, BK_EVENT_ANY_ID, on_test_event, &col, &sub_id) == BK_OK);

    for (int i = 1; i <= 6; ++i) {
        int stack_payload = i * 100;
        assert(bk_event_post(loop, 10, i, &stack_payload, sizeof(stack_payload), 50) == BK_OK);
        /* Immediately overwrite stack_payload to prove deep-copy semantics */
        stack_payload = -9999;
    }

    /* Oversized payload must be rejected */
    uint8_t huge[BK_EVENT_MAX_PAYLOAD_SIZE + 1];
    memset(huge, 0xAA, sizeof(huge));
    assert(bk_event_post(loop, 10, 99, huge, sizeof(huge), 0) == BK_ERR_PAYLOAD_TOO_LARGE);

    assert(bk_event_loop_flush(loop, 1000) == BK_OK);
    assert(col.count == 6U);
    for (int i = 0; i < 6; ++i) {
        assert(col.received_ids[i] == i + 1);
        assert(col.received_values[i] == (i + 1) * 100);
    }

    assert(bk_event_unsubscribe(loop, sub_id) == BK_OK);
    assert(bk_event_loop_deinit(loop) == BK_OK);

    bk_port_mem_stats_t mstats;
    bk_port_get_mem_stats(&mstats);
    assert(mstats.current_allocated_bytes == 0U);
    assert(mstats.active_allocations == 0U);
}

int main(void)
{
    test_port_mem_64b_alignment_and_cache_stats();
    test_event_ordering_deepcopy_and_deinit();
    printf("PASS: test_event_loop (64B DMA alignment, cache stats, deep-copy, FIFO order, clean deinit)\n");
    return 0;
}
