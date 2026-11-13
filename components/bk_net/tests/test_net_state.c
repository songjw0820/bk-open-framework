#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "bk_net.h"
#include "bk_provision.h"
#include "bk_lan.h"

static int connect_calls, provision_stops, lan_stops;
static bk_err_t connect_cb(void *ctx) { (void)ctx; connect_calls++; return BK_OK; }
static bk_err_t provision_cb(void *ctx) { (void)ctx; return BK_OK; }
static bk_err_t provision_stop_cb(void *ctx) { (void)ctx; provision_stops++; return BK_OK; }
static bk_err_t lan_publish_cb(void *ctx, const bk_device_record_t *r) { (void)ctx; (void)r; return BK_OK; }
static bk_err_t lan_stop_cb(void *ctx) { (void)ctx; lan_stops++; return BK_OK; }
static bk_err_t lan_publish_reentrant_cb(void *ctx, const bk_device_record_t *r) { (void)ctx; (void)r; return bk_net_disconnect(); }
static bk_err_t provision_reentrant_cb(void *ctx) { (void)ctx; return bk_net_notify_connected(); }

static void test_lifecycle(void) {
    bk_net_config_t cfg = {.connect = connect_cb};
    assert(bk_net_get_state() == BK_NET_STATE_INITIAL);
    assert(bk_net_start(&cfg) == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_CONNECTED);
    assert(connect_calls == 1);
    assert(bk_net_disconnect() == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_DISCONNECTED);
    assert(bk_net_retry() == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_CONNECTED);
    assert(bk_net_retry() == BK_ERR_BUSY);
}

static void test_reprovision_and_lan(void) {
    bk_provision_config_t provision = {.start = provision_cb, .stop = provision_stop_cb};
    bk_lan_config_t lan = {.publish = lan_publish_cb, .stop = lan_stop_cb};
    bk_device_record_t record = {.id = "dev-1", .name = "Robot"};
    assert(bk_provision_start(&provision) == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_PROVISIONING);
    assert(bk_provision_start(&provision) == BK_ERR_ALREADY_STARTED);
    assert(bk_lan_start(&lan) == BK_ERR_NOT_STARTED);
    assert(bk_net_notify_connected() == BK_OK);
    assert(provision_stops == 1);
    assert(bk_lan_start(&lan) == BK_OK);
    assert(bk_lan_publish(&record) == BK_OK);
    assert(bk_net_disconnect() == BK_OK);
    assert(lan_stops == 1);
    assert(bk_lan_publish(&record) == BK_ERR_NOT_STARTED);
    assert(bk_lan_start(&lan) == BK_ERR_NOT_STARTED);
    assert(bk_net_retry() == BK_OK);
    assert(bk_lan_start(&lan) == BK_OK);
    assert(bk_lan_publish(&record) == BK_OK);
    assert(bk_net_reprovision() == BK_OK);
    assert(lan_stops == 2);
    assert(bk_lan_publish(&record) == BK_ERR_NOT_STARTED);
    assert(bk_provision_start(&provision) == BK_OK);
    assert(bk_provision_stop() == BK_OK);
    assert(provision_stops == 2);
}

static void test_stop_restart_and_invalid(void) {
    bk_lan_config_t no_publish = {0};
    bk_device_record_t record = {.id = "dev-1", .name = "Robot"};
    assert(bk_net_stop() == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_STOPPED);
    assert(bk_net_disconnect() == BK_ERR_NOT_STARTED);
    assert(bk_net_restart() == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_CONNECTED);
    assert(bk_lan_start(&no_publish) == BK_OK);
    assert(bk_lan_publish(&record) == BK_ERR_NOT_FOUND);
    assert(bk_net_start(NULL) == BK_ERR_INVALID_ARG);
    assert(bk_lan_start(NULL) == BK_ERR_INVALID_ARG);
    assert(bk_provision_start(NULL) == BK_ERR_INVALID_ARG);
}

