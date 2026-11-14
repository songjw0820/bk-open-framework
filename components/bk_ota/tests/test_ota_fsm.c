/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_ota.h"
#include "bk_port_mem.h"
#include "bk_storage.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bk_err_t fail_quiesce_cb(bool suspend, void *user_ctx)
{
    (void)suspend;
    (void)user_ctx;
    return BK_ERR_BUSY;
}

static bk_err_t ok_quiesce_cb(bool suspend, void *user_ctx)
{
    int *state = (int *)user_ctx;
    *state = suspend ? 1 : 0;
    return BK_OK;
}

static bk_err_t test_sig_verify_cb(const bk_ota_image_header_t *hdr,
                                   const uint8_t *payload,
                                   size_t payload_len,
                                   void *user_ctx)
{
    (void)payload;
    (void)payload_len;
    (void)user_ctx;
    uint8_t acc = 0U;
    for (size_t i = 0U; i < sizeof(hdr->ec256_signature); ++i) {
        acc |= hdr->ec256_signature[i];
    }
    return acc != 0U ? BK_OK : BK_ERR_CORRUPTED;
}

static void test_rejects_unbudgeted_264k_partition(void)
{
    uint8_t dummy[1024];
    bk_ota_config_t bad_cfg = {
        .strategy = BK_OTA_STRATEGY_OVERWRITE,
        .ota_partition_bytes = 264U * 1024U, /* Default bk7259_robot 264KB slot */
        .has_control_partition = false,
        .staging_buf = dummy,
        .staging_buf_cap = sizeof(dummy),
    };
    bk_ota_handle_t ota = NULL;
    assert(bk_ota_create(&bad_cfg, &ota) == BK_ERR_NOT_VERIFIED);
}

static void test_media_quiesce_and_anti_rollback(void)
{
    size_t cap = 128U * 1024U;
    uint8_t *staging = (uint8_t *)malloc(cap);
    assert(staging != NULL);

    bk_ota_config_t cfg = {
        .strategy = BK_OTA_STRATEGY_OVERWRITE,
        .ota_partition_bytes = 1960U * 1024U,
        .has_control_partition = true,
        .current_security_counter = 5U,
        .staging_buf = staging,
        .staging_buf_cap = cap,
        .media_quiesce_fn = fail_quiesce_cb,
    };
    bk_ota_handle_t ota = NULL;
    assert(bk_ota_create(&cfg, &ota) == BK_OK);

    bk_ota_image_header_t hdr = {
        .magic = BK_OTA_HEADER_MAGIC,
        .image_size = 4096U,
        .image_crc32 = 0x12345678U,
        .security_counter = 4U, /* Downgrade attempt (< 5) */
    };
    hdr.ec256_signature[0] = 0xA5U;

    /* 1. Anti-rollback rejection */
    assert(bk_ota_begin(ota, &hdr) == BK_ERR_INVALID_STATE);

    /* 2. Media busy abort */
    hdr.security_counter = 5U;
    assert(bk_ota_begin(ota, &hdr) == BK_ERR_OTA_MEDIA_BUSY);
    assert(bk_ota_get_state(ota) == BK_OTA_STATE_ABORTED);

    bk_ota_destroy(ota);
    free(staging);
}

