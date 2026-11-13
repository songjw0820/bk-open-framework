#ifndef BK_NET_H
#define BK_NET_H

#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BK_NET_STATE_INITIAL = 0,
    BK_NET_STATE_STARTING,
    BK_NET_STATE_CONNECTING,
    BK_NET_STATE_CONNECTED,
    BK_NET_STATE_DISCONNECTED,
    BK_NET_STATE_RETRYING,
    BK_NET_STATE_PROVISIONING,
    BK_NET_STATE_STOPPED,
} bk_net_state_t;

#define BK_NET_STATE_START BK_NET_STATE_STARTING

typedef bk_err_t (*bk_net_action_fn)(void *context);

typedef struct {
    void *context;
    bk_net_action_fn start;
    bk_net_action_fn connect;
    bk_net_action_fn disconnect;
    unsigned max_retries;
} bk_net_config_t;

bk_err_t bk_net_start(const bk_net_config_t *config);
bk_net_state_t bk_net_get_state(void);
bk_err_t bk_net_disconnect(void);
bk_err_t bk_net_retry(void);
bk_err_t bk_net_reprovision(void);
bk_err_t bk_net_notify_connected(void);
bk_err_t bk_net_notify_disconnected(void);
bk_err_t bk_net_stop(void);
bk_err_t bk_net_restart(void);

#ifdef __cplusplus
}
#endif

#endif
