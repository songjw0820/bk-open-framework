#ifndef BK_MEDIA_H
#define BK_MEDIA_H

#include <stddef.h>
#include <stdint.h>
#include "bk_core.h"
#include "bk_board.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BK_MEDIA_STATE_INITIAL = 0,
    BK_MEDIA_STATE_READY,
    BK_MEDIA_STATE_STARTED,
    BK_MEDIA_STATE_STOPPED,
} bk_media_state_t;

typedef enum {
    BK_MEDIA_AUDIO = 0,
    BK_MEDIA_VIDEO,
} bk_media_stream_t;

typedef bk_err_t (*bk_media_void_fn)(void *context);
typedef bk_err_t (*bk_media_data_fn)(void *context, const void *data, size_t size);

typedef struct {
    void *context;
    bk_media_void_fn init;
    bk_media_void_fn deinit;
    bk_media_void_fn start;
    bk_media_void_fn stop;
    bk_media_data_fn submit_audio;
    bk_media_data_fn submit_video;
} bk_media_adapter_t;

typedef struct {
    const bk_media_adapter_t *adapter;
} bk_media_config_t;

bk_err_t bk_media_init(const bk_media_config_t *config);
bk_err_t bk_media_start(void);
bk_err_t bk_media_stop(void);
bk_err_t bk_media_deinit(void);
bk_media_state_t bk_media_get_state(void);
bk_err_t bk_media_submit_audio(const void *data, size_t size);
bk_err_t bk_media_submit_video(const void *data, size_t size);
const bk_media_adapter_t *bk_media_default_adapter(void);

#ifdef __cplusplus
}
#endif

#endif
