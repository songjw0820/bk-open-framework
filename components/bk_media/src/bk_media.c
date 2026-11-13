#include "bk_media.h"

#include <string.h>

static struct {
    bk_media_state_t state;
    bk_media_adapter_t adapter;
} s_media;

static int has_adapter(const bk_media_adapter_t *adapter)
{
    return adapter != NULL && (adapter->init != NULL || adapter->deinit != NULL ||
                               adapter->start != NULL || adapter->stop != NULL ||
                               adapter->submit_audio != NULL || adapter->submit_video != NULL);
}

bk_err_t bk_media_init(const bk_media_config_t *config)
{
    bk_err_t result;
    if (config == NULL || !has_adapter(config->adapter)) {
        return BK_ERR_NOT_FOUND;
    }
    if (s_media.state != BK_MEDIA_STATE_INITIAL) {
        return BK_ERR_ALREADY_INITIALIZED;
    }
    s_media.adapter = *config->adapter;
    if (s_media.adapter.init != NULL) {
        result = s_media.adapter.init(s_media.adapter.context);
        if (result != BK_OK) {
            memset(&s_media, 0, sizeof(s_media));
            return result;
        }
    }
    s_media.state = BK_MEDIA_STATE_READY;
    return BK_OK;
}

bk_err_t bk_media_start(void)
{
    bk_err_t result;
    if (s_media.state == BK_MEDIA_STATE_INITIAL) return BK_ERR_NOT_INITIALIZED;
    if (s_media.state == BK_MEDIA_STATE_STARTED) return BK_ERR_ALREADY_STARTED;
    if (s_media.adapter.start == NULL) return BK_ERR_NOT_SUPPORTED;
    result = s_media.adapter.start(s_media.adapter.context);
    if (result == BK_OK) s_media.state = BK_MEDIA_STATE_STARTED;
    return result;
}

bk_err_t bk_media_stop(void)
{
    bk_err_t result;
    if (s_media.state == BK_MEDIA_STATE_INITIAL) return BK_ERR_NOT_INITIALIZED;
    if (s_media.state != BK_MEDIA_STATE_STARTED) return BK_ERR_NOT_STARTED;
    if (s_media.adapter.stop == NULL) return BK_ERR_NOT_SUPPORTED;
    result = s_media.adapter.stop(s_media.adapter.context);
    if (result == BK_OK) s_media.state = BK_MEDIA_STATE_STOPPED;
    return result;
}

bk_err_t bk_media_deinit(void)
{
    bk_err_t result = BK_OK;
    if (s_media.state == BK_MEDIA_STATE_INITIAL) return BK_ERR_NOT_INITIALIZED;
    if (s_media.state == BK_MEDIA_STATE_STARTED) return BK_ERR_BUSY;
    if (s_media.adapter.deinit != NULL) result = s_media.adapter.deinit(s_media.adapter.context);
    if (result == BK_OK) memset(&s_media, 0, sizeof(s_media));
    return result;
}

bk_media_state_t bk_media_get_state(void)
{
    return s_media.state;
}

static bk_err_t submit(bk_media_data_fn callback, const void *data, size_t size)
{
    if (s_media.state == BK_MEDIA_STATE_INITIAL) return BK_ERR_NOT_INITIALIZED;
    if (s_media.state != BK_MEDIA_STATE_STARTED) return BK_ERR_NOT_STARTED;
    if (data == NULL || size == 0U) return BK_ERR_INVALID_ARG;
    if (callback == NULL) return BK_ERR_NOT_SUPPORTED;
    return callback(s_media.adapter.context, data, size);
}

bk_err_t bk_media_submit_audio(const void *data, size_t size)
{
    return submit(s_media.adapter.submit_audio, data, size);
}

bk_err_t bk_media_submit_video(const void *data, size_t size)
{
    return submit(s_media.adapter.submit_video, data, size);
}
