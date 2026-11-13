#include <assert.h>
#include <stdio.h>

#include "bk7259_robot.h"
#include "bk_net.h"
#include "bk_board.h"
#include "agora_adapter.h"

static int fail_disconnect;
static bk_err_t net_start(void *context) { (void)context; return BK_OK; }
static bk_err_t net_connect(void *context) { (void)context; return BK_OK; }
static bk_err_t net_disconnect(void *context)
{
    (void)context;
    return fail_disconnect ? BK_ERR_BUSY : BK_OK;
}

static int adapter_events;
static int fail_adapter_init;
static bk_err_t adapter_init(void *context)
{
    (void)context;
    if (fail_adapter_init) return BK_ERR_BUSY;
    adapter_events |= 1;
    return BK_OK;
}
static bk_err_t adapter_state(void *context, bk_net_state_t state)
{
    (void)context;
    if (state == BK_NET_STATE_CONNECTED) adapter_events |= 2;
    return BK_OK;
}
static bk_err_t adapter_deinit(void *context) { (void)context; adapter_events |= 4; return BK_OK; }

static void test_no_plugin_boot_and_network(void)
{
    bk_net_config_t net = {
        .start = net_start,
        .connect = net_connect,
        .disconnect = net_disconnect,
        .max_retries = 1,
    };
    assert(bk7259_robot_start(&net, NULL) == BK_OK);
    assert(bk_net_get_state() == BK_NET_STATE_CONNECTED);
    fail_disconnect = 1;
    assert(bk7259_robot_stop() == BK_ERR_BUSY);
    assert(bk_net_get_state() == BK_NET_STATE_CONNECTED);
    fail_disconnect = 0;
    assert(bk7259_robot_stop() == BK_OK);
    assert(bk7259_robot_start(&net, NULL) == BK_OK);
    assert(bk7259_robot_stop() == BK_OK);
}

static void test_invalid_network_config(void)
{
    assert(bk7259_robot_start(NULL, NULL) == BK_ERR_INVALID_ARG);
}

static void test_plugin_callback_boundary(void)
{
    bk_net_config_t net = {
        .start = net_start,
        .connect = net_connect,
        .disconnect = net_disconnect,
    };
    const bk7259_robot_agora_adapter_t adapter = {
        .context = NULL,
        .init = adapter_init,
        .on_network_state = adapter_state,
        .deinit = adapter_deinit,
    };
    adapter_events = 0;
#if defined(CONFIG_BK7259_ROBOT_AGORA) && CONFIG_BK7259_ROBOT_AGORA
    fail_adapter_init = 1;
    assert(bk7259_robot_start(&net, &adapter) == BK_ERR_BUSY);
    assert(bk_board_get_info() == NULL);
    fail_adapter_init = 0;
    assert(bk7259_robot_start(&net, &adapter) == BK_OK);
    assert(adapter_events == 3);
    assert(bk7259_robot_stop() == BK_OK);
    assert(adapter_events == 7);
#else
    assert(bk7259_robot_start(&net, &adapter) == BK_ERR_NOT_SUPPORTED);
    assert(adapter_events == 0);
#endif
}

int main(void)
{
    test_invalid_network_config();
#if defined(CONFIG_BK7259_ROBOT_AGORA) && CONFIG_BK7259_ROBOT_AGORA
    test_plugin_callback_boundary();
#else
    test_no_plugin_boot_and_network();
    test_plugin_callback_boundary();
#endif
    puts("bk7259_robot_product: ok");
    return 0;
}
