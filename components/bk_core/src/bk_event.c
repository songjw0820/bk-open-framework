#include "bk_core_internal.h"

#include <stddef.h>

bk_err_t bk_event_subscribe(bk_event_id_t id, bk_event_handler_t handler, void *ctx)
{
    size_t i;

    if (handler == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_core_lock();
    if (!g_bk_core.initialized) {
        bk_core_unlock();
        return BK_ERR_NOT_INITIALIZED;
    }
    for (i = 0U; i < g_bk_core.subscription_count; ++i) {
        if (g_bk_core.subscriptions[i].id == id &&
            g_bk_core.subscriptions[i].handler == handler &&
            g_bk_core.subscriptions[i].ctx == ctx) {
            bk_core_unlock();
            return BK_ERR_ALREADY_EXISTS;
        }
    }
    if (g_bk_core.subscription_count >= g_bk_core.max_subscribers) {
        bk_core_unlock();
        return BK_ERR_LIMIT;
    }
    g_bk_core.subscriptions[g_bk_core.subscription_count++] = (bk_subscription_t){
        .id = id,
        .handler = handler,
        .ctx = ctx,
    };
    bk_core_unlock();
    return BK_OK;
}

bk_err_t bk_event_publish(const bk_event_t *event)
{
    bk_subscription_t snapshot[BK_CORE_MAX_SUBSCRIBERS];
    size_t snapshot_count = 0U;
    size_t i;
    int delivered = 0;

    if (event == NULL || (event->size > 0U && event->data == NULL)) {
        return BK_ERR_INVALID_ARG;
    }
    bk_core_lock();
    if (!g_bk_core.initialized) {
        bk_core_unlock();
        return BK_ERR_NOT_INITIALIZED;
    }
    if (!g_bk_core.started) {
        bk_core_unlock();
        return BK_ERR_NOT_STARTED;
    }
    if (g_bk_core.publishing) {
        bk_core_unlock();
        return BK_ERR_REENTRANT;
    }

    for (i = 0U; i < g_bk_core.subscription_count; ++i) {
        if (g_bk_core.subscriptions[i].id == event->id) {
            snapshot[snapshot_count++] = g_bk_core.subscriptions[i];
        }
    }
    g_bk_core.publishing = 1;
    bk_core_unlock();

    for (i = 0U; i < snapshot_count; ++i) {
        delivered = 1;
        snapshot[i].handler(event, snapshot[i].ctx);
    }

    bk_core_lock();
    g_bk_core.publishing = 0;
    bk_core_unlock();
    return delivered ? BK_OK : BK_ERR_NOT_FOUND;
}
