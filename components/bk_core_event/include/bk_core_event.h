/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_CORE_EVENT_H
#define BK_OPEN_FRAMEWORK_BK_CORE_EVENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BK_EVENT_MAX_PAYLOAD_SIZE  256U
#define BK_EVENT_ANY_ID            (-1)

typedef struct bk_event_loop *bk_event_loop_handle_t;

typedef struct {
    int32_t family_id;
    int32_t event_id;
    const void *payload;
    size_t payload_len;
    uint64_t timestamp_ms;
} bk_event_msg_t;

typedef void (*bk_event_handler_t)(const bk_event_msg_t *event, void *user_ctx);

typedef struct {
    size_t queue_depth;
    size_t worker_stack_size;
    const char *worker_name;
} bk_event_loop_config_t;

bk_err_t bk_event_loop_create(const bk_event_loop_config_t *cfg,
                              bk_event_loop_handle_t *out_loop);

bk_err_t bk_event_loop_deinit(bk_event_loop_handle_t loop);

bk_err_t bk_event_subscribe(bk_event_loop_handle_t loop,
                            int32_t family_id,
                            int32_t event_id,
                            bk_event_handler_t handler,
                            void *user_ctx,
                            uint32_t *out_sub_id);

bk_err_t bk_event_unsubscribe(bk_event_loop_handle_t loop, uint32_t sub_id);

bk_err_t bk_event_post(bk_event_loop_handle_t loop,
                       int32_t family_id,
                       int32_t event_id,
                       const void *payload,
                       size_t payload_len,
                       uint32_t timeout_ms);

/* Drain all currently queued events synchronously (useful for deterministic testing) */
bk_err_t bk_event_loop_flush(bk_event_loop_handle_t loop, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_CORE_EVENT_H */
