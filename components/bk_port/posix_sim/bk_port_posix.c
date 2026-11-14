/* SPDX-License-Identifier: Apache-2.0 */
#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L

#include "bk_port_cache.h"
#include "bk_port_mem.h"
#include "bk_port_os.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct bk_port_mutex {
    pthread_mutex_t handle;
};

struct bk_port_thread {
    pthread_t handle;
    bk_port_thread_fn_t fn;
    void *arg;
};

typedef struct {
    void *raw_base;
    size_t user_size;
    uint32_t magic;
} bk_alloc_hdr_t;

#define BK_ALLOC_MAGIC 0x424B4D45U

static pthread_mutex_t g_stats_lock = PTHREAD_MUTEX_INITIALIZER;
static bk_port_mem_stats_t g_mem_stats = {0};
static bk_port_cache_stats_t g_cache_stats = {0};

bk_err_t bk_port_mutex_create(bool recursive, bk_port_mutex_t *out_mutex)
{
    if (out_mutex == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    struct bk_port_mutex *m = (struct bk_port_mutex *)calloc(1, sizeof(*m));
    if (m == NULL) {
        return BK_ERR_NO_MEM;
    }
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    if (recursive) {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    }
    int rc = pthread_mutex_init(&m->handle, &attr);
    pthread_mutexattr_destroy(&attr);
    if (rc != 0) {
        free(m);
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
    if (timeout_ms == UINT32_MAX) {
        return pthread_mutex_lock(&mutex->handle) == 0 ? BK_OK : BK_ERR_INVALID_STATE;
    }
    uint64_t deadline = bk_port_get_time_ms() + (uint64_t)timeout_ms;
    for (;;) {
        int rc = pthread_mutex_trylock(&mutex->handle);
        if (rc == 0) {
            return BK_OK;
        }
        if (rc != EBUSY) {
            return BK_ERR_INVALID_STATE;
        }
        if (bk_port_get_time_ms() >= deadline) {
            return BK_ERR_TIMEOUT;
        }
        bk_port_sleep_ms(1);
    }
}

bk_err_t bk_port_mutex_unlock(bk_port_mutex_t mutex)
{
    if (mutex == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    return pthread_mutex_unlock(&mutex->handle) == 0 ? BK_OK : BK_ERR_INVALID_STATE;
}

void bk_port_mutex_destroy(bk_port_mutex_t mutex)
{
    if (mutex == NULL) {
        return;
    }
    pthread_mutex_destroy(&mutex->handle);
    free(mutex);
}

static void *thread_trampoline(void *ctx)
{
    struct bk_port_thread *t = (struct bk_port_thread *)ctx;
    t->fn(t->arg);
    return NULL;
}

bk_err_t bk_port_thread_create(const char *name,
                               size_t stack_size,
                               bk_port_thread_fn_t fn,
                               void *arg,
                               bk_port_thread_t *out_thread)
{
    (void)name;
    (void)stack_size;
    if (fn == NULL || out_thread == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    struct bk_port_thread *t = (struct bk_port_thread *)calloc(1, sizeof(*t));
    if (t == NULL) {
        return BK_ERR_NO_MEM;
    }
    t->fn = fn;
    t->arg = arg;
    if (pthread_create(&t->handle, NULL, thread_trampoline, t) != 0) {
        free(t);
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
    pthread_join(thread->handle, NULL);
    free(thread);
    return BK_OK;
}

uint64_t bk_port_get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

void bk_port_sleep_ms(uint32_t ms)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000U);
    ts.tv_nsec = (long)((ms % 1000U) * 1000000L);
    nanosleep(&ts, NULL);
}

bk_err_t bk_port_get_random(void *buf, size_t len)
{
    if (buf == NULL && len > 0U) {
        return BK_ERR_INVALID_ARG;
    }
    if (len == 0U) {
        return BK_OK;
    }
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        return BK_ERR_IO;
    }
    uint8_t *out = (uint8_t *)buf;
    size_t got = 0U;
    while (got < len) {
        ssize_t n = read(fd, out + got, len - got);
        if (n <= 0) {
            if (errno == EINTR) {
                continue;
            }
            close(fd);
            return BK_ERR_IO;
        }
        got += (size_t)n;
    }
    close(fd);
    return BK_OK;
}

void *bk_port_malloc(size_t size, uint32_t caps, const char *tag)
{
    (void)tag;
    if (size == 0) {
        return NULL;
    }
    size_t align = (caps & BK_MEM_CAP_DMA_ALIGN64) ? BK_FRAME_QUEUE_ALIGN_BYTES : sizeof(void *);
    size_t total = size + align + sizeof(bk_alloc_hdr_t);
    uint8_t *raw = (uint8_t *)malloc(total);
    if (raw == NULL) {
        return NULL;
    }
    uintptr_t start = (uintptr_t)(raw + sizeof(bk_alloc_hdr_t));
    uintptr_t aligned = (start + (align - 1U)) & ~(uintptr_t)(align - 1U);
    bk_alloc_hdr_t *hdr = ((bk_alloc_hdr_t *)aligned) - 1;
    hdr->raw_base = raw;
    hdr->user_size = size;
    hdr->magic = BK_ALLOC_MAGIC;

    pthread_mutex_lock(&g_stats_lock);
    g_mem_stats.current_allocated_bytes += size;
    if (g_mem_stats.current_allocated_bytes > g_mem_stats.peak_allocated_bytes) {
        g_mem_stats.peak_allocated_bytes = g_mem_stats.current_allocated_bytes;
    }
    g_mem_stats.active_allocations += 1U;
    g_mem_stats.total_allocations += 1U;
    pthread_mutex_unlock(&g_stats_lock);

    return (void *)aligned;
}

void *bk_port_calloc(size_t nmemb, size_t size, uint32_t caps, const char *tag)
{
    if (nmemb == 0 || size == 0 || nmemb > (SIZE_MAX / size)) {
        return NULL;
    }
    size_t total = nmemb * size;
    void *ptr = bk_port_malloc(total, caps, tag);
    if (ptr != NULL) {
        memset(ptr, 0, total);
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
    size_t sz = hdr->user_size;
    void *raw = hdr->raw_base;

    pthread_mutex_lock(&g_stats_lock);
    if (g_mem_stats.current_allocated_bytes >= sz) {
        g_mem_stats.current_allocated_bytes -= sz;
    }
    if (g_mem_stats.active_allocations > 0) {
        g_mem_stats.active_allocations -= 1U;
    }
    pthread_mutex_unlock(&g_stats_lock);

    free(raw);
}

void bk_port_get_mem_stats(bk_port_mem_stats_t *out_stats)
{
    if (out_stats == NULL) {
        return;
    }
    pthread_mutex_lock(&g_stats_lock);
    *out_stats = g_mem_stats;
    pthread_mutex_unlock(&g_stats_lock);
}

void bk_port_reset_mem_stats(void)
{
    pthread_mutex_lock(&g_stats_lock);
    memset(&g_mem_stats, 0, sizeof(g_mem_stats));
    pthread_mutex_unlock(&g_stats_lock);
}

bk_err_t bk_port_dcache_clean(void *addr, size_t size)
{
    if (addr == NULL || size == 0) {
        return BK_ERR_INVALID_ARG;
    }
    pthread_mutex_lock(&g_stats_lock);
    g_cache_stats.clean_count += 1U;
    g_cache_stats.last_clean_bytes = size;
    pthread_mutex_unlock(&g_stats_lock);
    return BK_OK;
}

bk_err_t bk_port_dcache_invalidate(void *addr, size_t size)
{
    if (addr == NULL || size == 0) {
        return BK_ERR_INVALID_ARG;
    }
    pthread_mutex_lock(&g_stats_lock);
    g_cache_stats.invalidate_count += 1U;
    g_cache_stats.last_invalidate_bytes = size;
    pthread_mutex_unlock(&g_stats_lock);
    return BK_OK;
}

bool bk_port_addr_is_cacheable(const void *addr)
{
    return addr != NULL;
}

void bk_port_get_cache_stats(bk_port_cache_stats_t *out_stats)
{
    if (out_stats == NULL) {
        return;
    }
    pthread_mutex_lock(&g_stats_lock);
    *out_stats = g_cache_stats;
    pthread_mutex_unlock(&g_stats_lock);
}

void bk_port_reset_cache_stats(void)
{
    pthread_mutex_lock(&g_stats_lock);
    memset(&g_cache_stats, 0, sizeof(g_cache_stats));
    pthread_mutex_unlock(&g_stats_lock);
}
