/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_PORT_OS_H
#define BK_OPEN_FRAMEWORK_BK_PORT_OS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bk_port_mutex *bk_port_mutex_t;
typedef struct bk_port_thread *bk_port_thread_t;
typedef void (*bk_port_thread_fn_t)(void *arg);

bk_err_t bk_port_mutex_create(bool recursive, bk_port_mutex_t *out_mutex);
bk_err_t bk_port_mutex_lock(bk_port_mutex_t mutex, uint32_t timeout_ms);
bk_err_t bk_port_mutex_unlock(bk_port_mutex_t mutex);
void bk_port_mutex_destroy(bk_port_mutex_t mutex);

bk_err_t bk_port_thread_create(const char *name,
                               size_t stack_size,
                               bk_port_thread_fn_t fn,
                               void *arg,
                               bk_port_thread_t *out_thread);
bk_err_t bk_port_thread_join(bk_port_thread_t thread);

uint64_t bk_port_get_time_ms(void);
void bk_port_sleep_ms(uint32_t ms);
bk_err_t bk_port_get_random(void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_PORT_OS_H */
