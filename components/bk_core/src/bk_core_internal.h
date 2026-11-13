#ifndef BK_CORE_INTERNAL_H
#define BK_CORE_INTERNAL_H

#include <stddef.h>
#include "bk_core.h"

#define BK_CORE_MAX_SUBSCRIBERS BK_CORE_DEFAULT_MAX_SUBSCRIBERS

typedef struct {
    bk_event_id_t id;
    bk_event_handler_t handler;
    void *ctx;
} bk_subscription_t;

typedef struct {
    int initialized;
    int started;
    int publishing;
    size_t max_subscribers;
    size_t subscription_count;
    bk_subscription_t subscriptions[BK_CORE_MAX_SUBSCRIBERS];
} bk_core_state_t;

extern bk_core_state_t g_bk_core;

#endif
