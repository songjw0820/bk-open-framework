#include "bk_media.h"

#include <string.h>

#if (defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)) && (defined(CONFIG_MEDIA_SERVICE) || defined(CONFIG_MEDIA))
#include <common/bk_include.h>
extern bk_err_t media_service_init(void);
#if defined(CONFIG_BK_AUDIO_ENGINE)
extern int audio_engine_init(void);
extern int audio_engine_stop(void);
extern int audio_engine_write_data(const uint8_t *data, uint32_t size, uint32_t timeout_ms);
#endif
#if defined(CONFIG_BK_ROBOT_VIDEO_SERVICE)
extern bk_err_t robot_video_service_init(void);
extern bk_err_t robot_video_service_stop(void);
#endif
#endif

static struct {
    bk_media_state_t state;
    bk_media_adapter_t adapter;
    bk_media_stats_t stats;
} s_media;

static bk_err_t host_mock_media_noop(void *context)
{
    (void)context;
    return BK_OK;
}

static bk_err_t host_mock_media_submit(void *context, const void *data, size_t size)
{
    (void)context;
    if (data == NULL || size == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    return BK_OK;
}

static const bk_media_adapter_t k_host_mock_media_adapter = {
    .context = NULL,
    .init = host_mock_media_noop,
    .deinit = host_mock_media_noop,
    .start = host_mock_media_noop,
    .stop = host_mock_media_noop,
    .submit_audio = host_mock_media_submit,
    .submit_video = host_mock_media_submit,
};

#if (defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)) && (defined(CONFIG_MEDIA_SERVICE) || defined(CONFIG_MEDIA))
static bk_err_t armino_media_init(void *context)
{
    (void)context;
    if (media_service_init() != BK_OK) {
        return BK_ERR_STATE;
    }
#if defined(CONFIG_BK_AUDIO_ENGINE)
    if (audio_engine_init() != 0) {
        return BK_ERR_STATE;
    }
#endif
#if defined(CONFIG_BK_ROBOT_VIDEO_SERVICE)
    if (robot_video_service_init() != BK_OK) {
        return BK_ERR_STATE;
    }
#endif
    return BK_OK;
}

static bk_err_t armino_media_noop(void *context)
{
    (void)context;
    return BK_OK;
}

static bk_err_t armino_media_stop(void *context)
{
    (void)context;
#if defined(CONFIG_BK_AUDIO_ENGINE)
    (void)audio_engine_stop();
#endif
#if defined(CONFIG_BK_ROBOT_VIDEO_SERVICE)
    (void)robot_video_service_stop();
#endif
    return BK_OK;
}

static bk_err_t armino_media_submit_audio(void *context, const void *data, size_t size)
{
    (void)context;
    if (data == NULL || size == 0U) {
        return BK_ERR_INVALID_ARG;
    }
#if defined(CONFIG_BK_AUDIO_ENGINE)
    return audio_engine_write_data((const uint8_t *)data, (uint32_t)size, 100U) == 0
        ? BK_OK : BK_ERR_STATE;
#else
    return BK_OK;
#endif
}

static bk_err_t armino_media_submit_video(void *context, const void *data, size_t size)
{
    (void)context;
    if (data == NULL || size == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    return BK_OK;
}

static const bk_media_adapter_t k_default_media_adapter = {
    .context = NULL,
    .init = armino_media_init,
    .deinit = armino_media_noop,
    .start = armino_media_noop,
    .stop = armino_media_stop,
    .submit_audio = armino_media_submit_audio,
    .submit_video = armino_media_submit_video,
};
#endif

const bk_media_adapter_t *bk_media_default_adapter(void)
{
#if (defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)) && (defined(CONFIG_MEDIA_SERVICE) || defined(CONFIG_MEDIA))
    return &k_default_media_adapter;
#else
    return NULL;
#endif
}

const bk_media_adapter_t *bk_media_host_mock_adapter(void)
{
    return &k_host_mock_media_adapter;
}

static int has_adapter(const bk_media_adapter_t *adapter)
{
    return adapter != NULL && (adapter->init != NULL || adapter->deinit != NULL ||
                               adapter->start != NULL || adapter->stop != NULL ||
                               adapter->submit_audio != NULL || adapter->submit_video != NULL);
}

bk_err_t bk_media_init(const bk_media_config_t *config)
{
    bk_err_t result;
    const bk_media_adapter_t *adapter;
    if (config == NULL) {
        return BK_ERR_NOT_FOUND;
    }
    adapter = config->adapter != NULL ? config->adapter : bk_media_default_adapter();
    if (!has_adapter(adapter)) {
        return BK_ERR_NOT_FOUND;
    }
    if (s_media.state != BK_MEDIA_STATE_INITIAL) {
        return BK_ERR_ALREADY_INITIALIZED;
    }
    memset(&s_media.stats, 0, sizeof(s_media.stats));
    s_media.adapter = *adapter;
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
    bk_err_t err = submit(s_media.adapter.submit_audio, data, size);
    if (err == BK_OK) {
        s_media.stats.audio_frames++;
        s_media.stats.audio_bytes += size;
    }
    return err;
}

bk_err_t bk_media_submit_video(const void *data, size_t size)
{
    bk_err_t err = submit(s_media.adapter.submit_video, data, size);
    if (err == BK_OK) {
        s_media.stats.video_frames++;
        s_media.stats.video_bytes += size;
    }
    return err;
}

bk_err_t bk_media_get_stats(bk_media_stats_t *stats)
{
    if (stats == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (s_media.state == BK_MEDIA_STATE_INITIAL) {
        return BK_ERR_NOT_INITIALIZED;
    }
    *stats = s_media.stats;
    return BK_OK;
}
