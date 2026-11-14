/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_STORAGE_H
#define BK_OPEN_FRAMEWORK_BK_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BK_STORAGE_MAX_NS_LEN     16U
#define BK_STORAGE_MAX_KEY_LEN    32U
#define BK_STORAGE_MAX_VAL_LEN    512U

#define BK_STORAGE_FLAG_NONE      0U
#define BK_STORAGE_FLAG_SENSITIVE (1U << 0)

typedef struct bk_storage *bk_storage_handle_t;

typedef bk_err_t (*bk_storage_cipher_fn_t)(bool encrypt,
                                           const uint8_t *in_buf,
                                           uint8_t *out_buf,
                                           size_t len,
                                           void *user_ctx);

typedef struct {
    uint8_t *flash_sim_buf;
    size_t flash_sim_size;
    bk_storage_cipher_fn_t cipher_fn;
    void *cipher_ctx;
} bk_storage_config_t;

uint32_t bk_storage_crc32(const void *data, size_t len);

bk_err_t bk_storage_open(const bk_storage_config_t *cfg, bk_storage_handle_t *out_handle);
void bk_storage_close(bk_storage_handle_t handle);

bk_err_t bk_storage_set_blob(bk_storage_handle_t handle,
                             const char *ns,
                             const char *key,
                             const void *value,
                             size_t len,
                             uint32_t flags);

bk_err_t bk_storage_get_blob(bk_storage_handle_t handle,
                             const char *ns,
                             const char *key,
                             void *out_value,
                             size_t *inout_len,
                             uint32_t *out_flags);

bk_err_t bk_storage_delete_key(bk_storage_handle_t handle,
                               const char *ns,
                               const char *key);

bk_err_t bk_storage_erase_ns(bk_storage_handle_t handle, const char *ns);
bk_err_t bk_storage_factory_reset(bk_storage_handle_t handle);

/* Returns count of rolled-back incomplete/corrupted tail records during open */
size_t bk_storage_get_recovered_tail_records(bk_storage_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_STORAGE_H */
