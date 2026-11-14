/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_board.h"
#include "bk_of_net.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    assert(bk_board_init(BK_BOARD_PROFILE_MINIMAL) == BK_OK);
    assert(bk_board_require_cap(BK_BOARD_CAP_WIFI_STA, false) == BK_OK);

    char json_msg[128] = {0};
    assert(bk_lan_format_announce("bk7259-lan-node", 0x0A000015U, json_msg, sizeof(json_msg)) == BK_OK);
    assert(strstr(json_msg, "10.0.0.21") != NULL);

    bk_board_deinit();
    printf("PASS: example 02_lan_device (%s)\n", json_msg);
    return 0;
}
