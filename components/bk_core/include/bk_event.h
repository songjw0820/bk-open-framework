#ifndef BK_EVENT_H
#define BK_EVENT_H

#include <stddef.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t bk_event_id_t;

typedef struct {
    bk_event_id_t id;
    const void *data;
    size_t size;
} bk_event_t;

typedef void (*bk_event_handler_t)(const bk_event_t *event, void *ctx);

bk_err_t bk_event_subscribe(bk_event_id_t id, bk_event_handler_t handler, void *ctx);
bk_err_t bk_event_publish(const bk_event_t *event);

#ifdef __cplusplus
}
#endif

#endif
