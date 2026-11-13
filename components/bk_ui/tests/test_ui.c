#include "bk_ui.h"
#include <assert.h>
#include <stdio.h>

static int calls;
static bk_err_t init_error;
static bk_err_t start_error;
static bk_err_t stop_error;
static bk_err_t deinit_error;
static bk_err_t render_error;
static bk_err_t input_error;
static bk_err_t init_cb(void *ctx) { (void)ctx; ++calls; return init_error; }
static bk_err_t start_cb(void *ctx) { (void)ctx; ++calls; return start_error; }
static bk_err_t stop_cb(void *ctx) { (void)ctx; ++calls; return stop_error; }
static bk_err_t deinit_cb(void *ctx) { (void)ctx; ++calls; return deinit_error; }
static bk_err_t render_cb(void *ctx, const void *p, size_t n) { (void)ctx; assert(p != NULL && n == 1U); ++calls; return render_error; }
static bk_err_t input_cb(void *ctx, const bk_ui_input_event_t *event) { (void)ctx; assert(event != NULL); ++calls; return input_error; }
static bk_err_t callback(void *ctx) { (void)ctx; ++calls; return BK_OK; }

int main(void)
{
    const unsigned char frame = 1U;
    bk_ui_input_event_t event = { .type = 1U };
    bk_ui_adapter_t adapter = { .init = init_cb, .deinit = deinit_cb, .start = start_cb, .stop = stop_cb, .render = render_cb, .dispatch_input = input_cb };
    bk_ui_config_t config = { .adapter = &adapter };
    assert(bk_ui_init(NULL) == BK_ERR_NOT_FOUND);
    assert(bk_ui_init(&(bk_ui_config_t){0}) == BK_ERR_NOT_FOUND);

    init_error = BK_ERR_BUSY;
    assert(bk_ui_init(&config) == BK_ERR_BUSY);
    assert(bk_ui_get_state() == BK_UI_STATE_INITIAL);
    init_error = BK_OK;
    assert(bk_ui_init(&config) == BK_OK);
    assert(bk_ui_init(&config) == BK_ERR_ALREADY_INITIALIZED);
    assert(bk_ui_render(&frame, 1U) == BK_ERR_NOT_STARTED);
    assert(bk_ui_start() == BK_OK);
    assert(bk_ui_start() == BK_ERR_ALREADY_STARTED);
    assert(bk_ui_render(NULL, 1U) == BK_ERR_INVALID_ARG);
    assert(bk_ui_render(&frame, 0U) == BK_ERR_INVALID_ARG);
    render_error = BK_ERR_BUSY;
    assert(bk_ui_render(&frame, 1U) == BK_ERR_BUSY);
    assert(bk_ui_get_state() == BK_UI_STATE_STARTED);
    render_error = BK_OK;
    assert(bk_ui_render(&frame, 1U) == BK_OK);
    input_error = BK_ERR_BUSY;
    assert(bk_ui_dispatch_input(&event) == BK_ERR_BUSY);
    assert(bk_ui_get_state() == BK_UI_STATE_STARTED);
    input_error = BK_OK;
    assert(bk_ui_dispatch_input(&event) == BK_OK);
    assert(bk_ui_dispatch_input(NULL) == BK_ERR_INVALID_ARG);
    stop_error = BK_ERR_BUSY;
    assert(bk_ui_stop() == BK_ERR_BUSY);
    assert(bk_ui_get_state() == BK_UI_STATE_STARTED);
    stop_error = BK_OK;
    assert(bk_ui_stop() == BK_OK);
    deinit_error = BK_ERR_BUSY;
    assert(bk_ui_deinit() == BK_ERR_BUSY);
    assert(bk_ui_get_state() == BK_UI_STATE_STOPPED);
    deinit_error = BK_OK;
    assert(bk_ui_deinit() == BK_OK);

    adapter = (bk_ui_adapter_t){ .init = callback };
    config.adapter = &adapter;
    assert(bk_ui_init(&config) == BK_OK);
    assert(bk_ui_start() == BK_ERR_NOT_SUPPORTED);
    assert(bk_ui_deinit() == BK_OK);

    adapter = (bk_ui_adapter_t){ .start = callback };
    config.adapter = &adapter;
    assert(bk_ui_init(&config) == BK_OK);
    assert(bk_ui_start() == BK_OK);
    assert(bk_ui_stop() == BK_ERR_NOT_SUPPORTED);
    assert(bk_ui_get_state() == BK_UI_STATE_STARTED);
    assert(bk_ui_deinit() == BK_ERR_BUSY);

    adapter = (bk_ui_adapter_t){ .init = callback };
    assert(bk_ui_deinit() == BK_ERR_BUSY);
    puts("test_ui: PASS");
    return 0;
}
