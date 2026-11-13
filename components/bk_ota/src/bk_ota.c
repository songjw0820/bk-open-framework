#include "bk_ota.h"

#include <stddef.h>
#include <string.h>

#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
#include <common/bk_include.h>
#include <components/system.h>
#include <driver/flash_partition.h>
#if defined(CONFIG_OTA_FUNCTION) && defined(CONFIG_OTA_HTTP)
#include <modules/ota.h>
#endif
#endif

static struct {
    bk_ota_state_t state;
    bk_err_t last_error;
    bk_ota_config_t config;
    bk_ota_adapter_t adapter;
} s_ota;

static bk_err_t host_mock_ota_start(void *context, const char *url,
                                    bk_ota_security_mode_t security_mode)
{
    (void)context;
    (void)security_mode;
    if (url == NULL || url[0] == '\0') {
        return BK_ERR_INVALID_ARG;
    }
    if (strncmp(url, "http://", 7U) != 0 && strncmp(url, "https://", 8U) != 0) {
        return BK_ERR_INVALID_ARG;
    }
    return BK_OK;
}

static bk_err_t host_mock_ota_apply(void *context)
{
    (void)context;
    return BK_OK;
}

static const bk_ota_adapter_t k_host_mock_ota_adapter = {
    .start = host_mock_ota_start,
    .apply = host_mock_ota_apply,
    .context = NULL,
};

#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
static bk_err_t armino_ota_start(void *context, const char *url,
                                 bk_ota_security_mode_t security_mode)
{
    (void)context;
    (void)security_mode;
    if (url == NULL || url[0] == '\0') {
        return BK_ERR_INVALID_ARG;
    }
#if defined(CONFIG_OTA_FUNCTION) && defined(CONFIG_OTA_HTTP)
    if (bk_ota_start_download(url, OTA_WR_TO_FLASH) != BK_OK) {
        return BK_ERR_STATE;
    }
    return BK_OK;
#else
    return BK_ERR_NOT_SUPPORTED;
#endif
}

static bk_err_t armino_ota_apply(void *context)
{
    (void)context;
    bk_reboot_ex(RESET_SOURCE_OTA_REBOOT);
    return BK_OK;
}

static const bk_ota_adapter_t k_default_ota_adapter = {
    .start = armino_ota_start,
    .apply = armino_ota_apply,
    .context = NULL,
};
#endif

const bk_ota_adapter_t *bk_ota_default_adapter(void)
{
#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
    return &k_default_ota_adapter;
#else
    return NULL;
#endif
}

const bk_ota_adapter_t *bk_ota_host_mock_adapter(void)
{
    return &k_host_mock_ota_adapter;
}

bk_err_t bk_ota_get_partition_info(bk_ota_partition_info_t *info)
{
    if (info == NULL) {
        return BK_ERR_INVALID_ARG;
    }
#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
    {
        bk_logic_partition_t *pt = bk_flash_partition_get_info(BK_PARTITION_OTA);
        if (pt == NULL || pt->partition_length == 0U) {
            return BK_ERR_NOT_FOUND;
        }
        info->description = pt->partition_description != NULL ? pt->partition_description : "ota";
        info->start_addr = pt->partition_start_addr;
        info->length = pt->partition_length;
        return BK_OK;
    }
#else
    info->description = "ota";
    info->start_addr = 0x00792000U;
    info->length = 264U * 1024U;
    return BK_OK;
#endif
}

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
    adapter = config->adapter != NULL ? config->adapter : bk_ota_default_adapter();
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
        s_ota.last_error = BK_OK;
    } else {
        s_ota.state = BK_OTA_STATE_FAILED;
        s_ota.last_error = result;
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
    s_ota.last_error = error;
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
    if (result == BK_OK) {
        s_ota.state = BK_OTA_STATE_DOWNLOADING;
        s_ota.last_error = BK_OK;
    } else {
        s_ota.last_error = result;
    }
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

bk_err_t bk_ota_get_last_error(void)
{
    return s_ota.last_error;
}
