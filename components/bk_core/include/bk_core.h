#ifndef BK_CORE_H
#define BK_CORE_H

#include <stddef.h>
#include "bk_err.h"
#include "bk_event.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BK_CORE_DEFAULT_MAX_SUBSCRIBERS 16U

typedef struct {
    size_t max_subscribers;
} bk_core_config_t;

bk_err_t bk_core_init(const bk_core_config_t *config);
bk_err_t bk_core_start(void);
bk_err_t bk_core_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
