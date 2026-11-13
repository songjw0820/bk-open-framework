#include <assert.h>
#include <stdio.h>
#include "bk_ota.h"

static int start_calls;
static int apply_calls;
static bk_err_t adapter_result;
static bk_err_t apply_result;

static bk_err_t ota_start(void *context, const char *url, bk_ota_security_mode_t security_mode)
{
    (void)context;
    assert(url != NULL);
    assert(security_mode == BK_OTA_SECURITY_NONE);
    start_calls++;
    return adapter_result;
}

static bk_err_t ota_apply(void *context)
{
    (void)context;
    apply_calls++;
    return apply_result;
}

static const bk_ota_adapter_t adapter = {.start = ota_start, .apply = ota_apply, .context = NULL};
static const bk_ota_adapter_t no_start_adapter = {.start = NULL, .apply = NULL, .context = NULL};

static bk_ota_config_t config(void)
{
    bk_ota_config_t value = {
        .url = "https://example.invalid/firmware.bin",
        .security_mode = BK_OTA_SECURITY_NONE,
        .adapter = &adapter,
    };
    return value;
}

int main(void)
{
    bk_ota_config_t cfg = config();

    assert(bk_ota_get_state() == BK_OTA_STATE_IDLE);
    assert(bk_ota_start(NULL) == BK_ERR_INVALID_ARG);
    cfg.url = "";
    assert(bk_ota_start(&cfg) == BK_ERR_INVALID_ARG);
    cfg = config();
    cfg.adapter = NULL;
    assert(bk_ota_start(&cfg) == BK_ERR_NOT_FOUND);
    cfg.adapter = &no_start_adapter;
    assert(bk_ota_start(&cfg) == BK_ERR_NOT_SUPPORTED);
    cfg = config();
    cfg.security_mode = BK_OTA_SECURITY_SIGNED;
    assert(bk_ota_start(&cfg) == BK_ERR_NOT_SUPPORTED);

    cfg = config();
    adapter_result = BK_OK;
    assert(bk_ota_start(&cfg) == BK_OK);
    assert(start_calls == 1);
    assert(bk_ota_get_state() == BK_OTA_STATE_DOWNLOADING);
    assert(bk_ota_start(&cfg) == BK_ERR_BUSY);
    assert(bk_ota_finish() == BK_OK);
    assert(bk_ota_get_state() == BK_OTA_STATE_READY);
    assert(bk_ota_finish() == BK_ERR_BUSY);
    assert(bk_ota_apply() == BK_OK);
    assert(apply_calls == 1);
    assert(bk_ota_get_state() == BK_OTA_STATE_APPLYING);
    assert(bk_ota_reset() == BK_ERR_BUSY);
    assert(bk_ota_fail(BK_ERR_BUSY) == BK_ERR_BUSY);
    assert(bk_ota_get_state() == BK_OTA_STATE_FAILED);
    assert(bk_ota_fail(BK_ERR_NOT_FOUND) == BK_ERR_BUSY);
    assert(bk_ota_get_state() == BK_OTA_STATE_FAILED);

    assert(bk_ota_retry() == BK_OK);
    assert(start_calls == 2);
    assert(bk_ota_get_state() == BK_OTA_STATE_DOWNLOADING);
    assert(bk_ota_reset() == BK_OK);
    assert(bk_ota_get_state() == BK_OTA_STATE_IDLE);

    adapter_result = BK_ERR_BUSY;
    assert(bk_ota_start(&cfg) == BK_ERR_BUSY);
    assert(start_calls == 3);
    assert(bk_ota_get_state() == BK_OTA_STATE_FAILED);
    assert(bk_ota_retry() == BK_ERR_BUSY);
    assert(start_calls == 4);
    assert(bk_ota_get_state() == BK_OTA_STATE_FAILED);
    adapter_result = BK_OK;
    assert(bk_ota_retry() == BK_OK);
    assert(start_calls == 5);
    assert(bk_ota_get_state() == BK_OTA_STATE_DOWNLOADING);

    assert(bk_ota_fail(BK_ERR_NOT_FOUND) == BK_ERR_NOT_FOUND);
    assert(bk_ota_get_state() == BK_OTA_STATE_FAILED);
    assert(bk_ota_get_last_error() == BK_ERR_NOT_FOUND);
    adapter_result = BK_OK;
    assert(bk_ota_start(&cfg) == BK_OK); /* direct start from FAILED is explicit recovery */
    assert(start_calls == 6);
    assert(bk_ota_get_state() == BK_OTA_STATE_DOWNLOADING);
    assert(bk_ota_get_last_error() == BK_OK);
    assert(bk_ota_finish() == BK_OK);
    assert(bk_ota_get_state() == BK_OTA_STATE_DOWNLOADED);
    assert(bk_ota_reset() == BK_OK);

    {
        bk_ota_partition_info_t part = {0};
        bk_ota_config_t mock_cfg = {
            .url = "https://ota.beken.example/app_pack.rbl",
            .security_mode = BK_OTA_SECURITY_SDK,
            .adapter = bk_ota_host_mock_adapter(),
        };
        assert(bk_ota_get_partition_info(&part) == BK_OK);
        assert(part.length > 0U);
        assert(bk_ota_start(&mock_cfg) == BK_OK);
        assert(bk_ota_finish() == BK_OK);
        assert(bk_ota_apply() == BK_OK);
    }

    puts("bk_ota_state: ok");
    return 0;
}
