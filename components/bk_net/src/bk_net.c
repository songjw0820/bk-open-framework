/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_of_net.h"
#include "bk_port_mem.h"
#include "bk_port_os.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t count;
    bk_net_credential_t creds[BK_NET_MAX_SAVED_APS];
} bk_net_cred_table_t;

struct bk_net_mgr {
    bk_port_mutex_t lock;
    bk_event_loop_handle_t event_loop;
    bk_storage_handle_t storage;
    bk_net_state_t state;
    uint32_t retry_attempt;
    uint32_t ipv4_addr_be;
    uint64_t synced_epoch_sec;
    bk_net_cred_table_t table;
};

static void post_net_evt(struct bk_net_mgr *net, bk_net_event_id_t evt_id)
{
    if (net->event_loop != NULL) {
        int32_t st = (int32_t)net->state;
        (void)bk_of_event_post(
            net->event_loop, BK_EVENT_FAMILY_NET, (int32_t)evt_id, &st, sizeof(st), 10);
    }
}

static bk_err_t persist_cred_table(struct bk_net_mgr *net)
{
    if (net->storage == NULL) {
        return BK_OK;
    }
    return bk_storage_set_blob(net->storage,
                               "wifi",
                               "cred_table",
                               &net->table,
                               sizeof(net->table),
                               BK_STORAGE_FLAG_SENSITIVE);
}

