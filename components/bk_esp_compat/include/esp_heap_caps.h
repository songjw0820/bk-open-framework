/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_ESP_HEAP_CAPS_H
#define BK_OPEN_FRAMEWORK_ESP_HEAP_CAPS_H

#include <stddef.h>
#include <stdint.h>
#include "bk_caps.h"
#include "bk_port_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MALLOC_CAP_DEFAULT  0U
#define MALLOC_CAP_8BIT     (1U << 0)
#define MALLOC_CAP_INTERNAL (1U << 1)
#define MALLOC_CAP_SPIRAM   (1U << 2)
#define MALLOC_CAP_DMA      (1U << 3)

static inline uint32_t bk_esp_map_heap_caps(uint32_t esp_caps)
{
    uint32_t bk_caps = BK_MEM_CAP_DEFAULT;
    if (esp_caps & MALLOC_CAP_SPIRAM) {
        bk_caps |= BK_MEM_CAP_PSRAM_CACHEABLE;
    } else if (esp_caps & MALLOC_CAP_INTERNAL) {
        bk_caps |= BK_MEM_CAP_SRAM;
    }
    if (esp_caps & MALLOC_CAP_DMA) {
        bk_caps |= BK_MEM_CAP_DMA_ALIGN64;
    }
    return bk_caps;
}

static inline void *heap_caps_malloc(size_t size, uint32_t caps)
{
    return bk_port_malloc(size, bk_esp_map_heap_caps(caps), "esp_heap_caps");
}

static inline void *heap_caps_calloc(size_t n, size_t size, uint32_t caps)
{
    return bk_port_calloc(n, size, bk_esp_map_heap_caps(caps), "esp_heap_caps");
}

static inline void heap_caps_free(void *ptr)
{
    bk_port_free(ptr);
}

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_ESP_HEAP_CAPS_H */
