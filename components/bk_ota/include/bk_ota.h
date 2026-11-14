/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_OTA_H
#define BK_OPEN_FRAMEWORK_BK_OTA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_err.h"
#include "bk_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BK_OTA_HEADER_MAGIC       0x424B4F54U /* "BKOT" */
#define BK_OTA_CONFIRM_REC_MAGIC  0x4F544143U /* "OTAC" (Armino ota_confirm_rec) */
#define BK_OTA_CHECKPOINT_GRANULARITY_BYTES (64U * 1024U)
#define BK_OTA_MIN_SLOT_BYTES_8MB (1536U * 1024U)

typedef enum {
    BK_OTA_STRATEGY_NONE = 0,
    BK_OTA_STRATEGY_OVERWRITE, /* CONFIG_SECURE_OTA_OVERWRITE (BL2 decompresses over primary_all; no post-boot rollback) */
    BK_OTA_STRATEGY_XIP,       /* CONFIG_SECURE_OTA_XIP (A/B dual slot with boot_param TRIAL & post-boot rollback) */
} bk_ota_strategy_t;

typedef enum {
    BK_OTA_STATE_IDLE = 0,
    BK_OTA_STATE_QUIESCED,
    BK_OTA_STATE_DOWNLOADING,
    BK_OTA_STATE_VERIFIED_PENDING_REBOOT,
    BK_OTA_STATE_TRIAL_PENDING_CONFIRM,
    BK_OTA_STATE_CONFIRMED,
    BK_OTA_STATE_ABORTED,
} bk_ota_state_t;

typedef struct {
    uint32_t magic;                /* Must be BK_OTA_HEADER_MAGIC */
    uint32_t image_size;           /* Total compressed payload size in bytes */
    uint32_t image_crc32;          /* CRC32 of compressed payload */
    uint32_t security_counter;     /* BL2 anti-rollback counter (must be >= current_security_counter) */
    uint8_t ec256_signature[64];   /* EC-P256 raw R||S signature verified by BL2 / verify_sig_fn */
} bk_ota_image_header_t;

typedef bk_err_t (*bk_ota_media_quiesce_fn_t)(bool suspend, void *user_ctx);

typedef bk_err_t (*bk_ota_sig_verify_fn_t)(const bk_ota_image_header_t *hdr,
                                           const uint8_t *payload,
                                           size_t payload_len,
                                           void *user_ctx);

typedef struct {
    bk_ota_strategy_t strategy;
    size_t ota_partition_bytes;
    bool has_control_partition; /* true if ota_control (OVERWRITE) or boot_param (XIP) is present */
    uint32_t current_security_counter;
    bk_storage_handle_t storage; /* Optional KV store to persist 64KB resume checkpoints across power loss */
    uint8_t *staging_buf;        /* Staging buffer simulating or backing the OTA partition */
    size_t staging_buf_cap;
    bk_ota_media_quiesce_fn_t media_quiesce_fn;
    void *media_quiesce_ctx;
    bk_ota_sig_verify_fn_t sig_verify_fn;
    void *sig_verify_ctx;
} bk_ota_config_t;

typedef struct bk_ota_ctx *bk_ota_handle_t;

bk_err_t bk_ota_create(const bk_ota_config_t *cfg, bk_ota_handle_t *out_ota);
void bk_ota_destroy(bk_ota_handle_t ota);

bk_ota_state_t bk_ota_get_state(bk_ota_handle_t ota);
size_t bk_ota_get_resume_offset(bk_ota_handle_t ota);

/*
 * Begins or resumes an OTA session for `hdr`:
 * 1. Rejects anti-rollback violations (hdr->security_counter < current_security_counter) with BK_ERR_INVALID_STATE
 * 2. Rejects oversized images (hdr->image_size > ota_partition_bytes) with BK_ERR_PAYLOAD_TOO_LARGE
 * 3. Invokes media_quiesce_fn(true) to suspend audio/video DMA; aborts with BK_ERR_OTA_MEDIA_BUSY on failure
 * 4. If storage has a matching checkpoint for the same image_crc32 & security_counter, restores resume_offset
 */
bk_err_t bk_ota_begin(bk_ota_handle_t ota, const bk_ota_image_header_t *hdr);

/*
 * Writes a chunk at `offset` (must match current write offset).
 * Persists a resume checkpoint in `storage` at every 64KB boundary.
 */
bk_err_t bk_ota_write_chunk(bk_ota_handle_t ota,
                            size_t offset,
                            const uint8_t *data,
                            size_t len);

/*
 * Finalizes download:
 * 1. Verifies total written bytes == hdr->image_size
 * 2. Verifies CRC32 of staged payload == hdr->image_crc32 (returns BK_ERR_CRC_MISMATCH on mismatch)
 * 3. Verifies the image signature via sig_verify_fn. A target integration must
 *    provide this callback. If it is missing, returns BK_ERR_NOT_SUPPORTED.
 * 4. Transitions to VERIFIED_PENDING_REBOOT (OVERWRITE) or TRIAL_PENDING_CONFIRM (XIP)
 */
bk_err_t bk_ota_finalize(bk_ota_handle_t ota);

/*
 * Post-boot self-test confirmation:
 * - In BK_OTA_STRATEGY_XIP mode, marks the trial image valid -> BK_OTA_STATE_CONFIRMED.
 * - In BK_OTA_STRATEGY_OVERWRITE mode, returns BK_ERR_NOT_SUPPORTED (BL2 overwrite has no post-boot rollback).
 */
bk_err_t bk_ota_mark_app_valid(bk_ota_handle_t ota);

bk_err_t bk_ota_abort(bk_ota_handle_t ota);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_OTA_H */
