/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_ERR_H
#define BK_OPEN_FRAMEWORK_BK_ERR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t bk_of_err_t;

/* If Armino SDK common/bk_err.h is not included, provide compatible bk_err_t alias */
#ifndef _BK_ERR_H_
typedef bk_of_err_t bk_err_t;
#define BK_OK 0
#endif

#define BK_ERR_INVALID_ARG           (-1001)
#define BK_ERR_NO_MEM                (-1002)
#define BK_ERR_TIMEOUT               (-1003)
#define BK_ERR_INVALID_STATE         (-1004)
#define BK_ERR_NOT_FOUND             (-1005)
#define BK_ERR_NOT_SUPPORTED         (-1006)
#define BK_ERR_NOT_VERIFIED          (-1007)
#define BK_ERR_CRC_MISMATCH          (-1008)
#define BK_ERR_QUEUE_FULL            (-1009)
#define BK_ERR_PAYLOAD_TOO_LARGE     (-1010)
#define BK_ERR_NET_TIME_UNSYNCED     (-1011)
#define BK_ERR_NET_TLS_VERIFY_FAILED (-1012)
#define BK_ERR_AUDIO_RINGBUF_FULL    (-1013)
#define BK_ERR_OTA_MEDIA_BUSY        (-1014)
#define BK_ERR_BUSY                  (-1015)
#define BK_ERR_CORRUPTED             (-1016)
#define BK_ERR_IO                    (-1017)

static inline const char *bk_of_err_to_name(bk_of_err_t err)
{
    switch (err) {
    case 0:                            return "BK_OK";
    case BK_ERR_INVALID_ARG:           return "BK_ERR_INVALID_ARG";
    case BK_ERR_NO_MEM:                return "BK_ERR_NO_MEM";
    case BK_ERR_TIMEOUT:               return "BK_ERR_TIMEOUT";
    case BK_ERR_INVALID_STATE:         return "BK_ERR_INVALID_STATE";
    case BK_ERR_NOT_FOUND:             return "BK_ERR_NOT_FOUND";
    case BK_ERR_NOT_SUPPORTED:         return "BK_ERR_NOT_SUPPORTED";
    case BK_ERR_NOT_VERIFIED:          return "BK_ERR_NOT_VERIFIED";
    case BK_ERR_CRC_MISMATCH:          return "BK_ERR_CRC_MISMATCH";
    case BK_ERR_QUEUE_FULL:            return "BK_ERR_QUEUE_FULL";
    case BK_ERR_PAYLOAD_TOO_LARGE:     return "BK_ERR_PAYLOAD_TOO_LARGE";
    case BK_ERR_NET_TIME_UNSYNCED:     return "BK_ERR_NET_TIME_UNSYNCED";
    case BK_ERR_NET_TLS_VERIFY_FAILED: return "BK_ERR_NET_TLS_VERIFY_FAILED";
    case BK_ERR_AUDIO_RINGBUF_FULL:    return "BK_ERR_AUDIO_RINGBUF_FULL";
    case BK_ERR_OTA_MEDIA_BUSY:        return "BK_ERR_OTA_MEDIA_BUSY";
    case BK_ERR_BUSY:                  return "BK_ERR_BUSY";
    case BK_ERR_CORRUPTED:             return "BK_ERR_CORRUPTED";
    case BK_ERR_IO:                    return "BK_ERR_IO";
    default:                           return "BK_ERR_UNKNOWN";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_ERR_H */
