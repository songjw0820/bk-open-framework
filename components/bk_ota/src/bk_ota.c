#include "bk_ota.h"

#include <stddef.h>
#include <string.h>

static struct {
    bk_ota_state_t state;
    bk_ota_config_t config;
    bk_ota_adapter_t adapter;
} s_ota;

static int supported_security_mode(bk_ota_security_mode_t mode)
{
    return mode == BK_OTA_SECURITY_NONE || mode == BK_OTA_SECURITY_SDK;
}

bk_err_t bk_ota_start(const bk_ota_config_t *config)
{
    bk_err_t result;
    const bk_ota_adapter_t *adapter;

    if (config == NULL || config->url == NULL || config->url[0] == '\0') {
        return BK_ERR_INVALID_ARG;
    }
    if (!supported_security_mode(config->security_mode)) {
        return BK_ERR_NOT_SUPPORTED;
    }
    adapter = config->adapter;
    if (adapter == NULL) {
        return BK_ERR_NOT_FOUND;
    }
    if (adapter->start == NULL) {
        return BK_ERR_NOT_SUPPORTED;
    }
    if (s_ota.state != BK_OTA_STATE_IDLE && s_ota.state != BK_OTA_STATE_FAILED) {
        return BK_ERR_BUSY;
    }

    /* The SDK adapter is the only implementation of the OTA operation. */
    s_ota.config = *config;
    s_ota.adapter = *adapter;
    result = adapter->start(adapter->context, config->url, config->security_mode);
    if (result == BK_OK) {
        s_ota.state = BK_OTA_STATE_DOWNLOADING;
    } else {
        s_ota.state = BK_OTA_STATE_FAILED;
    }
    return result;
}

bk_err_t bk_ota_finish(void)
{
    if (s_ota.state == BK_OTA_STATE_IDLE) return BK_ERR_NOT_INITIALIZED;
    if (s_ota.state != BK_OTA_STATE_DOWNLOADING) return BK_ERR_BUSY;
    s_ota.state = BK_OTA_STATE_READY;
    return BK_OK;
}

bk_err_t bk_ota_apply(void)
{
    bk_err_t result;
    if (s_ota.state == BK_OTA_STATE_IDLE) return BK_ERR_NOT_INITIALIZED;
    if (s_ota.state != BK_OTA_STATE_READY) return BK_ERR_BUSY;
    if (s_ota.adapter.apply == NULL) return BK_ERR_NOT_SUPPORTED;
    result = s_ota.adapter.apply(s_ota.adapter.context);
    if (result == BK_OK) s_ota.state = BK_OTA_STATE_APPLYING;
    return result;
}

bk_err_t bk_ota_fail(bk_err_t error)
{
    if (s_ota.state == BK_OTA_STATE_IDLE) return BK_ERR_NOT_INITIALIZED;
    if (s_ota.state == BK_OTA_STATE_FAILED) return BK_ERR_BUSY;
    if (error == BK_OK) error = BK_ERR_BUSY;
    s_ota.state = BK_OTA_STATE_FAILED;
    return error;
}

bk_err_t bk_ota_retry(void)
{
    bk_err_t result;
    if (s_ota.state != BK_OTA_STATE_FAILED) return BK_ERR_BUSY;
    if (s_ota.adapter.start == NULL || s_ota.config.url == NULL || s_ota.config.url[0] == '\0') {
        return BK_ERR_NOT_SUPPORTED;
    }
    result = s_ota.adapter.start(s_ota.adapter.context, s_ota.config.url,
                                  s_ota.config.security_mode);
    if (result == BK_OK) s_ota.state = BK_OTA_STATE_DOWNLOADING;
    return result;
}

bk_err_t bk_ota_reset(void)
{
    if (s_ota.state == BK_OTA_STATE_APPLYING) return BK_ERR_BUSY;
    memset(&s_ota, 0, sizeof(s_ota));
    return BK_OK;
}

bk_ota_state_t bk_ota_get_state(void)
{
    return s_ota.state;
}