bk_err_t bk_net_create(const bk_net_config_t *cfg, bk_net_handle_t *out_net)
{
    if (cfg == NULL || out_net == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    struct bk_net_mgr *net = (struct bk_net_mgr *)bk_port_calloc(
        1, sizeof(*net), BK_MEM_CAP_DEFAULT, "bk_net");
    if (net == NULL) {
        return BK_ERR_NO_MEM;
    }

    bk_err_t err = bk_port_mutex_create(true, &net->lock);
    if (err != BK_OK) {
        bk_port_free(net);
        return err;
    }

    net->event_loop = cfg->event_loop;
    net->storage = cfg->storage;
    net->state = BK_NET_STATE_UNPROVISIONED;

    if (net->storage != NULL) {
        size_t len = sizeof(net->table);
        uint32_t flags = 0;
        if (bk_storage_get_blob(
                net->storage, "wifi", "cred_table", &net->table, &len, &flags) == BK_OK &&
            len == sizeof(net->table) && net->table.count > 0 &&
            net->table.count <= BK_NET_MAX_SAVED_APS) {
            net->state = BK_NET_STATE_IDLE;
        } else {
            memset(&net->table, 0, sizeof(net->table));
        }
    }

    *out_net = net;
    return BK_OK;
}

void bk_net_destroy(bk_net_handle_t net)
{
    if (net == NULL) {
        return;
    }
    if (net->lock != NULL) {
        bk_port_mutex_destroy(net->lock);
    }
    memset(&net->table, 0, sizeof(net->table));
    bk_port_free(net);
}

bk_net_state_t bk_net_get_state(bk_net_handle_t net)
{
    if (net == NULL) {
        return BK_NET_STATE_IDLE;
    }
    bk_port_mutex_lock(net->lock, UINT32_MAX);
    bk_net_state_t s = net->state;
    bk_port_mutex_unlock(net->lock);
    return s;
}

size_t bk_net_get_saved_credential_count(bk_net_handle_t net)
{
    if (net == NULL) {
        return 0U;
    }
    bk_port_mutex_lock(net->lock, UINT32_MAX);
    size_t c = net->table.count;
    bk_port_mutex_unlock(net->lock);
    return c;
}

bk_err_t bk_net_save_credential(bk_net_handle_t net,
                                const char *ssid,
                                const char *psk)
{
    if (net == NULL || ssid == NULL || psk == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    size_t ssid_len = strlen(ssid);
    size_t psk_len = strlen(psk);
    if (ssid_len == 0 || ssid_len > BK_NET_MAX_SSID_LEN || psk_len > BK_NET_MAX_PSK_LEN) {
        return BK_ERR_INVALID_ARG;
    }

    bk_port_mutex_lock(net->lock, UINT32_MAX);
    /* Check if SSID already exists; if so update and move to slot 0 (MRU) */
    int existing_idx = -1;
    for (uint32_t i = 0; i < net->table.count; ++i) {
        if (strcmp(net->table.creds[i].ssid, ssid) == 0) {
            existing_idx = (int)i;
            break;
        }
    }

    if (existing_idx >= 0) {
        for (int i = existing_idx; i > 0; --i) {
            net->table.creds[i] = net->table.creds[i - 1];
        }
    } else {
        uint32_t move_count = net->table.count < BK_NET_MAX_SAVED_APS
                                  ? net->table.count
                                  : (BK_NET_MAX_SAVED_APS - 1U);
        for (uint32_t i = move_count; i > 0; --i) {
            net->table.creds[i] = net->table.creds[i - 1U];
        }
        if (net->table.count < BK_NET_MAX_SAVED_APS) {
            net->table.count += 1U;
        }
    }

    memset(&net->table.creds[0], 0, sizeof(net->table.creds[0]));
    strncpy(net->table.creds[0].ssid, ssid, BK_NET_MAX_SSID_LEN);
    strncpy(net->table.creds[0].psk, psk, BK_NET_MAX_PSK_LEN);

    bk_err_t err = persist_cred_table(net);
    if (err == BK_OK && net->state == BK_NET_STATE_UNPROVISIONED) {
        net->state = BK_NET_STATE_IDLE;
    }
    bk_port_mutex_unlock(net->lock);
    return err;
}

bk_err_t bk_net_get_credential_by_index(bk_net_handle_t net,
                                        size_t idx,
                                        bk_net_credential_t *out_cred)
{
    if (net == NULL || out_cred == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_port_mutex_lock(net->lock, UINT32_MAX);
    if (idx >= net->table.count) {
        bk_port_mutex_unlock(net->lock);
        return BK_ERR_NOT_FOUND;
    }
    *out_cred = net->table.creds[idx];
    bk_port_mutex_unlock(net->lock);
    return BK_OK;
}

bk_err_t bk_net_connect(bk_net_handle_t net)
{
    if (net == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_port_mutex_lock(net->lock, UINT32_MAX);
    if (net->table.count == 0U) {
        bk_port_mutex_unlock(net->lock);
        return BK_ERR_INVALID_STATE;
    }
    net->state = BK_NET_STATE_CONNECTING;
    post_net_evt(net, BK_NET_EVT_CONNECTING);
    bk_port_mutex_unlock(net->lock);
    return BK_OK;
}

bk_err_t bk_net_notify_dhcp_bound(bk_net_handle_t net, uint32_t ipv4_addr_be)
{
    if (net == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_port_mutex_lock(net->lock, UINT32_MAX);
    net->ipv4_addr_be = ipv4_addr_be;
    net->retry_attempt = 0U;
    net->state = BK_NET_STATE_IP_READY;
    post_net_evt(net, BK_NET_EVT_IP_READY);
    bk_port_mutex_unlock(net->lock);
    return BK_OK;
}

bk_err_t bk_net_notify_sntp_synced(bk_net_handle_t net, uint64_t unix_epoch_sec)
{
    if (net == NULL || unix_epoch_sec == 0ULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_port_mutex_lock(net->lock, UINT32_MAX);
    if (net->state != BK_NET_STATE_IP_READY && net->state != BK_NET_STATE_TIME_SYNCED) {
        bk_port_mutex_unlock(net->lock);
        return BK_ERR_INVALID_STATE;
    }
    net->synced_epoch_sec = unix_epoch_sec;
    net->state = BK_NET_STATE_TIME_SYNCED;
    post_net_evt(net, BK_NET_EVT_TIME_SYNCED);
    bk_port_mutex_unlock(net->lock);
    return BK_OK;
}

bk_err_t bk_net_notify_sntp_timeout(bk_net_handle_t net)
{
    if (net == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_port_mutex_lock(net->lock, UINT32_MAX);
    if (net->state == BK_NET_STATE_TIME_SYNCED) {
        net->state = BK_NET_STATE_IP_READY;
    }
    bk_port_mutex_unlock(net->lock);
    return BK_ERR_NET_TIME_UNSYNCED;
}

bk_err_t bk_net_notify_link_lost(bk_net_handle_t net, bool auth_failure)
{
    if (net == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_port_mutex_lock(net->lock, UINT32_MAX);
    if (auth_failure) {
        net->state = BK_NET_STATE_AUTH_FAILED;
        post_net_evt(net, BK_NET_EVT_AUTH_FAILED);
    } else {
        net->state = BK_NET_STATE_DISCONNECTED_BACKOFF;
        if (net->retry_attempt < 16U) {
            net->retry_attempt += 1U;
        }
        post_net_evt(net, BK_NET_EVT_DISCONNECTED);
    }
    bk_port_mutex_unlock(net->lock);
    return BK_OK;
}

uint32_t bk_net_get_next_backoff_ms(bk_net_handle_t net)
{
    if (net == NULL) {
        return BK_NET_BACKOFF_MIN_MS;
    }
    bk_port_mutex_lock(net->lock, UINT32_MAX);
    uint32_t shift = net->retry_attempt > 0U ? (net->retry_attempt - 1U) : 0U;
    if (shift > 5U) {
        shift = 5U;
    }
    uint32_t base = BK_NET_BACKOFF_MIN_MS << shift;
    if (base > BK_NET_BACKOFF_MAX_MS) {
        base = BK_NET_BACKOFF_MAX_MS;
    }
    uint32_t jitter = (net->retry_attempt * 73U) % 250U;
    uint32_t total = base + jitter;
    if (total > BK_NET_BACKOFF_MAX_MS) {
        total = BK_NET_BACKOFF_MAX_MS;
    }
    bk_port_mutex_unlock(net->lock);
    return total;
}

bk_err_t bk_net_require_tls_ready(bk_net_handle_t net,
                                  bool verify_cert_chain,
                                  bool verify_sni)
{
    if (net == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (!verify_cert_chain || !verify_sni) {
        return BK_ERR_NET_TLS_VERIFY_FAILED;
    }
    bk_port_mutex_lock(net->lock, UINT32_MAX);
    bk_net_state_t st = net->state;
    bk_port_mutex_unlock(net->lock);

    if (st != BK_NET_STATE_TIME_SYNCED) {
        return BK_ERR_NET_TIME_UNSYNCED;
    }
    return BK_OK;
}

bk_err_t bk_net_factory_reset(bk_net_handle_t net)
{
    if (net == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_port_mutex_lock(net->lock, UINT32_MAX);
    memset(&net->table, 0, sizeof(net->table));
    net->retry_attempt = 0U;
    net->ipv4_addr_be = 0U;
    net->synced_epoch_sec = 0ULL;
    net->state = BK_NET_STATE_UNPROVISIONED;
    if (net->storage != NULL) {
        (void)bk_storage_erase_ns(net->storage, "wifi");
    }
    bk_port_mutex_unlock(net->lock);
    return BK_OK;
}

/* Internal helpers exposed to bk_provision.c */
bk_err_t bk_net_internal_set_provisioning(bk_net_handle_t net)
{
    if (net == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_port_mutex_lock(net->lock, UINT32_MAX);
    net->state = BK_NET_STATE_PROVISIONING;
    post_net_evt(net, BK_NET_EVT_PROV_STARTED);
    bk_port_mutex_unlock(net->lock);
    return BK_OK;
}
