#include "bk_board.h"
#include "bk_core.h"
#include "bk_event.h"
#include "bk_net.h"
#include "bk_provision.h"

#include <stdio.h>
#include <string.h>

#define EXAMPLE_EVENT_PROVISIONED 0x1001U

typedef struct {
    int network_events;
    bk_net_state_t last_state;
} provision_observer_t;

static void on_network_event(const bk_event_t *event, void *ctx)
{
    provision_observer_t *obs = (provision_observer_t *)ctx;
    if (obs != NULL && event != NULL) {
        obs->network_events++;
        if (event->data != NULL && event->size == sizeof(bk_net_state_t)) {
            obs->last_state = *(const bk_net_state_t *)event->data;
        }
    }
}

int main(void)
{
    bk_core_config_t core_cfg = {.max_subscribers = BK_CORE_DEFAULT_MAX_SUBSCRIBERS};
    bk_board_config_t board_cfg = {.board_name = "bk7259_robot"};
    provision_observer_t observer = {0};
    bk_net_ip_info_t ip_info = {0};
    const bk_net_config_t *net_cfg;
    const bk_provision_config_t *prov_cfg;
    bk_net_state_t connected_state = BK_NET_STATE_CONNECTED;
    bk_event_t ev = {
        .id = EXAMPLE_EVENT_PROVISIONED,
        .data = &connected_state,
        .size = sizeof(connected_state),
    };

    if (bk_core_init(&core_cfg) != BK_OK || bk_core_start() != BK_OK) {
        return 1;
    }
    if (bk_board_init(&board_cfg) != BK_OK) {
        return 2;
    }
    if (bk_event_subscribe(EXAMPLE_EVENT_PROVISIONED, on_network_event, &observer) != BK_OK) {
        return 3;
    }

    if (bk_net_set_credentials("BK7259-OpenAP", "openframework123") != BK_OK) {
        return 4;
    }

    net_cfg = bk_net_default_config();
    if (net_cfg == NULL) {
        net_cfg = bk_net_host_mock_config();
    }
    if (bk_net_start(net_cfg) != BK_OK) {
        return 5;
    }

    prov_cfg = bk_provision_default_config();
    if (prov_cfg == NULL) {
        prov_cfg = bk_provision_host_mock_config();
    }
    if (bk_provision_start(prov_cfg) != BK_OK ||
        bk_net_get_state() != BK_NET_STATE_PROVISIONING) {
        return 6;
    }

    if (bk_net_notify_connected() != BK_OK ||
        bk_net_get_state() != BK_NET_STATE_CONNECTED) {
        return 7;
    }
    if (bk_event_publish(&ev) != BK_OK || observer.network_events != 1) {
        return 8;
    }
    if (bk_net_get_ip_info(&ip_info) != BK_OK || strlen(ip_info.ip) == 0) {
        return 9;
    }

    printf("01_wifi_provision: connected ssid=%s ip=%s events=%d\n",
           bk_net_get_ssid(),
           ip_info.ip,
           observer.network_events);

    if (bk_net_stop() != BK_OK || bk_board_deinit() != BK_OK || bk_core_deinit() != BK_OK) {
        return 10;
    }
    return 0;
}
