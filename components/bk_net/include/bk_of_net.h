/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_NET_H
#define BK_OPEN_FRAMEWORK_BK_NET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_core_event.h"
#include "bk_err.h"
#include "bk_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BK_EVENT_FAMILY_NET       100
#define BK_NET_MAX_SAVED_APS      5U
#define BK_NET_MAX_SSID_LEN       32U
#define BK_NET_MAX_PSK_LEN        64U
#define BK_NET_BACKOFF_MIN_MS     1000U
#define BK_NET_BACKOFF_MAX_MS     30000U

typedef enum {
    BK_NET_STATE_IDLE = 0,
    BK_NET_STATE_UNPROVISIONED,
    BK_NET_STATE_PROVISIONING,
    BK_NET_STATE_CONNECTING,
    BK_NET_STATE_IP_READY,
    BK_NET_STATE_TIME_SYNCED,
    BK_NET_STATE_DISCONNECTED_BACKOFF,
    BK_NET_STATE_AUTH_FAILED,
} bk_net_state_t;

typedef enum {
    BK_NET_EVT_PROV_STARTED = 1,
    BK_NET_EVT_CONNECTING,
    BK_NET_EVT_IP_READY,
    BK_NET_EVT_TIME_SYNCED,
    BK_NET_EVT_DISCONNECTED,
    BK_NET_EVT_AUTH_FAILED,
} bk_net_event_id_t;

typedef struct {
    char ssid[BK_NET_MAX_SSID_LEN + 1U];
    char psk[BK_NET_MAX_PSK_LEN + 1U];
} bk_net_credential_t;

typedef struct {
    bk_event_loop_handle_t event_loop;
    bk_storage_handle_t storage;
    uint32_t sntp_timeout_ms;
} bk_net_config_t;

typedef struct bk_net_mgr *bk_net_handle_t;

bk_err_t bk_net_create(const bk_net_config_t *cfg, bk_net_handle_t *out_net);
void bk_net_destroy(bk_net_handle_t net);

bk_net_state_t bk_net_get_state(bk_net_handle_t net);
size_t bk_net_get_saved_credential_count(bk_net_handle_t net);

bk_err_t bk_net_save_credential(bk_net_handle_t net,
                                const char *ssid,
                                const char *psk);

bk_err_t bk_net_get_credential_by_index(bk_net_handle_t net,
                                        size_t idx,
                                        bk_net_credential_t *out_cred);

/* Provisioning orchestration (BLE / SoftAP) */
bk_err_t bk_net_start_provisioning(bk_net_handle_t net);
bk_err_t bk_net_complete_provisioning(bk_net_handle_t net,
                                      const char *ssid,
                                      const char *psk);

/* Connection lifecycle and deterministic FSM transitions */
bk_err_t bk_net_connect(bk_net_handle_t net);
bk_err_t bk_net_notify_dhcp_bound(bk_net_handle_t net, uint32_t ipv4_addr_be);
bk_err_t bk_net_notify_sntp_synced(bk_net_handle_t net, uint64_t unix_epoch_sec);
bk_err_t bk_net_notify_sntp_timeout(bk_net_handle_t net);
bk_err_t bk_net_notify_link_lost(bk_net_handle_t net, bool auth_failure);

/* Exponential backoff calculation (1s..30s + deterministic jitter 0..250ms) */
uint32_t bk_net_get_next_backoff_ms(bk_net_handle_t net);

/*
 * Production TLS readiness gate:
 * - Rejects verify_cert_chain == false or verify_sni == false with BK_ERR_NET_TLS_VERIFY_FAILED.
 * - Rejects when state != BK_NET_STATE_TIME_SYNCED with BK_ERR_NET_TIME_UNSYNCED.
 */
bk_err_t bk_net_require_tls_ready(bk_net_handle_t net,
                                  bool verify_cert_chain,
                                  bool verify_sni);

bk_err_t bk_net_factory_reset(bk_net_handle_t net);

/* LAN device discovery / echo framing helper (bk_lan.c) */
bk_err_t bk_lan_format_announce(const char *device_id,
                                uint32_t ipv4_addr_be,
                                char *out_json,
                                size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_NET_H */
