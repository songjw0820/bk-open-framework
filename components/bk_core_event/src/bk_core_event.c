/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_core_event.h"
#include "bk_port_mem.h"
#include "bk_port_os.h"

#include <string.h>

typedef struct {
    int32_t family_id;
    int32_t event_id;
    uint8_t payload[BK_EVENT_MAX_PAYLOAD_SIZE];
    size_t payload_len;
    uint64_t timestamp_ms;
} bk_event_slot_t;

typedef struct bk_event_sub_node {
    uint32_t sub_id;
    int32_t family_id;
    int32_t event_id;
    bk_event_handler_t handler;
    void *user_ctx;
    struct bk_event_sub_node *next;
} bk_event_sub_node_t;

struct bk_event_loop {
    bk_port_mutex_t lock;
    bk_port_thread_t worker;
    bk_event_slot_t *ring;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    bool stopping;
    bool busy_dispatching;
    uint32_t next_sub_id;
    bk_event_sub_node_t *subs;
};

static void event_worker_fn(void *arg)
{
    struct bk_event_loop *loop = (struct bk_event_loop *)arg;

    for (;;) {
        bk_event_slot_t item;
        bool has_item = false;

        bk_port_mutex_lock(loop->lock, UINT32_MAX);
        if (loop->count > 0) {
            item = loop->ring[loop->head];
            loop->head = (loop->head + 1U) % loop->capacity;
            loop->count -= 1U;
            loop->busy_dispatching = true;
            has_item = true;
        } else if (loop->stopping) {
            loop->busy_dispatching = false;
            bk_port_mutex_unlock(loop->lock);
            break;
        } else {
            loop->busy_dispatching = false;
        }
        bk_port_mutex_unlock(loop->lock);

        if (!has_item) {
            bk_port_sleep_ms(1);
            continue;
        }

        /* Snapshot matching subscribers under lock to allow safe unsubscribe during callback */
        bk_port_mutex_lock(loop->lock, UINT32_MAX);
        size_t match_cap = 0;
        for (bk_event_sub_node_t *cur = loop->subs; cur != NULL; cur = cur->next) {
            if ((cur->family_id == BK_EVENT_ANY_ID || cur->family_id == item.family_id) &&
                (cur->event_id == BK_EVENT_ANY_ID || cur->event_id == item.event_id)) {
                match_cap++;
            }
        }

        bk_event_handler_t stack_handlers[16];
        void *stack_ctxs[16];
        size_t match_count = 0;
        for (bk_event_sub_node_t *cur = loop->subs;
             cur != NULL && match_count < 16U && match_count < match_cap;
             cur = cur->next) {
            if ((cur->family_id == BK_EVENT_ANY_ID || cur->family_id == item.family_id) &&
                (cur->event_id == BK_EVENT_ANY_ID || cur->event_id == item.event_id)) {
                stack_handlers[match_count] = cur->handler;
                stack_ctxs[match_count] = cur->user_ctx;
                match_count++;
            }
        }
        bk_port_mutex_unlock(loop->lock);

        bk_event_msg_t msg = {
            .family_id = item.family_id,
            .event_id = item.event_id,
            .payload = item.payload_len > 0 ? item.payload : NULL,
            .payload_len = item.payload_len,
            .timestamp_ms = item.timestamp_ms,
        };
        for (size_t i = 0; i < match_count; ++i) {
            stack_handlers[i](&msg, stack_ctxs[i]);
        }

        bk_port_mutex_lock(loop->lock, UINT32_MAX);
        loop->busy_dispatching = false;
        bk_port_mutex_unlock(loop->lock);
    }
}

bk_err_t bk_event_loop_create(const bk_event_loop_config_t *cfg,
                              bk_event_loop_handle_t *out_loop)
{
    if (cfg == NULL || out_loop == NULL || cfg->queue_depth == 0) {
        return BK_ERR_INVALID_ARG;
    }
    struct bk_event_loop *loop = (struct bk_event_loop *)bk_port_calloc(
        1, sizeof(*loop), BK_MEM_CAP_DEFAULT, "bk_core_event");
    if (loop == NULL) {
        return BK_ERR_NO_MEM;
    }

    loop->ring = (bk_event_slot_t *)bk_port_calloc(
        cfg->queue_depth, sizeof(bk_event_slot_t), BK_MEM_CAP_DEFAULT, "bk_core_event_q");
    if (loop->ring == NULL) {
        bk_port_free(loop);
        return BK_ERR_NO_MEM;
    }
    loop->capacity = cfg->queue_depth;
    loop->next_sub_id = 1U;

    bk_err_t err = bk_port_mutex_create(true, &loop->lock);
    if (err != BK_OK) {
        bk_port_free(loop->ring);
        bk_port_free(loop);
        return err;
    }

    size_t stack_sz = cfg->worker_stack_size > 0 ? cfg->worker_stack_size : 4096U;
    const char *name = cfg->worker_name != NULL ? cfg->worker_name : "bk_evt_loop";
    err = bk_port_thread_create(name, stack_sz, event_worker_fn, loop, &loop->worker);
    if (err != BK_OK) {
        bk_port_mutex_destroy(loop->lock);
        bk_port_free(loop->ring);
        bk_port_free(loop);
        return err;
    }

    *out_loop = loop;
    return BK_OK;
}

