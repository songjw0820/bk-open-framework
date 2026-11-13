#ifndef BK_ERR_H
#define BK_ERR_H

#ifdef __cplusplus
extern "C" {
#endif

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
} bk_err_t;

#define BK_ERR_OK BK_OK
#define BK_ERR_NOT_INIT BK_ERR_NOT_INITIALIZED
#define BK_ERR_ALREADY_INIT BK_ERR_ALREADY_INITIALIZED

#ifdef __cplusplus
}
#endif

#endif
