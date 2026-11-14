/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_of_net.h"
#include "bk_port_mem.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_multi_ap_mru_and_factory_reset_wipe(void)
{
    uint8_t flash_buf[8192];
    memset(flash_buf, 0, sizeof(flash_buf));
    bk_storage_config_t st_cfg = {
        .flash_sim_buf = flash_buf,
        .flash_sim_size = sizeof(flash_buf),
    };
    bk_storage_handle_t st = NULL;
    assert(bk_storage_open(&st_cfg, &st) == BK_OK);

    bk_event_loop_config_t ev_cfg = {
        .queue_depth = 16,
        .worker_stack_size = 4096,
        .worker_name = "net_evt",
    };
    bk_event_loop_handle_t loop = NULL;
    assert(bk_event_loop_create(&ev_cfg, &loop) == BK_OK);

    bk_net_config_t net_cfg = {
        .event_loop = loop,
        .storage = st,
        .sntp_timeout_ms = 5000,
    };
    bk_net_handle_t net = NULL;
    assert(bk_net_create(&net_cfg, &net) == BK_OK);
    assert(bk_net_get_state(net) == BK_NET_STATE_UNPROVISIONED);

    /* Save 6 APs: table caps at 5 and keeps MRU at index 0 */
    for (int i = 1; i <= 6; ++i) {
        char ssid[16];
        char psk[16];
        snprintf(ssid, sizeof(ssid), "Office_AP_%d", i);
        snprintf(psk, sizeof(psk), "Pass_%d_Secret", i);
        assert(bk_net_save_credential(net, ssid, psk) == BK_OK);
    }
    assert(bk_net_get_saved_credential_count(net) == BK_NET_MAX_SAVED_APS);

    bk_net_credential_t mru = {0};
    assert(bk_net_get_credential_by_index(net, 0, &mru) == BK_OK);
    assert(strcmp(mru.ssid, "Office_AP_6") == 0);

    /* Factory reset wipes credentials from both RAM and flash */
    assert(bk_net_factory_reset(net) == BK_OK);
    assert(bk_net_get_saved_credential_count(net) == 0U);
    assert(bk_net_get_state(net) == BK_NET_STATE_UNPROVISIONED);
    for (size_t i = 0; i < sizeof(flash_buf); ++i) {
        assert(flash_buf[i] == 0U);
    }

    bk_net_destroy(net);
    bk_event_loop_deinit(loop);
    bk_storage_close(st);
}

static void test_fsm_backoff_and_sntp_tls_gate(void)
{
    bk_net_config_t net_cfg = {0};
    bk_net_handle_t net = NULL;
    assert(bk_net_create(&net_cfg, &net) == BK_OK);

    assert(bk_net_start_provisioning(net) == BK_OK);
    assert(bk_net_get_state(net) == BK_NET_STATE_PROVISIONING);

    assert(bk_net_complete_provisioning(net, "RobotLab_5G", "wpa2_secret") == BK_OK);
    assert(bk_net_get_state(net) == BK_NET_STATE_CONNECTING);

    /* DHCP bound -> IP_READY, but SNTP not yet synced -> TLS gate must reject */
    assert(bk_net_notify_dhcp_bound(net, 0xC0A80164U) == BK_OK);
    assert(bk_net_get_state(net) == BK_NET_STATE_IP_READY);
    assert(bk_net_require_tls_ready(net, true, true) == BK_ERR_NET_TIME_UNSYNCED);

    /* SNTP synced -> TIME_SYNCED -> TLS gate passes ONLY when cert_chain & SNI are enabled */
    assert(bk_net_notify_sntp_synced(net, 1763080000ULL) == BK_OK);
    assert(bk_net_get_state(net) == BK_NET_STATE_TIME_SYNCED);
    assert(bk_net_require_tls_ready(net, false, true) == BK_ERR_NET_TLS_VERIFY_FAILED);
    assert(bk_net_require_tls_ready(net, true, false) == BK_ERR_NET_TLS_VERIFY_FAILED);
    assert(bk_net_require_tls_ready(net, true, true) == BK_OK);

    /* SNTP timeout demotes state back to IP_READY */
    assert(bk_net_notify_sntp_timeout(net) == BK_ERR_NET_TIME_UNSYNCED);
    assert(bk_net_get_state(net) == BK_NET_STATE_IP_READY);

    /* Disconnect exponential backoff progression: 1s..30s */
    uint32_t prev_b = 0;
    for (int i = 0; i < 6; ++i) {
        assert(bk_net_notify_link_lost(net, false) == BK_OK);
        assert(bk_net_get_state(net) == BK_NET_STATE_DISCONNECTED_BACKOFF);
        uint32_t b = bk_net_get_next_backoff_ms(net);
        assert(b >= BK_NET_BACKOFF_MIN_MS && b <= BK_NET_BACKOFF_MAX_MS);
        assert(b >= prev_b);
        prev_b = b;
    }

    /* Auth failure transitions to AUTH_FAILED */
    assert(bk_net_notify_link_lost(net, true) == BK_OK);
    assert(bk_net_get_state(net) == BK_NET_STATE_AUTH_FAILED);

    char announce[128] = {0};
    assert(bk_lan_format_announce("bk7259-dev-01", 0xC0A80164U, announce, sizeof(announce)) == BK_OK);
    assert(strstr(announce, "192.168.1.100") != NULL);

    bk_net_destroy(net);
}

int main(void)
{
    bk_port_reset_mem_stats();
    test_multi_ap_mru_and_factory_reset_wipe();
    test_fsm_backoff_and_sntp_tls_gate();

    bk_port_mem_stats_t mstats;
    bk_port_get_mem_stats(&mstats);
    assert(mstats.current_allocated_bytes == 0U);
    assert(mstats.active_allocations == 0U);

    printf("PASS: test_net_fsm (MRU 5-AP storage, backoff 1s..30s, SNTP/TLS gate, factory reset wipe)\n");
    return 0;
}