bk_err_t bk_event_loop_deinit(bk_event_loop_handle_t loop)
{
    if (loop == NULL) {
        return BK_ERR_INVALID_ARG;
    }

    bk_port_mutex_lock(loop->lock, UINT32_MAX);
    loop->stopping = true;
    bk_port_mutex_unlock(loop->lock);

    bk_port_thread_join(loop->worker);

    bk_port_mutex_lock(loop->lock, UINT32_MAX);
    bk_event_sub_node_t *cur = loop->subs;
    loop->subs = NULL;
    bk_port_mutex_unlock(loop->lock);

    while (cur != NULL) {
        bk_event_sub_node_t *next = cur->next;
        bk_port_free(cur);
        cur = next;
    }

    bk_port_mutex_destroy(loop->lock);
    bk_port_free(loop->ring);
    bk_port_free(loop);
    return BK_OK;
}

bk_err_t bk_event_subscribe(bk_event_loop_handle_t loop,
                            int32_t family_id,
                            int32_t event_id,
                            bk_event_handler_t handler,
                            void *user_ctx,
                            uint32_t *out_sub_id)
{
    if (loop == NULL || handler == NULL) {
        return BK_ERR_INVALID_ARG;
    }

    bk_event_sub_node_t *node = (bk_event_sub_node_t *)bk_port_calloc(
        1, sizeof(*node), BK_MEM_CAP_DEFAULT, "bk_evt_sub");
    if (node == NULL) {
        return BK_ERR_NO_MEM;
    }

    bk_port_mutex_lock(loop->lock, UINT32_MAX);
    if (loop->stopping) {
        bk_port_mutex_unlock(loop->lock);
        bk_port_free(node);
        return BK_ERR_INVALID_STATE;
    }
    node->sub_id = loop->next_sub_id++;
    node->family_id = family_id;
    node->event_id = event_id;
    node->handler = handler;
    node->user_ctx = user_ctx;
    node->next = NULL;

    if (loop->subs == NULL) {
        loop->subs = node;
    } else {
        bk_event_sub_node_t *tail = loop->subs;
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = node;
    }
    if (out_sub_id != NULL) {
        *out_sub_id = node->sub_id;
    }
    bk_port_mutex_unlock(loop->lock);
    return BK_OK;
}

bk_err_t bk_event_unsubscribe(bk_event_loop_handle_t loop, uint32_t sub_id)
{
    if (loop == NULL || sub_id == 0) {
        return BK_ERR_INVALID_ARG;
    }

    bk_port_mutex_lock(loop->lock, UINT32_MAX);
    bk_event_sub_node_t *prev = NULL;
    bk_event_sub_node_t *cur = loop->subs;
    while (cur != NULL) {
        if (cur->sub_id == sub_id) {
            if (prev == NULL) {
                loop->subs = cur->next;
            } else {
                prev->next = cur->next;
            }
            bk_port_mutex_unlock(loop->lock);
            bk_port_free(cur);
            return BK_OK;
        }
        prev = cur;
        cur = cur->next;
    }
    bk_port_mutex_unlock(loop->lock);
    return BK_ERR_NOT_FOUND;
}

bk_err_t bk_of_event_post(bk_event_loop_handle_t loop,
                       int32_t family_id,
                       int32_t event_id,
                       const void *payload,
                       size_t payload_len,
                       uint32_t timeout_ms)
{
    if (loop == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (payload_len > 0 && payload == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (payload_len > BK_EVENT_MAX_PAYLOAD_SIZE) {
        return BK_ERR_PAYLOAD_TOO_LARGE;
    }

    uint64_t deadline = bk_port_get_time_ms() + timeout_ms;
    for (;;) {
        bk_port_mutex_lock(loop->lock, UINT32_MAX);
        if (loop->stopping) {
            bk_port_mutex_unlock(loop->lock);
            return BK_ERR_INVALID_STATE;
        }
        if (loop->count < loop->capacity) {
            bk_event_slot_t *slot = &loop->ring[loop->tail];
            slot->family_id = family_id;
            slot->event_id = event_id;
            slot->payload_len = payload_len;
            slot->timestamp_ms = bk_port_get_time_ms();
            if (payload_len > 0) {
                memcpy(slot->payload, payload, payload_len);
            }
            loop->tail = (loop->tail + 1U) % loop->capacity;
            loop->count += 1U;
            bk_port_mutex_unlock(loop->lock);
            return BK_OK;
        }
        bk_port_mutex_unlock(loop->lock);

        if (timeout_ms == 0 || bk_port_get_time_ms() >= deadline) {
            return BK_ERR_QUEUE_FULL;
        }
        bk_port_sleep_ms(1);
    }
}

bk_err_t bk_event_loop_flush(bk_event_loop_handle_t loop, uint32_t timeout_ms)
{
    if (loop == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    uint64_t deadline = bk_port_get_time_ms() + timeout_ms;
    for (;;) {
        bk_port_mutex_lock(loop->lock, UINT32_MAX);
        bool empty = (loop->count == 0 && !loop->busy_dispatching);
        bk_port_mutex_unlock(loop->lock);
        if (empty) {
            return BK_OK;
        }
        if (bk_port_get_time_ms() >= deadline) {
            return BK_ERR_TIMEOUT;
        }
        bk_port_sleep_ms(1);
    }
}
