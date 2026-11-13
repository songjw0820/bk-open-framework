#ifndef BK_ERR_H
#define BK_ERR_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)
#include <common/bk_err.h>

#ifndef BK_ERR_INVALID_ARG
#define BK_ERR_INVALID_ARG         (BK_ERR_COMMON_BASE - 101)
#endif
#ifndef BK_ERR_NOT_INITIALIZED
#define BK_ERR_NOT_INITIALIZED     BK_ERR_NOT_INIT
#endif
#ifndef BK_ERR_ALREADY_INITIALIZED
#define BK_ERR_ALREADY_INITIALIZED (BK_ERR_COMMON_BASE - 103)
#endif
#ifndef BK_ERR_NOT_STARTED
#define BK_ERR_NOT_STARTED         (BK_ERR_COMMON_BASE - 104)
#endif
#ifndef BK_ERR_ALREADY_STARTED
#define BK_ERR_ALREADY_STARTED     (BK_ERR_COMMON_BASE - 105)
#endif
#ifndef BK_ERR_ALREADY_EXISTS
#define BK_ERR_ALREADY_EXISTS      BK_ERR_IS_EXIST
#endif
#ifndef BK_ERR_NO_MEMORY
#define BK_ERR_NO_MEMORY           BK_ERR_NO_MEM
#endif
#ifndef BK_ERR_LIMIT
#define BK_ERR_LIMIT               (BK_ERR_COMMON_BASE - 109)
#endif
#ifndef BK_ERR_REENTRANT
#define BK_ERR_REENTRANT           (BK_ERR_COMMON_BASE - 110)
#endif
#ifndef BK_ERR_NOT_SUPPORTED
#define BK_ERR_NOT_SUPPORTED       BK_ERR_NOT_SUPPORT
#endif

#else /* Host standalone build */

typedef enum {
    BK_OK = 0,
    BK_ERR_INVALID_ARG = -1,
    BK_ERR_NOT_INITIALIZED = -2,
    BK_ERR_ALREADY_INITIALIZED = -3,
    BK_ERR_NOT_STARTED = -4,
    BK_ERR_ALREADY_STARTED = -5,
    BK_ERR_ALREADY_EXISTS = -6,
    BK_ERR_NOT_FOUND = -7,
    BK_ERR_NO_MEMORY = -8,
    BK_ERR_LIMIT = -9,
    BK_ERR_REENTRANT = -10,
    BK_ERR_BUSY = -11,
    BK_ERR_NOT_SUPPORTED = -12,
} bk_err_t;

#define BK_ERR_NOT_INIT BK_ERR_NOT_INITIALIZED

#endif /* defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE) */

#define BK_ERR_OK BK_OK
#define BK_ERR_ALREADY_INIT BK_ERR_ALREADY_INITIALIZED

#ifdef __cplusplus
}
#endif

#endif
