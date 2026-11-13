#include <assert.h>
#include <stdio.h>

#include "bk7259_robot.h"
#include "bk_net.h"
#include "bk_board.h"
#include "agora_adapter.h"

static int fail_disconnect;
static int fail_connect;
static bk_err_t net_start(void *context) { (void)context; return BK_OK; }
static bk_err_t net_connect(void *context)
{
    (void)context;
    return fail_connect ? BK_ERR_BUSY : BK_OK;
}
static bk_err_t net_disconnect(void *context)
{
    (void)context;
    return fail_disconnect ? BK_ERR_BUSY : BK_OK;
}

static int adapter_events;
static int fail_adapter_init;
static int fail_adapter_deinit;
static int fail_core_deinit;
static int fail_board_init;
static int fail_board_deinit;
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
static bk_err_t adapter_deinit(void *context)
{
    (void)context;
    if (fail_adapter_deinit) return BK_ERR_BUSY;
    adapter_events |= 4;
    return BK_OK;
}

static bk_err_t hooked_core_deinit(void)
{
    return fail_core_deinit ? BK_ERR_BUSY : bk_core_deinit();
}

static bk_err_t hooked_board_init(const bk_board_config_t *config)
{
    if (fail_board_init) return BK_ERR_INVALID_ARG;
    return bk_board_init(config);
}

static bk_err_t hooked_board_deinit(void)
{
    return fail_board_deinit ? BK_ERR_BUSY : bk_board_deinit();
}

static void test_lifecycle_failure_retry(void)
{
    bk_net_config_t net = {.connect = net_connect, .disconnect = net_disconnect};
    const bk7259_robot_lifecycle_ops_t ops = {
        .core_deinit = hooked_core_deinit,
        .board_init = hooked_board_init,
        .board_deinit = hooked_board_deinit,
    };

    fail_core_deinit = 1;
    fail_board_init = 1;
    bk7259_robot_set_lifecycle_ops(&ops);
    assert(bk7259_robot_start(&net, NULL) == BK_ERR_BUSY);
    assert(bk_board_get_info() == NULL);
    fail_core_deinit = 0;
    assert(bk7259_robot_stop() == BK_OK);
    bk7259_robot_reset_lifecycle_ops();

    assert(bk7259_robot_start(&net, NULL) == BK_OK);
    fail_board_deinit = 1;
    bk7259_robot_set_lifecycle_ops(&ops);
    assert(bk7259_robot_stop() == BK_ERR_BUSY);
    assert(bk_board_get_info() != NULL);
    fail_board_deinit = 0;
    assert(bk7259_robot_stop() == BK_OK);
    bk7259_robot_reset_lifecycle_ops();
}

#if !defined(CONFIG_BK7259_ROBOT_AGORA)
static void test_startup_rollback_with_disconnect_failure(void)
{
    bk_net_config_t net = {
        .start = net_start,
        .connect = net_connect,
        .disconnect = net_disconnect,
    };
    fail_connect = 1;
    fail_disconnect = 1;
    assert(bk7259_robot_start(&net, NULL) == BK_ERR_BUSY);
    assert(bk_net_get_state() == BK_NET_STATE_RETRYING);
    assert(bk_board_get_info() != NULL);
    fail_disconnect = 0;
    assert(bk7259_robot_stop() == BK_OK);
    assert(bk_board_get_info() == NULL);
    fail_connect = 0;
}

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
#endif

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
    fail_adapter_deinit = 1;
    assert(bk7259_robot_stop() == BK_ERR_BUSY);
    assert(bk_board_get_info() != NULL);
    fail_adapter_deinit = 0;
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
    test_lifecycle_failure_retry();
#if defined(CONFIG_BK7259_ROBOT_AGORA) && CONFIG_BK7259_ROBOT_AGORA
    test_plugin_callback_boundary();
#else
    test_startup_rollback_with_disconnect_failure();
    test_no_plugin_boot_and_network();
    test_plugin_callback_boundary();
#endif
    puts("bk7259_robot_product: ok");
    return 0;
}
