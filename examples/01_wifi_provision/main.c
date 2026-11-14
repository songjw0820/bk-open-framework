/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_board.h"
#include "bk_core_event.h"
#include "bk_net.h"
#include "bk_storage.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* Verify 01_wifi_provision runs purely on bk7259_minimal without bk_solution_ai */
    assert(bk_board_init(BK_BOARD_PROFILE_MINIMAL) == BK_OK);
    assert(bk_board_require_cap(BK_BOARD_CAP_WIFI_STA, false) == BK_OK);
    assert(bk_board_require_cap(BK_BOARD_CAP_BLE_PROV, false) == BK_OK);

    bk_storage_handle_t storage = NULL;
    assert(bk_storage_open(NULL, &storage) == BK_OK);

    bk_event_loop_config_t ev_cfg = {
        .queue_depth = 8,
        .worker_stack_size = 4096,
        .worker_name = "prov_evt",
    };
    bk_event_loop_handle_t loop = NULL;
    assert(bk_event_loop_create(&ev_cfg, &loop) == BK_OK);

    bk_net_config_t net_cfg = {
        .event_loop = loop,
        .storage = storage,
        .sntp_timeout_ms = 5000,
    };
    bk_net_handle_t net = NULL;
    assert(bk_net_create(&net_cfg, &net) == BK_OK);

    assert(bk_net_start_provisioning(net) == BK_OK);
    assert(bk_net_complete_provisioning(net, "BK7259_Minimal_AP", "MinimalPass123") == BK_OK);
    assert(bk_net_notify_dhcp_bound(net, 0xC0A80A0AU) == BK_OK);
    assert(bk_net_notify_sntp_synced(net, 1763080000ULL) == BK_OK);
    assert(bk_net_require_tls_ready(net, true, true) == BK_OK);
    assert(bk_event_loop_flush(loop, 500) == BK_OK);

    bk_net_destroy(net);
    bk_event_loop_deinit(loop);
    bk_storage_close(storage);
    bk_board_deinit();

    printf("PASS: example 01_wifi_provision (minimal profile, zero bk_solution_ai dependency)\n");
    return 0;
}
