/* SPDX-License-Identifier: Apache-2.0 */
/*
 * BK7259 (release/v4.0.1.11) Armino SMP porting backend.
 * Wraps verified Armino SDK APIs:
 * - rtos_init_mutex / rtos_init_recursive_mutex / rtos_create_thread /
 *   rtos_get_time / rtos_delay_milliseconds (ap/include/os/os.h)
 * - os_sram_malloc / psram_malloc (ap/include/os/mem.h)
 * - bk_dcache_clean_for_producer / bk_dcache_invalidate_for_consumer /
 *   bk_dcache_addr_is_cacheable (ap/middleware/arch/cortex-m/cache.c)
 */
#include "bk_port_cache.h"
#include "bk_port_mem.h"
#include "bk_port_os.h"

#include <components/system.h>
#include <os/mem.h>
#include <os/os.h>
#include <string.h>

/* Provided by the Armino platform/PSA RNG implementation. */
extern int bk_fill_rand(void *buff, size_t len);

extern bool bk_dcache_addr_is_cacheable(const void *addr);
extern void bk_dcache_clean_for_producer(void *addr, uint32_t size);
extern void bk_dcache_invalidate_for_consumer(void *addr, uint32_t size);

struct bk_port_mutex {
    beken_mutex_t handle;
    bool recursive;
};

struct bk_port_thread {
    beken_thread_t handle;
    beken_semaphore_t done_sem;
    bk_port_thread_fn_t fn;
    void *arg;
};

typedef struct {
    void *raw_base;
    size_t user_size;
    uint32_t magic;
} bk_alloc_hdr_t;

#define BK_ALLOC_MAGIC 0x424B4D45U

static bk_port_mem_stats_t g_mem_stats = {0};
static bk_port_cache_stats_t g_cache_stats = {0};

