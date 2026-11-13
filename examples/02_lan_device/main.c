#include "bk_board.h"
#include "bk_core.h"
#include "bk_lan.h"
#include "bk_net.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    bk_core_config_t core_cfg = {.max_subscribers = BK_CORE_DEFAULT_MAX_SUBSCRIBERS};
    bk_board_config_t board_cfg = {.board_name = "bk7259_robot"};
    const bk_net_config_t *net_cfg;
    const bk_lan_config_t *lan_cfg;
    bk_net_ip_info_t ip_info = {0};
    bk_device_record_t record = {
        .id = "bk7259-node-01",
        .name = "bk7259-lan-device",
        .address = "192.168.4.100",
        .port = 8266,
    };
    const bk_device_record_t *published;

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
    lan_cfg = bk_lan_default_config();
    if (lan_cfg == NULL) {
        lan_cfg = bk_lan_host_mock_config();
    }

    if (bk_net_start(net_cfg) != BK_OK ||
        bk_net_get_state() != BK_NET_STATE_CONNECTED) {
        return 3;
    }
    if (bk_net_get_ip_info(&ip_info) == BK_OK && strlen(ip_info.ip) > 0) {
        record.address = ip_info.ip;
    }

    if (bk_lan_start(lan_cfg) != BK_OK) {
        return 4;
    }
    if (bk_lan_publish(&record) != BK_OK) {
        return 5;
    }
    published = bk_lan_get_published_record();
    if (published == NULL ||
        strcmp(published->id, record.id) != 0 ||
        published->port != record.port) {
        return 6;
    }

    printf("02_lan_device: published id=%s name=%s addr=%s:%u\n",
           published->id,
           published->name,
           published->address,
           published->port);

    if (bk_lan_stop() != BK_OK || bk_net_stop() != BK_OK ||
        bk_board_deinit() != BK_OK || bk_core_deinit() != BK_OK) {
        return 7;
    }
    return 0;
}
