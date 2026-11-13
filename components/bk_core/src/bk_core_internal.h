#ifndef BK_CORE_INTERNAL_H
#define BK_CORE_INTERNAL_H

#include <stddef.h>
#include "bk_core.h"

#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
#include <os/os.h>
#endif

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
#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
    beken_mutex_t lock;
#endif
} bk_core_state_t;

extern bk_core_state_t g_bk_core;

void bk_core_lock(void);
void bk_core_unlock(void);

#endif
