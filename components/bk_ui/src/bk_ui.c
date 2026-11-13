#include "bk_ui.h"

#include <string.h>

static struct {
    bk_ui_state_t state;
    bk_ui_adapter_t adapter;
} s_ui;

static int has_adapter(const bk_ui_adapter_t *adapter)
{
    return adapter != NULL && (adapter->init != NULL || adapter->deinit != NULL ||
                               adapter->start != NULL || adapter->stop != NULL ||
                               adapter->render != NULL || adapter->dispatch_input != NULL);
}

bk_err_t bk_ui_init(const bk_ui_config_t *config)
{
    bk_err_t result;
    if (config == NULL || !has_adapter(config->adapter)) return BK_ERR_NOT_FOUND;
    if (s_ui.state != BK_UI_STATE_INITIAL) return BK_ERR_ALREADY_INITIALIZED;
    s_ui.adapter = *config->adapter;
    if (s_ui.adapter.init != NULL) {
        result = s_ui.adapter.init(s_ui.adapter.context);
        if (result != BK_OK) {
            memset(&s_ui, 0, sizeof(s_ui));
            return result;
        }
    }
    s_ui.state = BK_UI_STATE_READY;
    return BK_OK;
}

bk_err_t bk_ui_start(void)
{
    bk_err_t result;
    if (s_ui.state == BK_UI_STATE_INITIAL) return BK_ERR_NOT_INITIALIZED;
    if (s_ui.state == BK_UI_STATE_STARTED) return BK_ERR_ALREADY_STARTED;
    if (s_ui.adapter.start == NULL) return BK_ERR_NOT_SUPPORTED;
    result = s_ui.adapter.start(s_ui.adapter.context);
    if (result == BK_OK) s_ui.state = BK_UI_STATE_STARTED;
    return result;
}

bk_err_t bk_ui_stop(void)
{
    bk_err_t result;
    if (s_ui.state == BK_UI_STATE_INITIAL) return BK_ERR_NOT_INITIALIZED;
    if (s_ui.state != BK_UI_STATE_STARTED) return BK_ERR_NOT_STARTED;
    if (s_ui.adapter.stop == NULL) return BK_ERR_NOT_SUPPORTED;
    result = s_ui.adapter.stop(s_ui.adapter.context);
    if (result == BK_OK) s_ui.state = BK_UI_STATE_STOPPED;
    return result;
}

bk_err_t bk_ui_deinit(void)
{
    bk_err_t result = BK_OK;
    if (s_ui.state == BK_UI_STATE_INITIAL) return BK_ERR_NOT_INITIALIZED;
    if (s_ui.state == BK_UI_STATE_STARTED) return BK_ERR_BUSY;
    if (s_ui.adapter.deinit != NULL) result = s_ui.adapter.deinit(s_ui.adapter.context);
    if (result == BK_OK) memset(&s_ui, 0, sizeof(s_ui));
    return result;
}

bk_ui_state_t bk_ui_get_state(void)
{
    return s_ui.state;
}

bk_err_t bk_ui_render(const void *frame, size_t size)
{
    if (s_ui.state == BK_UI_STATE_INITIAL) return BK_ERR_NOT_INITIALIZED;
    if (s_ui.state != BK_UI_STATE_STARTED) return BK_ERR_NOT_STARTED;
    if (frame == NULL || size == 0U) return BK_ERR_INVALID_ARG;
    if (s_ui.adapter.render == NULL) return BK_ERR_NOT_SUPPORTED;
    return s_ui.adapter.render(s_ui.adapter.context, frame, size);
}

bk_err_t bk_ui_dispatch_input(const bk_ui_input_event_t *event)
{
    if (s_ui.state == BK_UI_STATE_INITIAL) return BK_ERR_NOT_INITIALIZED;
    if (s_ui.state != BK_UI_STATE_STARTED) return BK_ERR_NOT_STARTED;
    if (event == NULL) return BK_ERR_INVALID_ARG;
    if (s_ui.adapter.dispatch_input == NULL) return BK_ERR_NOT_SUPPORTED;
    return s_ui.adapter.dispatch_input(s_ui.adapter.context, event);
}
