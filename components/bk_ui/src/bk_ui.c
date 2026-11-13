#include "bk_ui.h"

#include <string.h>

#if (defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)) && defined(CONFIG_LVGL)
#include <common/bk_include.h>
extern void lv_vendor_start(void);
extern void lv_vendor_stop(void);
extern void lv_vendor_disp_lock(void);
extern void lv_vendor_disp_unlock(void);
#if defined(CONFIG_BK_SMART_CONFIG)
extern void bk_key_app_notify_ui_nav(uint8_t event);
#endif
#endif

static struct {
    bk_ui_state_t state;
    bk_ui_adapter_t adapter;
    bk_ui_stats_t stats;
} s_ui;

static bk_err_t host_mock_ui_noop(void *context)
{
    (void)context;
    return BK_OK;
}

static bk_err_t host_mock_ui_render(void *context, const void *frame, size_t size)
{
    (void)context;
    if (frame == NULL || size == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    return BK_OK;
}

static bk_err_t host_mock_ui_input(void *context, const bk_ui_input_event_t *event)
{
    (void)context;
    if (event == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    return BK_OK;
}

static const bk_ui_adapter_t k_host_mock_ui_adapter = {
    .context = NULL,
    .init = host_mock_ui_noop,
    .deinit = host_mock_ui_noop,
    .start = host_mock_ui_noop,
    .stop = host_mock_ui_noop,
    .render = host_mock_ui_render,
    .dispatch_input = host_mock_ui_input,
};

#if (defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)) && defined(CONFIG_LVGL)
static bk_err_t armino_ui_noop(void *context)
{
    (void)context;
    return BK_OK;
}

static bk_err_t armino_ui_start(void *context)
{
    (void)context;
    lv_vendor_start();
    return BK_OK;
}

static bk_err_t armino_ui_stop(void *context)
{
    (void)context;
    lv_vendor_stop();
    return BK_OK;
}

static bk_err_t armino_ui_render(void *context, const void *frame, size_t size)
{
    (void)context;
    if (frame == NULL || size == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    lv_vendor_disp_lock();
    lv_vendor_disp_unlock();
    return BK_OK;
}

static bk_err_t armino_ui_input(void *context, const bk_ui_input_event_t *event)
{
    (void)context;
    if (event == NULL) {
        return BK_ERR_INVALID_ARG;
    }
#if defined(CONFIG_BK_SMART_CONFIG)
    bk_key_app_notify_ui_nav((uint8_t)event->type);
#endif
    return BK_OK;
}

static const bk_ui_adapter_t k_default_ui_adapter = {
    .context = NULL,
    .init = armino_ui_noop,
    .deinit = armino_ui_noop,
    .start = armino_ui_start,
    .stop = armino_ui_stop,
    .render = armino_ui_render,
    .dispatch_input = armino_ui_input,
};
#endif

const bk_ui_adapter_t *bk_ui_default_adapter(void)
{
#if (defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)) && defined(CONFIG_LVGL)
    return &k_default_ui_adapter;
#else
    return NULL;
#endif
}

const bk_ui_adapter_t *bk_ui_host_mock_adapter(void)
{
    return &k_host_mock_ui_adapter;
}

static int has_adapter(const bk_ui_adapter_t *adapter)
{
    return adapter != NULL && (adapter->init != NULL || adapter->deinit != NULL ||
                               adapter->start != NULL || adapter->stop != NULL ||
                               adapter->render != NULL || adapter->dispatch_input != NULL);
}

bk_err_t bk_ui_init(const bk_ui_config_t *config)
{
    bk_err_t result;
    const bk_ui_adapter_t *adapter;
    if (config == NULL) return BK_ERR_NOT_FOUND;
    adapter = config->adapter != NULL ? config->adapter : bk_ui_default_adapter();
    if (!has_adapter(adapter)) return BK_ERR_NOT_FOUND;
    if (s_ui.state != BK_UI_STATE_INITIAL) return BK_ERR_ALREADY_INITIALIZED;
    memset(&s_ui.stats, 0, sizeof(s_ui.stats));
    s_ui.adapter = *adapter;
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
    bk_err_t err;
    if (s_ui.state == BK_UI_STATE_INITIAL) return BK_ERR_NOT_INITIALIZED;
    if (s_ui.state != BK_UI_STATE_STARTED) return BK_ERR_NOT_STARTED;
    if (frame == NULL || size == 0U) return BK_ERR_INVALID_ARG;
    if (s_ui.adapter.render == NULL) return BK_ERR_NOT_SUPPORTED;
    err = s_ui.adapter.render(s_ui.adapter.context, frame, size);
    if (err == BK_OK) {
        s_ui.stats.rendered_frames++;
        s_ui.stats.rendered_bytes += size;
    }
    return err;
}

bk_err_t bk_ui_dispatch_input(const bk_ui_input_event_t *event)
{
    bk_err_t err;
    if (s_ui.state == BK_UI_STATE_INITIAL) return BK_ERR_NOT_INITIALIZED;
    if (s_ui.state != BK_UI_STATE_STARTED) return BK_ERR_NOT_STARTED;
    if (event == NULL) return BK_ERR_INVALID_ARG;
    if (s_ui.adapter.dispatch_input == NULL) return BK_ERR_NOT_SUPPORTED;
    err = s_ui.adapter.dispatch_input(s_ui.adapter.context, event);
    if (err == BK_OK) {
        s_ui.stats.input_events++;
        s_ui.stats.last_event = *event;
    }
    return err;
}

bk_err_t bk_ui_get_stats(bk_ui_stats_t *stats)
{
    if (stats == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (s_ui.state == BK_UI_STATE_INITIAL) {
        return BK_ERR_NOT_INITIALIZED;
    }
    *stats = s_ui.stats;
    return BK_OK;
}
