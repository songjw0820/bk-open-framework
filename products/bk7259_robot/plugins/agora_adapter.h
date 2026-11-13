#ifndef BK7259_ROBOT_AGORA_ADAPTER_H
#define BK7259_ROBOT_AGORA_ADAPTER_H

#include "bk_err.h"
#include "bk_net.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Optional adapter boundary for Agora or another realtime service.
 * The framework never includes a vendor header and never owns this object.
 */
typedef struct {
    void *context;
    bk_err_t (*init)(void *context);
    bk_err_t (*on_network_state)(void *context, bk_net_state_t state);
    bk_err_t (*deinit)(void *context);
} bk7259_robot_agora_adapter_t;

#ifdef __cplusplus
}
#endif

#endif