bk_err_t bk_port_mutex_create(bool recursive, bk_port_mutex_t *out_mutex)
{
    if (out_mutex == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    struct bk_port_mutex *m = (struct bk_port_mutex *)os_zalloc(sizeof(*m));
    if (m == NULL) {
        return BK_ERR_NO_MEM;
    }
    m->recursive = recursive;
    bk_err_t rc = recursive ? rtos_init_recursive_mutex(&m->handle)
                            : rtos_init_mutex(&m->handle);
    if (rc != BK_OK) {
        os_free(m);
        return BK_ERR_NO_MEM;
    }
    *out_mutex = m;
    return BK_OK;
}

bk_err_t bk_port_mutex_lock(bk_port_mutex_t mutex, uint32_t timeout_ms)
{
    if (mutex == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (mutex->recursive) {
        return rtos_lock_recursive_mutex(&mutex->handle) == BK_OK ? BK_OK : BK_ERR_TIMEOUT;
    }
    return rtos_lock_mutex_timeout(&mutex->handle, timeout_ms) == BK_OK ? BK_OK : BK_ERR_TIMEOUT;
}

bk_err_t bk_port_mutex_unlock(bk_port_mutex_t mutex)
{
    if (mutex == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (mutex->recursive) {
        return rtos_unlock_recursive_mutex(&mutex->handle) == BK_OK ? BK_OK : BK_ERR_INVALID_STATE;
    }
    return rtos_unlock_mutex(&mutex->handle) == BK_OK ? BK_OK : BK_ERR_INVALID_STATE;
}

void bk_port_mutex_destroy(bk_port_mutex_t mutex)
{
    if (mutex == NULL) {
        return;
    }
    if (mutex->recursive) {
        rtos_deinit_recursive_mutex(&mutex->handle);
    } else {
        rtos_deinit_mutex(&mutex->handle);
    }
    os_free(mutex);
}

static void armino_thread_entry(beken_thread_arg_t arg)
{
    struct bk_port_thread *t = (struct bk_port_thread *)arg;
    t->fn(t->arg);
    rtos_set_semaphore(&t->done_sem);
    rtos_delete_thread(NULL);
}

bk_err_t bk_port_thread_create(const char *name,
                               size_t stack_size,
                               bk_port_thread_fn_t fn,
                               void *arg,
                               bk_port_thread_t *out_thread)
{
    if (fn == NULL || out_thread == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    struct bk_port_thread *t = (struct bk_port_thread *)os_zalloc(sizeof(*t));
    if (t == NULL) {
        return BK_ERR_NO_MEM;
    }
    t->fn = fn;
    t->arg = arg;
    if (rtos_init_semaphore(&t->done_sem, 1) != BK_OK) {
        os_free(t);
        return BK_ERR_NO_MEM;
    }
    if (rtos_create_thread(&t->handle,
                           BEKEN_DEFAULT_WORKER_PRIORITY,
                           name != NULL ? name : "bk_of_wrk",
                           armino_thread_entry,
                           (uint32_t)(stack_size > 0 ? stack_size : 4096U),
                           (beken_thread_arg_t)t) != BK_OK) {
        rtos_deinit_semaphore(&t->done_sem);
        os_free(t);
        return BK_ERR_NO_MEM;
    }
    *out_thread = t;
    return BK_OK;
}

bk_err_t bk_port_thread_join(bk_port_thread_t thread)
{
    if (thread == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    rtos_get_semaphore(&thread->done_sem, BEKEN_NEVER_TIMEOUT);
    rtos_deinit_semaphore(&thread->done_sem);
    os_free(thread);
    return BK_OK;
}

uint64_t bk_port_get_time_ms(void)
{
    return (uint64_t)rtos_get_time();
}

void bk_port_sleep_ms(uint32_t ms)
{
    rtos_delay_milliseconds(ms);
}

bk_err_t bk_port_get_random(void *buf, size_t len)
{
    if (buf == NULL && len > 0U) {
        return BK_ERR_INVALID_ARG;
    }
    if (len == 0U) {
        return BK_OK;
    }
    return bk_fill_rand(buf, len) == 0 ? BK_OK : BK_ERR_IO;
}

void *bk_port_malloc(size_t size, uint32_t caps, const char *tag)
{
    (void)tag;
    if (size == 0) {
        return NULL;
    }
    size_t align = (caps & BK_MEM_CAP_DMA_ALIGN64) ? BK_FRAME_QUEUE_ALIGN_BYTES : sizeof(void *);
    size_t total = size + align + sizeof(bk_alloc_hdr_t);
    uint8_t *raw = NULL;

    if (caps & (BK_MEM_CAP_PSRAM_CACHEABLE | BK_MEM_CAP_PSRAM)) {
        raw = (uint8_t *)psram_malloc(total);
    } else if (caps & BK_MEM_CAP_SRAM) {
        raw = (uint8_t *)os_sram_malloc(total);
    } else {
        raw = (uint8_t *)os_malloc(total);
    }

    if (raw == NULL) {
        return NULL;
    }
    uintptr_t start = (uintptr_t)(raw + sizeof(bk_alloc_hdr_t));
    uintptr_t aligned = (start + (align - 1U)) & ~(uintptr_t)(align - 1U);
    bk_alloc_hdr_t *hdr = ((bk_alloc_hdr_t *)aligned) - 1;
    hdr->raw_base = raw;
    hdr->user_size = size;
    hdr->magic = BK_ALLOC_MAGIC;

    g_mem_stats.current_allocated_bytes += size;
    if (g_mem_stats.current_allocated_bytes > g_mem_stats.peak_allocated_bytes) {
        g_mem_stats.peak_allocated_bytes = g_mem_stats.current_allocated_bytes;
    }
    g_mem_stats.active_allocations += 1U;
    g_mem_stats.total_allocations += 1U;
    return (void *)aligned;
}

void *bk_port_calloc(size_t nmemb, size_t size, uint32_t caps, const char *tag)
{
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    size_t total = nmemb * size;
    void *ptr = bk_port_malloc(total, caps, tag);
    if (ptr != NULL) {
        os_memset(ptr, 0, total);
    }
    return ptr;
}

void bk_port_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }
    bk_alloc_hdr_t *hdr = ((bk_alloc_hdr_t *)ptr) - 1;
    if (hdr->magic != BK_ALLOC_MAGIC) {
        return;
    }
    hdr->magic = 0U;
    if (g_mem_stats.current_allocated_bytes >= hdr->user_size) {
        g_mem_stats.current_allocated_bytes -= hdr->user_size;
    }
    if (g_mem_stats.active_allocations > 0) {
        g_mem_stats.active_allocations -= 1U;
    }
    os_free(hdr->raw_base);
}

void bk_port_get_mem_stats(bk_port_mem_stats_t *out_stats)
{
    if (out_stats != NULL) {
        *out_stats = g_mem_stats;
    }
}

void bk_port_reset_mem_stats(void)
{
    os_memset(&g_mem_stats, 0, sizeof(g_mem_stats));
}

bk_err_t bk_port_dcache_clean(void *addr, size_t size)
{
    if (addr == NULL || size == 0) {
        return BK_ERR_INVALID_ARG;
    }
    bk_dcache_clean_for_producer(addr, (uint32_t)size);
    g_cache_stats.clean_count += 1U;
    g_cache_stats.last_clean_bytes = size;
    return BK_OK;
}

bk_err_t bk_port_dcache_invalidate(void *addr, size_t size)
{
    if (addr == NULL || size == 0) {
        return BK_ERR_INVALID_ARG;
    }
    bk_dcache_invalidate_for_consumer(addr, (uint32_t)size);
    g_cache_stats.invalidate_count += 1U;
    g_cache_stats.last_invalidate_bytes = size;
    return BK_OK;
}

bool bk_port_addr_is_cacheable(const void *addr)
{
    return addr != NULL && bk_dcache_addr_is_cacheable(addr);
}

void bk_port_get_cache_stats(bk_port_cache_stats_t *out_stats)
{
    if (out_stats != NULL) {
        *out_stats = g_cache_stats;
    }
}

void bk_port_reset_cache_stats(void)
{
    os_memset(&g_cache_stats, 0, sizeof(g_cache_stats));
}
