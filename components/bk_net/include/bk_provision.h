#ifndef BK_PROVISION_H
#define BK_PROVISION_H

#include "bk_net.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *context;
    bk_net_action_fn start;
    bk_net_action_fn stop;
} bk_provision_config_t;

bk_err_t bk_provision_start(const bk_provision_config_t *config);
bk_err_t bk_provision_stop(void);

#ifdef __cplusplus
}
#endif

#endif
