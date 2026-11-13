#ifndef BK_UI_H
#define BK_UI_H

#include <stddef.h>
#include <stdint.h>
#include "bk_core.h"
#include "bk_board.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BK_UI_STATE_INITIAL = 0,
    BK_UI_STATE_READY,
    BK_UI_STATE_STARTED,
    BK_UI_STATE_STOPPED,
} bk_ui_state_t;

typedef struct {
    uint16_t type;
    int32_t x;
    int32_t y;
    int32_t value;
} bk_ui_input_event_t;

typedef bk_err_t (*bk_ui_void_fn)(void *context);
typedef bk_err_t (*bk_ui_render_fn)(void *context, const void *frame, size_t size);
typedef bk_err_t (*bk_ui_input_fn)(void *context, const bk_ui_input_event_t *event);

typedef struct {
    void *context;
    bk_ui_void_fn init;
    bk_ui_void_fn deinit;
    bk_ui_void_fn start;
    bk_ui_void_fn stop;
    bk_ui_render_fn render;
    bk_ui_input_fn dispatch_input;
} bk_ui_adapter_t;

typedef struct {
    const bk_ui_adapter_t *adapter;
} bk_ui_config_t;

bk_err_t bk_ui_init(const bk_ui_config_t *config);
bk_err_t bk_ui_start(void);
bk_err_t bk_ui_stop(void);
bk_err_t bk_ui_deinit(void);
bk_ui_state_t bk_ui_get_state(void);
bk_err_t bk_ui_render(const void *frame, size_t size);
bk_err_t bk_ui_dispatch_input(const bk_ui_input_event_t *event);

#ifdef __cplusplus
}
#endif

#endif