static void test_power_loss_64kb_resume_and_signature_verification(void)
{
    uint8_t flash_sim[4096];
    memset(flash_sim, 0xFF, sizeof(flash_sim));
    bk_storage_config_t scfg = {
        .flash_sim_buf = flash_sim,
        .flash_sim_size = sizeof(flash_sim),
    };
    bk_storage_handle_t store = NULL;
    assert(bk_storage_open(&scfg, &store) == BK_OK);

    size_t img_sz = 96U * 1024U; /* 96KB image (> 64KB checkpoint granularity) */
    uint8_t *fw_img = (uint8_t *)malloc(img_sz);
    uint8_t *staging = (uint8_t *)malloc(img_sz);
    assert(fw_img != NULL && staging != NULL);
    for (size_t i = 0U; i < img_sz; ++i) {
        fw_img[i] = (uint8_t)((i * 31U + 7U) & 0xFFU);
    }

    bk_ota_image_header_t hdr = {
        .magic = BK_OTA_HEADER_MAGIC,
        .image_size = (uint32_t)img_sz,
        .image_crc32 = bk_storage_crc32(fw_img, img_sz),
        .security_counter = 7U,
    };
    hdr.ec256_signature[0] = 0x5AU;

    int media_suspended = 0;
    bk_ota_config_t cfg = {
        .strategy = BK_OTA_STRATEGY_OVERWRITE,
        .ota_partition_bytes = 1960U * 1024U,
        .has_control_partition = true,
        .current_security_counter = 6U,
        .storage = store,
        .staging_buf = staging,
        .staging_buf_cap = img_sz,
        .media_quiesce_fn = ok_quiesce_cb,
        .media_quiesce_ctx = &media_suspended,
    };

    bk_ota_config_t no_verifier_cfg = cfg;
    no_verifier_cfg.storage = NULL;
    bk_ota_handle_t no_verifier_ota = NULL;
    assert(bk_ota_create(&no_verifier_cfg, &no_verifier_ota) == BK_OK);
    assert(bk_ota_begin(no_verifier_ota, &hdr) == BK_OK);
    assert(bk_ota_write_chunk(no_verifier_ota, 0U, fw_img, img_sz) == BK_OK);
    assert(bk_ota_finalize(no_verifier_ota) == BK_ERR_NOT_SUPPORTED);
    bk_ota_destroy(no_verifier_ota);

    cfg.sig_verify_fn = test_sig_verify_cb;

    /* Session 1: write 64KB chunk, then simulate power loss before remaining 32KB */
    bk_ota_handle_t ota1 = NULL;
    assert(bk_ota_create(&cfg, &ota1) == BK_OK);
    assert(bk_ota_begin(ota1, &hdr) == BK_OK);
    assert(media_suspended == 1);
    assert(bk_ota_write_chunk(ota1, 0U, fw_img, 64U * 1024U) == BK_OK);
    bk_ota_destroy(ota1);

    /* Session 2: reopen after power loss -> resumes from 64KB checkpoint */
    bk_ota_handle_t ota2 = NULL;
    assert(bk_ota_create(&cfg, &ota2) == BK_OK);
    assert(bk_ota_begin(ota2, &hdr) == BK_OK);
    assert(bk_ota_get_resume_offset(ota2) == 64U * 1024U);
    assert(bk_ota_write_chunk(ota2, 64U * 1024U, fw_img + (64U * 1024U), 32U * 1024U) == BK_OK);
    assert(bk_ota_finalize(ota2) == BK_OK);
    assert(bk_ota_get_state(ota2) == BK_OTA_STATE_VERIFIED_PENDING_REBOOT);
    /* OVERWRITE strategy explicitly forbids post-boot trial confirmation */
    assert(bk_ota_mark_app_valid(ota2) == BK_ERR_NOT_SUPPORTED);
    bk_ota_destroy(ota2);

    /* Session 3: tamper 1 byte -> must fail with BK_ERR_CRC_MISMATCH */
    bk_ota_handle_t ota3 = NULL;
    assert(bk_ota_create(&cfg, &ota3) == BK_OK);
    assert(bk_ota_begin(ota3, &hdr) == BK_OK);
    assert(bk_ota_write_chunk(ota3, 0U, fw_img, img_sz) == BK_OK);
    staging[100] ^= 0xFFU;
    assert(bk_ota_finalize(ota3) == BK_ERR_CRC_MISMATCH);
    bk_ota_destroy(ota3);

    /* Session 4: zeroed signature -> must fail with BK_ERR_CORRUPTED */
    bk_ota_image_header_t bad_sig_hdr = hdr;
    memset(bad_sig_hdr.ec256_signature, 0, sizeof(bad_sig_hdr.ec256_signature));
    bk_ota_handle_t ota4 = NULL;
    assert(bk_ota_create(&cfg, &ota4) == BK_OK);
    assert(bk_ota_begin(ota4, &bad_sig_hdr) == BK_OK);
    assert(bk_ota_write_chunk(ota4, 0U, fw_img, img_sz) == BK_OK);
    assert(bk_ota_finalize(ota4) == BK_ERR_CORRUPTED);
    bk_ota_destroy(ota4);

    bk_storage_close(store);
    free(fw_img);
    free(staging);
}

int main(void)
{
    bk_port_reset_mem_stats();
    test_rejects_unbudgeted_264k_partition();
    test_media_quiesce_and_anti_rollback();
    test_power_loss_64kb_resume_and_signature_verification();

    bk_port_mem_stats_t mstats;
    bk_port_get_mem_stats(&mstats);
    assert(mstats.current_allocated_bytes == 0U);
    assert(mstats.active_allocations == 0U);

    printf("PASS: test_ota_fsm (264KB gate rejection, media quiesce, anti-rollback, 64KB resume, CRC/signature verifier gate)\n");
    return 0;
}
