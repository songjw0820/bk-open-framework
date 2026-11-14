/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_ota.h"
#include "bk_board.h"
#include "bk_port_mem.h"

#include <string.h>

typedef struct {
    uint32_t magic;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t security_counter;
    uint32_t checkpoint_offset;
} bk_ota_checkpoint_rec_t;

struct bk_ota_ctx {
    bk_ota_config_t cfg;
    bk_ota_state_t state;
    bk_ota_image_header_t active_hdr;
    size_t write_offset;
    size_t last_checkpoint_offset;
    bool media_quiesced;
};

static bk_err_t missing_sig_verifier(const bk_ota_image_header_t *hdr,
                                     const uint8_t *payload,
                                     size_t payload_len,
                                     void *user_ctx)
{
    /* A cryptographic verifier must be supplied by the target integration. */
    (void)hdr;
    (void)payload;
    (void)payload_len;
    (void)user_ctx;
    return BK_ERR_NOT_SUPPORTED;
}

bk_err_t bk_ota_create(const bk_ota_config_t *cfg, bk_ota_handle_t *out_ota)
{
    if (cfg == NULL || out_ota == NULL || cfg->staging_buf == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (cfg->strategy != BK_OTA_STRATEGY_OVERWRITE && cfg->strategy != BK_OTA_STRATEGY_XIP) {
        return BK_ERR_INVALID_ARG;
    }
    /* Enforce Task 0 / Task 7 partition gate:
     * Reject the default 264KB slot or missing ota_control/boot_param partition.
     */
    if (!cfg->has_control_partition || cfg->ota_partition_bytes < BK_OTA_MIN_SLOT_BYTES_8MB) {
        return BK_ERR_NOT_VERIFIED;
    }
    if (cfg->staging_buf_cap == 0U) {
        return BK_ERR_INVALID_ARG;
    }

    struct bk_ota_ctx *ota = (struct bk_ota_ctx *)bk_port_calloc(
        1, sizeof(*ota), BK_MEM_CAP_DEFAULT, "bk_ota");
    if (ota == NULL) {
        return BK_ERR_NO_MEM;
    }
    ota->cfg = *cfg;
    ota->state = BK_OTA_STATE_IDLE;
    *out_ota = ota;
    return BK_OK;
}

void bk_ota_destroy(bk_ota_handle_t ota)
{
    if (ota != NULL) {
        if (ota->media_quiesced && ota->cfg.media_quiesce_fn != NULL) {
            (void)ota->cfg.media_quiesce_fn(false, ota->cfg.media_quiesce_ctx);
        }
        bk_port_free(ota);
    }
}

bk_ota_state_t bk_ota_get_state(bk_ota_handle_t ota)
{
    return ota != NULL ? ota->state : BK_OTA_STATE_IDLE;
}

size_t bk_ota_get_resume_offset(bk_ota_handle_t ota)
{
    return ota != NULL ? ota->write_offset : 0U;
}

bk_err_t bk_ota_begin(bk_ota_handle_t ota, const bk_ota_image_header_t *hdr)
{
    if (ota == NULL || hdr == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (hdr->magic != BK_OTA_HEADER_MAGIC || hdr->image_size == 0U) {
        return BK_ERR_CORRUPTED;
    }
    if (hdr->security_counter < ota->cfg.current_security_counter) {
        return BK_ERR_INVALID_STATE;
    }
    if (hdr->image_size > ota->cfg.ota_partition_bytes ||
        hdr->image_size > ota->cfg.staging_buf_cap) {
        return BK_ERR_PAYLOAD_TOO_LARGE;
    }

    if (ota->cfg.media_quiesce_fn != NULL) {
        bk_err_t qerr = ota->cfg.media_quiesce_fn(true, ota->cfg.media_quiesce_ctx);
        if (qerr != BK_OK) {
            ota->state = BK_OTA_STATE_ABORTED;
            return BK_ERR_OTA_MEDIA_BUSY;
        }
        ota->media_quiesced = true;
    }

    ota->active_hdr = *hdr;
    ota->write_offset = 0U;
    ota->last_checkpoint_offset = 0U;

    if (ota->cfg.storage != NULL) {
        bk_ota_checkpoint_rec_t rec = {0};
        size_t len = sizeof(rec);
        uint32_t flags = 0U;
        if (bk_storage_get_blob(ota->cfg.storage, "ota", "ckpt", &rec, &len, &flags) == BK_OK &&
            len == sizeof(rec) &&
            rec.magic == BK_OTA_CONFIRM_REC_MAGIC &&
            rec.image_size == hdr->image_size &&
            rec.image_crc32 == hdr->image_crc32 &&
            rec.security_counter == hdr->security_counter &&
            rec.checkpoint_offset <= hdr->image_size) {
            ota->write_offset = rec.checkpoint_offset;
            ota->last_checkpoint_offset = rec.checkpoint_offset;
        }
    }

    ota->state = BK_OTA_STATE_DOWNLOADING;
    return BK_OK;
}

static void save_checkpoint(struct bk_ota_ctx *ota)
{
    if (ota->cfg.storage == NULL) {
        return;
    }
    bk_ota_checkpoint_rec_t rec = {
        .magic = BK_OTA_CONFIRM_REC_MAGIC,
        .image_size = ota->active_hdr.image_size,
        .image_crc32 = ota->active_hdr.image_crc32,
        .security_counter = ota->active_hdr.security_counter,
        .checkpoint_offset = (uint32_t)ota->write_offset,
    };
    (void)bk_storage_set_blob(
        ota->cfg.storage, "ota", "ckpt", &rec, sizeof(rec), BK_STORAGE_FLAG_NONE);
}

bk_err_t bk_ota_write_chunk(bk_ota_handle_t ota,
                            size_t offset,
                            const uint8_t *data,
                            size_t len)
{
    if (ota == NULL || data == NULL || len == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    if (ota->state != BK_OTA_STATE_DOWNLOADING) {
        return BK_ERR_INVALID_STATE;
    }
    if (offset != ota->write_offset) {
        return BK_ERR_INVALID_ARG;
    }
    if (offset + len > ota->active_hdr.image_size) {
        return BK_ERR_PAYLOAD_TOO_LARGE;
    }

    memcpy(ota->cfg.staging_buf + offset, data, len);
    ota->write_offset += len;

    if ((ota->write_offset - ota->last_checkpoint_offset) >= BK_OTA_CHECKPOINT_GRANULARITY_BYTES ||
        ota->write_offset == ota->active_hdr.image_size) {
        ota->last_checkpoint_offset =
            (ota->write_offset / BK_OTA_CHECKPOINT_GRANULARITY_BYTES) *
            BK_OTA_CHECKPOINT_GRANULARITY_BYTES;
        if (ota->write_offset == ota->active_hdr.image_size) {
            ota->last_checkpoint_offset = ota->write_offset;
        }
        save_checkpoint(ota);
    }
    return BK_OK;
}

bk_err_t bk_ota_finalize(bk_ota_handle_t ota)
{
    if (ota == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (ota->state != BK_OTA_STATE_DOWNLOADING ||
        ota->write_offset != ota->active_hdr.image_size) {
        return BK_ERR_INVALID_STATE;
    }

    uint32_t crc = bk_storage_crc32(ota->cfg.staging_buf, ota->active_hdr.image_size);
    if (crc != ota->active_hdr.image_crc32) {
        if (ota->cfg.storage != NULL) {
            (void)bk_storage_delete_key(ota->cfg.storage, "ota", "ckpt");
        }
        ota->state = BK_OTA_STATE_ABORTED;
        return BK_ERR_CRC_MISMATCH;
    }

    bk_ota_sig_verify_fn_t verify_fn = ota->cfg.sig_verify_fn != NULL
                                           ? ota->cfg.sig_verify_fn
                                           : missing_sig_verifier;
    bk_err_t sig_err = verify_fn(
        &ota->active_hdr, ota->cfg.staging_buf, ota->active_hdr.image_size, ota->cfg.sig_verify_ctx);
    if (sig_err != BK_OK) {
        if (ota->cfg.storage != NULL) {
            (void)bk_storage_delete_key(ota->cfg.storage, "ota", "ckpt");
        }
        ota->state = BK_OTA_STATE_ABORTED;
        return sig_err == BK_ERR_NOT_SUPPORTED ? BK_ERR_NOT_SUPPORTED : BK_ERR_CORRUPTED;
    }

    if (ota->cfg.storage != NULL) {
        (void)bk_storage_delete_key(ota->cfg.storage, "ota", "ckpt");
    }

    ota->state = (ota->cfg.strategy == BK_OTA_STRATEGY_XIP)
                     ? BK_OTA_STATE_TRIAL_PENDING_CONFIRM
                     : BK_OTA_STATE_VERIFIED_PENDING_REBOOT;
    return BK_OK;
}

bk_err_t bk_ota_mark_app_valid(bk_ota_handle_t ota)
{
    if (ota == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (ota->cfg.strategy == BK_OTA_STRATEGY_OVERWRITE) {
        return BK_ERR_NOT_SUPPORTED;
    }
    if (ota->state != BK_OTA_STATE_TRIAL_PENDING_CONFIRM) {
        return BK_ERR_INVALID_STATE;
    }
    ota->state = BK_OTA_STATE_CONFIRMED;
    if (ota->media_quiesced && ota->cfg.media_quiesce_fn != NULL) {
        (void)ota->cfg.media_quiesce_fn(false, ota->cfg.media_quiesce_ctx);
        ota->media_quiesced = false;
    }
    return BK_OK;
}

bk_err_t bk_ota_abort(bk_ota_handle_t ota)
{
    if (ota == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    ota->state = BK_OTA_STATE_ABORTED;
    if (ota->media_quiesced && ota->cfg.media_quiesce_fn != NULL) {
        (void)ota->cfg.media_quiesce_fn(false, ota->cfg.media_quiesce_ctx);
        ota->media_quiesced = false;
    }
    return BK_OK;
}
