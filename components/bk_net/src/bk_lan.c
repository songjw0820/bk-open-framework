/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_of_net.h"

#include <stdio.h>

bk_err_t bk_lan_format_announce(const char *device_id,
                                uint32_t ipv4_addr_be,
                                char *out_json,
                                size_t out_size)
{
    if (device_id == NULL || out_json == NULL || out_size < 64U) {
        return BK_ERR_INVALID_ARG;
    }
    unsigned int b0 = (ipv4_addr_be >> 24U) & 0xFFU;
    unsigned int b1 = (ipv4_addr_be >> 16U) & 0xFFU;
    unsigned int b2 = (ipv4_addr_be >> 8U) & 0xFFU;
    unsigned int b3 = ipv4_addr_be & 0xFFU;

    int n = snprintf(out_json,
                     out_size,
                     "{\"type\":\"lan_announce\",\"device_id\":\"%s\",\"ip\":\"%u.%u.%u.%u\"}",
                     device_id,
                     b0,
                     b1,
                     b2,
                     b3);
    if (n < 0 || (size_t)n >= out_size) {
        return BK_ERR_PAYLOAD_TOO_LARGE;
    }
    return BK_OK;
}
