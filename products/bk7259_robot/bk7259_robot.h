#ifndef BK7259_ROBOT_PRODUCT_H
#define BK7259_ROBOT_PRODUCT_H

#include "bk_err.h"
#include "bk_net.h"
#ifdef BK7259_ROBOT_TEST_HOOKS
#include "bk_board.h"
#include "bk_core.h"
#endif
#include "agora_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

bk_err_t bk7259_robot_start(const bk_net_config_t *network,
                            const bk7259_robot_agora_adapter_t *agora);
bk_err_t bk7259_robot_stop(void);

#ifdef BK7259_ROBOT_TEST_HOOKS
typedef struct {
    bk_err_t (*core_init)(const bk_core_config_t *config);
    bk_err_t (*core_start)(void);
    bk_err_t (*core_deinit)(void);
    bk_err_t (*board_init)(const bk_board_config_t *config);
    bk_err_t (*board_deinit)(void);
} bk7259_robot_lifecycle_ops_t;

void bk7259_robot_set_lifecycle_ops(const bk7259_robot_lifecycle_ops_t *ops);
void bk7259_robot_reset_lifecycle_ops(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
