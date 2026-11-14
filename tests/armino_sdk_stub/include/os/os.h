/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ARMINO_STUB_OS_OS_H
#define ARMINO_STUB_OS_OS_H

#include <stdint.h>
#include "bk_err.h"

typedef void *beken_mutex_t;
typedef void *beken_thread_t;
typedef void *beken_semaphore_t;
typedef void *beken_thread_arg_t;
typedef void (*beken_thread_function_t)(beken_thread_arg_t arg);

#define BEKEN_DEFAULT_WORKER_PRIORITY 5
#define BEKEN_NEVER_TIMEOUT           0xFFFFFFFFU

bk_err_t rtos_init_mutex(beken_mutex_t *mutex);
bk_err_t rtos_init_recursive_mutex(beken_mutex_t *mutex);
bk_err_t rtos_lock_mutex_timeout(beken_mutex_t *mutex, uint32_t timeout_ms);
bk_err_t rtos_lock_recursive_mutex(beken_mutex_t *mutex);
bk_err_t rtos_unlock_mutex(beken_mutex_t *mutex);
bk_err_t rtos_unlock_recursive_mutex(beken_mutex_t *mutex);
bk_err_t rtos_deinit_mutex(beken_mutex_t *mutex);
bk_err_t rtos_deinit_recursive_mutex(beken_mutex_t *mutex);

bk_err_t rtos_init_semaphore(beken_semaphore_t *semaphore, int max_count);
bk_err_t rtos_set_semaphore(beken_semaphore_t *semaphore);
bk_err_t rtos_get_semaphore(beken_semaphore_t *semaphore, uint32_t timeout_ms);
bk_err_t rtos_deinit_semaphore(beken_semaphore_t *semaphore);

bk_err_t rtos_create_thread(beken_thread_t *thread,
                            uint8_t priority,
                            const char *name,
                            beken_thread_function_t function,
                            uint32_t stack_size,
                            beken_thread_arg_t arg);
bk_err_t rtos_delete_thread(beken_thread_t *thread);

uint32_t rtos_get_time(void);
bk_err_t rtos_delay_milliseconds(uint32_t num_ms);

#endif /* ARMINO_STUB_OS_OS_H */
