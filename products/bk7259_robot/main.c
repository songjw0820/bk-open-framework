#include "bk7259_robot.h"

#include "bk_board.h"
#include "bk_core.h"

#include <stddef.h>

#if defined(CONFIG_BK7259_ROBOT_AGORA) && CONFIG_BK7259_ROBOT_AGORA
#define BK7259_ROBOT_AGORA_ENABLED 1
#else
#define BK7259_ROBOT_AGORA_ENABLED 0
#endif

typedef struct {
    bk_net_config_t user_network;
    const bk7259_robot_agora_adapter_t *agora;
} robot_runtime_t;

static robot_runtime_t g_runtime;
static int g_started;
static int g_core_initialized;
static int g_board_initialized;
#if BK7259_ROBOT_AGORA_ENABLED
static int g_agora_initialized;
#endif

#ifdef BK7259_ROBOT_TEST_HOOKS
static bk7259_robot_lifecycle_ops_t g_lifecycle_ops;

void bk7259_robot_set_lifecycle_ops(const bk7259_robot_lifecycle_ops_t *ops)
{
    g_lifecycle_ops = ops != NULL ? *ops : (bk7259_robot_lifecycle_ops_t){0};
}

void bk7259_robot_reset_lifecycle_ops(void)
{
    g_lifecycle_ops = (bk7259_robot_lifecycle_ops_t){0};
}
#endif

static bk_err_t lifecycle_core_init(const bk_core_config_t *config)
{
#ifdef BK7259_ROBOT_TEST_HOOKS
    if (g_lifecycle_ops.core_init != NULL) return g_lifecycle_ops.core_init(config);
#endif
    return bk_core_init(config);
}

static bk_err_t lifecycle_core_start(void)
{
#ifdef BK7259_ROBOT_TEST_HOOKS
    if (g_lifecycle_ops.core_start != NULL) return g_lifecycle_ops.core_start();
#endif
    return bk_core_start();
}

static bk_err_t lifecycle_core_deinit(void)
{
#ifdef BK7259_ROBOT_TEST_HOOKS
    if (g_lifecycle_ops.core_deinit != NULL) return g_lifecycle_ops.core_deinit();
#endif
    return bk_core_deinit();
}

static bk_err_t lifecycle_board_init(const bk_board_config_t *config)
{
#ifdef BK7259_ROBOT_TEST_HOOKS
    if (g_lifecycle_ops.board_init != NULL) return g_lifecycle_ops.board_init(config);
#endif
    return bk_board_init(config);
}

static bk_err_t lifecycle_board_deinit(void)
{
#ifdef BK7259_ROBOT_TEST_HOOKS
    if (g_lifecycle_ops.board_deinit != NULL) return g_lifecycle_ops.board_deinit();
#endif
    return bk_board_deinit();
}

static bk_err_t notify_agora(bk_net_state_t state)
{
#if BK7259_ROBOT_AGORA_ENABLED
    if (g_runtime.agora != NULL && g_runtime.agora->on_network_state != NULL) {
        return g_runtime.agora->on_network_state(g_runtime.agora->context, state);
    }
#else
    (void)state;
#endif
    return BK_OK;
}

static bk_err_t network_start(void *context)
{
    robot_runtime_t *runtime = context;
    if (runtime->user_network.start != NULL) {
        return runtime->user_network.start(runtime->user_network.context);
    }
    return BK_OK;
}

static bk_err_t network_connect(void *context)
{
    robot_runtime_t *runtime = context;
    bk_err_t err;
    if (runtime->user_network.connect == NULL) {
        return BK_ERR_NOT_FOUND;
    }
    err = runtime->user_network.connect(runtime->user_network.context);
    if (err != BK_OK) {
        return err;
    }
    return notify_agora(BK_NET_STATE_CONNECTED);
}

static bk_err_t network_disconnect(void *context)
{
    robot_runtime_t *runtime = context;
    bk_err_t err = BK_OK;
    if (runtime->user_network.disconnect != NULL) {
        err = runtime->user_network.disconnect(runtime->user_network.context);
    }
    if (err != BK_OK) {
        return err;
    }
    return notify_agora(BK_NET_STATE_DISCONNECTED);
}

bk_err_t bk7259_robot_start(const bk_net_config_t *network,
                            const bk7259_robot_agora_adapter_t *agora)
{
    bk_board_config_t board = {.board_name = "bk7259_robot"};
    bk_core_config_t core = {.max_subscribers = BK_CORE_DEFAULT_MAX_SUBSCRIBERS};
    bk_net_config_t wrapped_network;
    bk_err_t err;

    if (network == NULL || network->connect == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (g_started) {
        return BK_ERR_ALREADY_STARTED;
    }
#if !BK7259_ROBOT_AGORA_ENABLED
    if (agora != NULL) {
        return BK_ERR_NOT_SUPPORTED;
    }
#else
    if (agora != NULL && agora->init == NULL) {
        return BK_ERR_INVALID_ARG;
    }
#endif

    err = lifecycle_core_init(&core);
    if (err != BK_OK) {
        return err;
    }
    g_core_initialized = 1;
    g_started = 1;
    err = lifecycle_board_init(&board);
    if (err != BK_OK) {
        bk_err_t cleanup_err = bk7259_robot_stop();
        return cleanup_err == BK_OK ? err : cleanup_err;
    }
    g_board_initialized = 1;

    g_runtime.user_network = *network;
    g_runtime.agora = agora;
#if BK7259_ROBOT_AGORA_ENABLED
    if (agora != NULL) {
        err = agora->init(agora->context);
        if (err != BK_OK) {
            bk_err_t cleanup_err = bk7259_robot_stop();
            return cleanup_err == BK_OK ? err : cleanup_err;
        }
        g_agora_initialized = 1;
    }
#else
    (void)agora;
#endif

    err = lifecycle_core_start();
    if (err != BK_OK) {
        bk_err_t cleanup_err = bk7259_robot_stop();
        return cleanup_err == BK_OK ? err : cleanup_err;
    }
    wrapped_network = *network;
    wrapped_network.context = &g_runtime;
    wrapped_network.start = network_start;
    wrapped_network.connect = network_connect;
    wrapped_network.disconnect = network_disconnect;
    err = bk_net_start(&wrapped_network);
    if (err != BK_OK) {
        bk_err_t cleanup_err = bk7259_robot_stop();
        return cleanup_err == BK_OK ? err : cleanup_err;
    }
    return BK_OK;
}

bk_err_t bk7259_robot_stop(void)
{
    bk_err_t err;

    if (!g_started) {
        return BK_ERR_NOT_STARTED;
    }
    if (bk_net_get_state() != BK_NET_STATE_INITIAL &&
        bk_net_get_state() != BK_NET_STATE_STOPPED) {
        err = bk_net_stop();
        if (err != BK_OK) {
            return err;
        }
    }
#if BK7259_ROBOT_AGORA_ENABLED
    if (g_agora_initialized && g_runtime.agora != NULL &&
        g_runtime.agora->deinit != NULL) {
        err = g_runtime.agora->deinit(g_runtime.agora->context);
        if (err != BK_OK) {
            return err;
        }
        g_agora_initialized = 0;
    }
#endif
    if (g_core_initialized) {
        err = lifecycle_core_deinit();
        if (err != BK_OK) {
            return err;
        }
        g_core_initialized = 0;
    }
    if (g_board_initialized) {
        err = lifecycle_board_deinit();
        if (err != BK_OK) {
            return err;
        }
        g_board_initialized = 0;
    }
    g_runtime.agora = NULL;
    g_started = 0;
    return BK_OK;
}
