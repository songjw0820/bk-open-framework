#include "bk_media.h"
#include <assert.h>
#include <stdio.h>

static int calls;
static bk_err_t init_error;
static bk_err_t start_error;
static bk_err_t stop_error;
static bk_err_t deinit_error;
static bk_err_t data_error;
static bk_err_t callback(void *ctx) { (void)ctx; ++calls; return BK_OK; }
static bk_err_t init_cb(void *ctx) { (void)ctx; ++calls; return init_error; }
static bk_err_t start_cb(void *ctx) { (void)ctx; ++calls; return start_error; }
static bk_err_t stop_cb(void *ctx) { (void)ctx; ++calls; return stop_error; }
static bk_err_t deinit_cb(void *ctx) { (void)ctx; ++calls; return deinit_error; }
static bk_err_t data_cb(void *ctx, const void *p, size_t n) { (void)ctx; assert(p != NULL && n == 1U); ++calls; return data_error; }

int main(void)
{
    const unsigned char sample = 1U;
    bk_media_adapter_t adapter = { .init = init_cb, .deinit = deinit_cb, .start = start_cb, .stop = stop_cb, .submit_audio = data_cb };
    bk_media_config_t config = { .adapter = &adapter };
    assert(bk_media_init(NULL) == BK_ERR_NOT_FOUND);
    assert(bk_media_init(&(bk_media_config_t){0}) == BK_ERR_NOT_FOUND);

    init_error = BK_ERR_BUSY;
    assert(bk_media_init(&config) == BK_ERR_BUSY);
    assert(bk_media_get_state() == BK_MEDIA_STATE_INITIAL);
    init_error = BK_OK;
    assert(bk_media_init(&config) == BK_OK);
    assert(bk_media_init(&config) == BK_ERR_ALREADY_INITIALIZED);
    assert(bk_media_submit_audio(&sample, 1U) == BK_ERR_NOT_STARTED);
    assert(bk_media_start() == BK_OK);
    assert(bk_media_start() == BK_ERR_ALREADY_STARTED);
    assert(bk_media_submit_audio(NULL, 1U) == BK_ERR_INVALID_ARG);
    assert(bk_media_submit_audio(&sample, 0U) == BK_ERR_INVALID_ARG);
    assert(bk_media_submit_video(&sample, 1U) == BK_ERR_NOT_SUPPORTED);
    data_error = BK_ERR_BUSY;
    assert(bk_media_submit_audio(&sample, 1U) == BK_ERR_BUSY);
    assert(bk_media_get_state() == BK_MEDIA_STATE_STARTED);
    data_error = BK_OK;
    assert(bk_media_submit_audio(&sample, 1U) == BK_OK);

    stop_error = BK_ERR_BUSY;
    assert(bk_media_stop() == BK_ERR_BUSY);
    assert(bk_media_get_state() == BK_MEDIA_STATE_STARTED);
    stop_error = BK_OK;
    assert(bk_media_stop() == BK_OK);
    deinit_error = BK_ERR_BUSY;
    assert(bk_media_deinit() == BK_ERR_BUSY);
    assert(bk_media_get_state() == BK_MEDIA_STATE_STOPPED);
    deinit_error = BK_OK;
    assert(bk_media_deinit() == BK_OK);

    adapter = (bk_media_adapter_t){ .init = callback };
    config.adapter = &adapter;
    assert(bk_media_init(&config) == BK_OK);
    assert(bk_media_start() == BK_ERR_NOT_SUPPORTED);
    assert(bk_media_deinit() == BK_OK);

    adapter = (bk_media_adapter_t){ .start = callback };
    config.adapter = &adapter;
    assert(bk_media_init(&config) == BK_OK);
    assert(bk_media_start() == BK_OK);
    assert(bk_media_stop() == BK_ERR_NOT_SUPPORTED);
    assert(bk_media_get_state() == BK_MEDIA_STATE_STARTED);
    assert(bk_media_deinit() == BK_ERR_BUSY);

    adapter = (bk_media_adapter_t){ .init = callback };
    assert(bk_media_deinit() == BK_ERR_BUSY);
    puts("test_media: PASS");
    return 0;
}
