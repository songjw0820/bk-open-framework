#include "bk_core.h"

#include <stddef.h>

#define BK_CORE_MAX_SUBSCRIBERS BK_CORE_DEFAULT_MAX_SUBSCRIBERS

typedef struct {
    bk_event_id_t id;
    bk_event_handler_t handler;
    void *ctx;
} bk_subscription_t;

static struct {
    int initialized;
    int started;
    size_t max_subscribers;
    size_t subscription_count;
    bk_subscription_t subscriptions[BK_CORE_MAX_SUBSCRIBERS];
} s_core;

bk_err_t bk_core_init(const bk_core_config_t *config)
{
    if (config == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (s_core.initialized) {
        return BK_ERR_ALREADY_INITIALIZED;
    }

    s_core.initialized = 1;
    s_core.started = 0;
    s_core.max_subscribers = config->max_subscribers;
    if (s_core.max_subscribers == 0U || s_core.max_subscribers > BK_CORE_MAX_SUBSCRIBERS) {
        s_core.max_subscribers = BK_CORE_MAX_SUBSCRIBERS;
    }
    s_core.subscription_count = 0U;
    return BK_OK;
}

bk_err_t bk_core_start(void)
{
    if (!s_core.initialized) {
        return BK_ERR_NOT_INITIALIZED;
    }
    if (s_core.started) {
        return BK_ERR_ALREADY_STARTED;
    }
    s_core.started = 1;
    return BK_OK;
}

bk_err_t bk_core_deinit(void)
{
    if (!s_core.initialized) {
        return BK_ERR_NOT_INITIALIZED;
    }
    s_core.initialized = 0;
    s_core.started = 0;
    s_core.max_subscribers = 0U;
    s_core.subscription_count = 0U;
    return BK_OK;
}

bk_err_t bk_event_subscribe(bk_event_id_t id, bk_event_handler_t handler, void *ctx)
{
    size_t i;

    if (handler == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (!s_core.initialized) {
        return BK_ERR_NOT_INITIALIZED;
    }
    for (i = 0U; i < s_core.subscription_count; ++i) {
        if (s_core.subscriptions[i].id == id &&
            s_core.subscriptions[i].handler == handler &&
            s_core.subscriptions[i].ctx == ctx) {
            return BK_ERR_ALREADY_EXISTS;
        }
    }
    if (s_core.subscription_count >= s_core.max_subscribers) {
        return BK_ERR_LIMIT;
    }
    s_core.subscriptions[s_core.subscription_count++] = (bk_subscription_t){
        .id = id,
        .handler = handler,
        .ctx = ctx,
    };
    return BK_OK;
}

bk_err_t bk_event_publish(const bk_event_t *event)
{
    size_t i;
    int delivered = 0;

    if (event == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (event->size > 0U && event->data == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (!s_core.initialized) {
        return BK_ERR_NOT_INITIALIZED;
    }
    if (!s_core.started) {
        return BK_ERR_NOT_STARTED;
    }
    for (i = 0U; i < s_core.subscription_count; ++i) {
        if (s_core.subscriptions[i].id == event->id) {
            delivered = 1;
            s_core.subscriptions[i].handler(event, s_core.subscriptions[i].ctx);
        }
    }
    return delivered ? BK_OK : BK_ERR_NOT_FOUND;
}
