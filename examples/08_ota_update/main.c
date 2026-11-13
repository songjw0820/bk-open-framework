#include "bk_board.h"
#include "bk_core.h"
#include "bk_net.h"
#include "bk_ota.h"

#include <stdio.h>

int main(void)
{
    bk_core_config_t core_cfg = {.max_subscribers = BK_CORE_DEFAULT_MAX_SUBSCRIBERS};
    bk_board_config_t board_cfg = {.board_name = "bk7259_robot"};
    const bk_net_config_t *net_cfg;
    const bk_ota_adapter_t *ota_adapter;
    bk_ota_partition_info_t part_info = {0};

    if (bk_core_init(&core_cfg) != BK_OK || bk_core_start() != BK_OK) {
        return 1;
    }
    if (bk_board_init(&board_cfg) != BK_OK) {
        return 2;
    }

    net_cfg = bk_net_default_config();
    if (net_cfg == NULL) {
        net_cfg = bk_net_host_mock_config();
    }
    if (bk_net_start(net_cfg) != BK_OK ||
        bk_net_get_state() != BK_NET_STATE_CONNECTED) {
        return 3;
    }

    if (bk_ota_get_partition_info(&part_info) != BK_OK || part_info.length == 0) {
        return 4;
    }

    ota_adapter = bk_ota_default_adapter();
    if (ota_adapter == NULL) {
        ota_adapter = bk_ota_host_mock_adapter();
    }

    bk_ota_config_t invalid_cfg = {
        .url = "ftp://invalid.example.com/fw.rbl",
        .security_mode = BK_OTA_SECURITY_SDK,
        .adapter = ota_adapter,
    };
    bk_ota_config_t valid_cfg = {
        .url = "https://ota.example.com/bk7259/v4.0.1.11/app_pack.rbl",
        .security_mode = BK_OTA_SECURITY_SDK,
        .adapter = ota_adapter,
    };

    bk_ota_reset();
    if (bk_ota_start(&invalid_cfg) != BK_ERR_INVALID_ARG) {
        return 5;
    }

    if (bk_ota_start(&valid_cfg) != BK_OK ||
        bk_ota_get_state() != BK_OTA_STATE_DOWNLOADING) {
        return 6;
    }

    if (bk_ota_fail(BK_ERR_BUSY) != BK_ERR_BUSY ||
        bk_ota_get_state() != BK_OTA_STATE_FAILED ||
        bk_ota_get_last_error() != BK_ERR_BUSY) {
        return 7;
    }

    if (bk_ota_retry() != BK_OK ||
        bk_ota_get_state() != BK_OTA_STATE_DOWNLOADING) {
        return 8;
    }

    if (bk_ota_finish() != BK_OK ||
        bk_ota_get_state() != BK_OTA_STATE_DOWNLOADED) {
        return 9;
    }

    if (bk_ota_apply() != BK_OK ||
        bk_ota_get_state() != BK_OTA_STATE_APPLYING) {
        return 10;
    }

    printf("08_ota_update: partition=%s start=0x%08x len=0x%08x state=%d\n",
           part_info.description,
           (unsigned)part_info.start_addr,
           (unsigned)part_info.length,
           (int)bk_ota_get_state());

    bk_ota_reset();
    if (bk_net_stop() != BK_OK || bk_board_deinit() != BK_OK || bk_core_deinit() != BK_OK) {
        return 11;
    }
    return 0;
}
