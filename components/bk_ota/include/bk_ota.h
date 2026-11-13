#ifndef BK_OTA_H
#define BK_OTA_H

#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BK_OTA_STATE_IDLE = 0,
    BK_OTA_STATE_DOWNLOADING,
    BK_OTA_STATE_READY,
    BK_OTA_STATE_APPLYING,
    BK_OTA_STATE_FAILED,
} bk_ota_state_t;

typedef enum {
    BK_OTA_SECURITY_NONE = 0,
    BK_OTA_SECURITY_SDK = 1,
    BK_OTA_SECURITY_SIGNED = 2,
} bk_ota_security_mode_t;

typedef bk_err_t (*bk_ota_start_fn)(void *context, const char *url,
                                    bk_ota_security_mode_t security_mode);
typedef bk_err_t (*bk_ota_apply_fn)(void *context);

typedef struct {
    bk_ota_start_fn start;
    bk_ota_apply_fn apply;
    void *context;
} bk_ota_adapter_t;

typedef struct {
    const char *url;
    bk_ota_security_mode_t security_mode;
    const bk_ota_adapter_t *adapter;
} bk_ota_config_t;

/* Signing keys, rollback policy, and production security remain SDK/product responsibilities. */
bk_err_t bk_ota_start(const bk_ota_config_t *config);
bk_err_t bk_ota_finish(void);
/* Apply is optional. It enters APPLYING only after an adapter apply callback succeeds. */
bk_err_t bk_ota_apply(void);
bk_err_t bk_ota_fail(bk_err_t error);
bk_err_t bk_ota_retry(void);
bk_err_t bk_ota_reset(void);
bk_ota_state_t bk_ota_get_state(void);
const bk_ota_adapter_t *bk_ota_default_adapter(void);

#ifdef __cplusplus
}
#endif

#endif
