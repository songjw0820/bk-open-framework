#include "bk_ui.h"
#include <assert.h>
#include <stdio.h>

static int calls;
static bk_err_t ok(void *ctx) { (void)ctx; ++calls; return BK_OK; }
static bk_err_t render(void *ctx, const void *p, size_t n) { (void)ctx; assert(p != NULL && n == 1U); ++calls; return BK_OK; }
static bk_err_t input(void *ctx, const bk_ui_input_event_t *event) { (void)ctx; assert(event != NULL); ++calls; return BK_OK; }

int main(void)
{
    const unsigned char frame = 1U;
    bk_ui_input_event_t event = { .type = 1U };
    bk_ui_adapter_t adapter = { .init = ok, .deinit = ok, .start = ok, .stop = ok, .render = render, .dispatch_input = input };
    bk_ui_config_t config = { .adapter = &adapter };
    assert(bk_ui_init(NULL) == BK_ERR_NOT_FOUND);
    assert(bk_ui_init(&(bk_ui_config_t){0}) == BK_ERR_NOT_FOUND);
    assert(bk_ui_init(&config) == BK_OK);
    assert(bk_ui_init(&config) == BK_ERR_ALREADY_INITIALIZED);
    assert(bk_ui_render(&frame, 1U) == BK_ERR_NOT_STARTED);
    assert(bk_ui_start() == BK_OK);
    assert(bk_ui_start() == BK_ERR_ALREADY_STARTED);
    assert(bk_ui_render(NULL, 1U) == BK_ERR_INVALID_ARG);
    assert(bk_ui_render(&frame, 1U) == BK_OK);
    assert(bk_ui_dispatch_input(&event) == BK_OK);
    assert(bk_ui_stop() == BK_OK);
    assert(bk_ui_deinit() == BK_OK);
    assert(calls == 6);
    puts("test_ui: PASS");
    return 0;
}
