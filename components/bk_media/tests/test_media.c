#include "bk_media.h"
#include <assert.h>
#include <stdio.h>

static int calls;
static bk_err_t ok(void *ctx) { (void)ctx; ++calls; return BK_OK; }
static bk_err_t data(void *ctx, const void *p, size_t n) { (void)ctx; assert(p != NULL && n == 1U); ++calls; return BK_OK; }

int main(void)
{
    const unsigned char sample = 1U;
    bk_media_adapter_t adapter = { .init = ok, .deinit = ok, .start = ok, .stop = ok, .submit_audio = data };
    bk_media_config_t config = { .adapter = &adapter };
    assert(bk_media_init(NULL) == BK_ERR_NOT_FOUND);
    assert(bk_media_init(&(bk_media_config_t){0}) == BK_ERR_NOT_FOUND);
    assert(bk_media_init(&config) == BK_OK);
    assert(bk_media_init(&config) == BK_ERR_ALREADY_INITIALIZED);
    assert(bk_media_submit_audio(&sample, 1U) == BK_ERR_NOT_STARTED);
    assert(bk_media_start() == BK_OK);
    assert(bk_media_start() == BK_ERR_ALREADY_STARTED);
    assert(bk_media_submit_audio(NULL, 1U) == BK_ERR_INVALID_ARG);
    assert(bk_media_submit_video(&sample, 1U) == BK_ERR_NOT_SUPPORTED);
    assert(bk_media_submit_audio(&sample, 1U) == BK_OK);
    assert(bk_media_stop() == BK_OK);
    assert(bk_media_deinit() == BK_OK);
    assert(calls == 5);
    puts("test_media: PASS");
    return 0;
}
