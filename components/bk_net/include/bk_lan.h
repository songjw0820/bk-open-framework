#ifndef BK_LAN_H
#define BK_LAN_H

#include "bk_net.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *id;
    const char *name;
    const char *address;
    unsigned port;
} bk_device_record_t;

typedef bk_err_t (*bk_lan_publish_fn)(void *context, const bk_device_record_t *record);

typedef struct {
    void *context;
    bk_lan_publish_fn publish;
    bk_net_action_fn stop;
} bk_lan_config_t;

bk_err_t bk_lan_start(const bk_lan_config_t *config);
bk_err_t bk_lan_stop(void);
bk_err_t bk_lan_publish(const bk_device_record_t *record);

#ifdef __cplusplus
}
#endif

#endif
