/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_port_mem.h"
#include "bk_storage.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool mem_contains_substr(const uint8_t *haystack, size_t hlen, const char *needle)
{
    size_t nlen = strlen(needle);
    if (nlen == 0 || hlen < nlen) {
        return false;
    }
    for (size_t i = 0; i + nlen <= hlen; ++i) {
        if (memcmp(haystack + i, needle, nlen) == 0) {
            return true;
        }
    }
    return false;
}

static void test_sensitive_credential_encryption_and_scrubbing(void)
{
    uint8_t flash_sim[4096];
    memset(flash_sim, 0, sizeof(flash_sim));

    bk_storage_config_t cfg = {
        .flash_sim_buf = flash_sim,
        .flash_sim_size = sizeof(flash_sim),
    };
    bk_storage_handle_t st = NULL;
    assert(bk_storage_open(&cfg, &st) == BK_OK);

    const char *secret_psk = "SuperSecretWiFiPassword_2026!";
    assert(bk_storage_set_blob(st,
                               "wifi",
                               "psk0",
                               secret_psk,
                               strlen(secret_psk) + 1U,
                               BK_STORAGE_FLAG_SENSITIVE) == BK_OK);

    /* Raw flash buffer must NOT contain plaintext password */
    assert(!mem_contains_substr(flash_sim, sizeof(flash_sim), secret_psk));

    /* Reopen from flash_sim and verify decryption */
    bk_storage_close(st);
    st = NULL;
    assert(bk_storage_open(&cfg, &st) == BK_OK);

    char out_buf[64] = {0};
    size_t out_len = sizeof(out_buf);
    uint32_t out_flags = 0;
    assert(bk_storage_get_blob(st, "wifi", "psk0", out_buf, &out_len, &out_flags) == BK_OK);
    assert(strcmp(out_buf, secret_psk) == 0);
    assert((out_flags & BK_STORAGE_FLAG_SENSITIVE) != 0U);

    /* Erase namespace must scrub flash record to zeros */
    assert(bk_storage_erase_ns(st, "wifi") == BK_OK);
    for (size_t i = 0; i < sizeof(flash_sim); ++i) {
        assert(flash_sim[i] == 0U);
    }

    bk_storage_close(st);
}

static void test_power_loss_half_record_rollback(void)
{
    uint8_t flash_sim[4096];
    memset(flash_sim, 0, sizeof(flash_sim));

    bk_storage_config_t cfg = {
        .flash_sim_buf = flash_sim,
        .flash_sim_size = sizeof(flash_sim),
    };
    bk_storage_handle_t st = NULL;
    assert(bk_storage_open(&cfg, &st) == BK_OK);

    const char *v1 = "stable_v1";
    assert(bk_storage_set_blob(st, "sys", "boot_cfg", v1, strlen(v1) + 1U, BK_STORAGE_FLAG_NONE) == BK_OK);

    /* Snapshot offset after first valid record */
    uint8_t snap[4096];
    memcpy(snap, flash_sim, sizeof(flash_sim));

    const char *v2 = "interrupted_v2_record";
    assert(bk_storage_set_blob(st, "sys", "boot_cfg", v2, strlen(v2) + 1U, BK_STORAGE_FLAG_NONE) == BK_OK);
    bk_storage_close(st);

    /* Corrupt the commit trailer of the second record to simulate mid-write power loss */
    for (size_t i = sizeof(flash_sim) - 1U; i > 0; --i) {
        if (flash_sim[i] != 0U) {
            flash_sim[i] ^= 0xFFU;
            break;
        }
    }

    st = NULL;
    assert(bk_storage_open(&cfg, &st) == BK_OK);
    assert(bk_storage_get_recovered_tail_records(st) == 1U);

    char out_buf[64] = {0};
    size_t out_len = sizeof(out_buf);
    assert(bk_storage_get_blob(st, "sys", "boot_cfg", out_buf, &out_len, NULL) == BK_OK);
    assert(strcmp(out_buf, v1) == 0);

    bk_storage_close(st);
}

int main(void)
{
    bk_port_reset_mem_stats();
    test_sensitive_credential_encryption_and_scrubbing();
    test_power_loss_half_record_rollback();

    bk_port_mem_stats_t mstats;
    bk_port_get_mem_stats(&mstats);
    assert(mstats.current_allocated_bytes == 0U);
    assert(mstats.active_allocations == 0U);

    printf("PASS: test_storage_security (CRC32, power-loss rollback, sensitive credential scrubbing)\n");
    return 0;
}
