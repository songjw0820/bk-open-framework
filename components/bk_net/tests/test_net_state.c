#include <assert.h>
#include <string.h>
#include "bk_net.h"
#include "bk_provision.h"
#include "bk_lan.h"

static int connect_calls;
static bk_err_t connect_cb(void *ctx) { (void)ctx; connect_calls++; return BK_OK; }

static void test_lifecycle(void) {
    bk_net_config_t cfg = {0};
    cfg.connect = connect_cb;
    assert(bk_net_get_state() == BK_NET_STATE_INITIAL);
    assert(bk_net_start(&cfg) == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_CONNECTED);
    assert(connect_calls == 1);
    assert(bk_net_disconnect() == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_DISCONNECTED);
    assert(bk_net_retry() == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_CONNECTED);
}

static void test_reprovision_and_lan(void) {
    bk_provision_config_t provision = {0};
    bk_lan_config_t lan = {0};
    bk_device_record_t record = {0};
    record.id = "dev-1";
    record.name = "Robot";
    assert(bk_provision_start(&provision) == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_PROVISIONING);
    assert(bk_provision_start(&provision) == BK_ERR_ALREADY_STARTED);
    assert(bk_net_notify_connected() == BK_OK);
    assert(bk_lan_publish(&record) == BK_ERR_NOT_STARTED);
    assert(bk_lan_start(&lan) == BK_OK);
    assert(bk_lan_publish(&record) == BK_OK);
    assert(bk_lan_publish(NULL) == BK_ERR_INVALID_ARG);
}

static void test_stop_restart_and_invalid(void) {
    assert(bk_net_stop() == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_STOPPED);
    assert(bk_net_disconnect() == BK_ERR_NOT_STARTED);
    assert(bk_net_restart() == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_CONNECTED);
    assert(bk_net_start(NULL) == BK_ERR_INVALID_ARG);
    assert(bk_lan_start(NULL) == BK_ERR_INVALID_ARG);
}

int main(void) {
    test_lifecycle();
    test_reprovision_and_lan();
    test_stop_restart_and_invalid();
    return 0;
}
