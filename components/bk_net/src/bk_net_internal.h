#ifndef BK_NET_INTERNAL_H
#define BK_NET_INTERNAL_H

#include "bk_err.h"

bk_err_t bk_net_callback_enter(void);
void bk_net_callback_leave(void);
int bk_net_callback_active(void);
void bk_net_lock(void);
void bk_net_unlock(void);

bk_err_t bk_provision_reset_for_net_connected(void);
void bk_provision_reset_for_net_stop(void);
void bk_lan_reset_for_net_stop(void);

#endif
