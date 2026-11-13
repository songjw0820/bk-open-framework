#ifndef BK7259_ROBOT_PRODUCT_H
#define BK7259_ROBOT_PRODUCT_H

#include "bk_err.h"
#include "bk_net.h"
#include "agora_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

bk_err_t bk7259_robot_start(const bk_net_config_t *network,
                            const bk7259_robot_agora_adapter_t *agora);
bk_err_t bk7259_robot_stop(void);

#ifdef __cplusplus
}
#endif

#endif
