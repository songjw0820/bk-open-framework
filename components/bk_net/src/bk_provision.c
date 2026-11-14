/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_of_net.h"

extern bk_err_t bk_net_internal_set_provisioning(bk_net_handle_t net);

bk_err_t bk_net_start_provisioning(bk_net_handle_t net)
{
    return bk_net_internal_set_provisioning(net);
}

bk_err_t bk_net_complete_provisioning(bk_net_handle_t net,
                                      const char *ssid,
                                      const char *psk)
{
    bk_err_t err = bk_net_save_credential(net, ssid, psk);
    if (err != BK_OK) {
        return err;
    }
    return bk_net_connect(net);
}
