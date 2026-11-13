#ifndef BK_CORE_H
#define BK_CORE_H

#include <stddef.h>
#include "bk_err.h"
#include "bk_event.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
#include <components/log.h>
#define BK_FRAMEWORK_LOGI(tag, fmt, ...) BK_LOGI(tag, fmt "\r\n", ##__VA_ARGS__)
#define BK_FRAMEWORK_LOGW(tag, fmt, ...) BK_LOGW(tag, fmt "\r\n", ##__VA_ARGS__)
#define BK_FRAMEWORK_LOGE(tag, fmt, ...) BK_LOGE(tag, fmt "\r\n", ##__VA_ARGS__)
#else
#include <stdio.h>
#define BK_FRAMEWORK_LOGI(tag, fmt, ...) fprintf(stdout, "[I][%s] " fmt "\n", (tag), ##__VA_ARGS__)
#define BK_FRAMEWORK_LOGW(tag, fmt, ...) fprintf(stdout, "[W][%s] " fmt "\n", (tag), ##__VA_ARGS__)
#define BK_FRAMEWORK_LOGE(tag, fmt, ...) fprintf(stderr, "[E][%s] " fmt "\n", (tag), ##__VA_ARGS__)
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