static bk_err_t failing_connect(void *ctx) { (void)ctx; return BK_ERR_BUSY; }
static bk_err_t reentrant_connect(void *ctx) { (void)ctx; return bk_net_retry(); }
static bk_err_t reentrant_provision(void *ctx) { (void)ctx; static bk_provision_config_t c; c.start = provision_cb; return bk_provision_start(&c); }
static void test_reentrancy_and_missing_callbacks(void) {
    bk_net_config_t bad = {0};
    bk_net_config_t reentrant = {.connect = reentrant_connect};
    bk_provision_config_t no_start = {0};
    assert(bk_net_stop() == BK_OK);
    assert(bk_net_start(&bad) == BK_ERR_NOT_FOUND);
    assert(bk_net_start(&(bk_net_config_t){.connect = failing_connect, .max_retries = 1}) == BK_ERR_BUSY);
    assert(bk_net_retry() == BK_ERR_BUSY);
    assert(bk_net_retry() == BK_ERR_LIMIT);
    assert(bk_net_stop() == BK_OK);
    assert(bk_net_start(&reentrant) == BK_ERR_REENTRANT);
    assert(bk_net_get_state() == BK_NET_STATE_RETRYING);
    assert(bk_net_stop() == BK_OK);
    assert(bk_net_start(&(bk_net_config_t){.connect = connect_cb}) == BK_OK);
    assert(bk_provision_start(&no_start) == BK_ERR_NOT_FOUND);
    assert(bk_provision_start(&(bk_provision_config_t){.start = reentrant_provision}) == BK_ERR_REENTRANT);
}

static void test_cross_module_reentrancy(void) {
    bk_device_record_t record = {.id = "dev-r", .name = "Robot"};
    bk_lan_config_t lan = {.publish = lan_publish_reentrant_cb};
    assert(bk_lan_start(&lan) == BK_OK);
    assert(bk_lan_publish(&record) == BK_ERR_REENTRANT);
    assert(bk_net_get_state() == BK_NET_STATE_CONNECTED);
    assert(bk_lan_stop() == BK_OK);
    assert(bk_provision_start(&(bk_provision_config_t){.start = provision_reentrant_cb}) == BK_ERR_REENTRANT);
    assert(bk_net_get_state() == BK_NET_STATE_CONNECTED);
}

static void test_credentials_ip_and_host_mocks(void) {
    bk_net_ip_info_t ip_info;
    bk_device_record_t record = {
        .id = "bk7259-min-01",
        .name = "BK7259 Minimal Node",
        .address = "192.168.4.100",
        .port = 52110U,
    };
    const bk_device_record_t *published;

    assert(bk_net_stop() == BK_OK);
    assert(bk_net_get_ip_info(&ip_info) == BK_ERR_NOT_STARTED);
    assert(bk_net_set_credentials(NULL, "pwd") == BK_ERR_INVALID_ARG);
    assert(bk_net_set_credentials("", "pwd") == BK_ERR_INVALID_ARG);
    assert(bk_net_set_credentials("Beken_Lab_WiFi", "12345678") == BK_OK);
    assert(bk_net_get_ssid() != NULL && strcmp(bk_net_get_ssid(), "Beken_Lab_WiFi") == 0);

    assert(bk_net_start(bk_net_host_mock_config()) == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_CONNECTED);
    assert(bk_net_get_ip_info(&ip_info) == BK_OK);
    assert(strcmp(ip_info.ip, "192.168.4.100") == 0);

    assert(bk_provision_start(bk_provision_host_mock_config()) == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_PROVISIONING);
    assert(bk_net_notify_connected() == BK_OK);

    assert(bk_lan_start(bk_lan_host_mock_config()) == BK_OK);
    assert(bk_lan_get_published_record() == NULL);
    assert(bk_lan_publish(&record) == BK_OK);
    published = bk_lan_get_published_record();
    assert(published != NULL && strcmp(published->id, "bk7259-min-01") == 0);
    assert(bk_lan_stop() == BK_OK);
    assert(bk_lan_get_published_record() == NULL);
    assert(bk_net_stop() == BK_OK);
}

static void test_async_connect(void) {
    bk_net_config_t async_cfg = {.connect = connect_cb, .async_connect = true};
    assert(bk_net_start(&async_cfg) == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_CONNECTING);
    assert(bk_net_notify_connected() == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_CONNECTED);
    assert(bk_net_stop() == BK_OK);
}

int main(void) {
    test_lifecycle();
    test_reprovision_and_lan();
    test_stop_restart_and_invalid();
    test_reentrancy_and_missing_callbacks();
    test_cross_module_reentrancy();
    test_credentials_ip_and_host_mocks();
    test_async_connect();
    return 0;
}
