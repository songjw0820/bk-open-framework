/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_board.h"
#include "bk_health.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_cp_template_isolation_contract(void)
{
    bk_board_cp_template_info_t min_info = {0};
    assert(bk_board_get_cp_template_info(BK_BOARD_PROFILE_MINIMAL, &min_info) == BK_OK);
    assert(strcmp(min_info.cp_template_source, "sdk") == 0);
    assert(strcmp(min_info.cp_template_path, "projects/app/cp") == 0);

    bk_board_cp_template_info_t rob_info = {0};
    assert(bk_board_get_cp_template_info(BK_BOARD_PROFILE_ROBOT, &rob_info) == BK_OK);
    assert(strcmp(rob_info.cp_template_source, "solution") == 0);
    assert(strcmp(rob_info.cp_template_path, "projects/beken_robot/cp") == 0);
}

static void test_capability_verification_enforcement(void)
{
    assert(bk_board_init(BK_BOARD_PROFILE_MINIMAL) == BK_OK);

    /* Static-verified capability succeeds when hardware verification is not strictly required */
    assert(bk_board_require_cap(BK_BOARD_CAP_WIFI_STA, false) == BK_OK);

    /* Robot-only capability is rejected under minimal profile */
    assert(bk_board_require_cap(BK_BOARD_CAP_SERVO_PWM, false) == BK_ERR_NOT_SUPPORTED);

    /* Unverified capabilities must return BK_ERR_NOT_VERIFIED */
    assert(bk_board_require_cap(BK_BOARD_CAP_PSRAM_PERIPHERAL_DMA_DIRECT, false) == BK_ERR_NOT_VERIFIED);
    assert(bk_board_require_cap(BK_BOARD_CAP_SECURE_OTA_8MB_DEFAULT, false) == BK_ERR_NOT_VERIFIED);

    bk_board_deinit();

    assert(bk_board_init(BK_BOARD_PROFILE_ROBOT) == BK_OK);
    assert(bk_board_require_cap(BK_BOARD_CAP_SERVO_PWM, false) == BK_OK);
    assert(bk_board_require_cap(BK_BOARD_CAP_PSRAM_PERIPHERAL_DMA_DIRECT, false) == BK_ERR_NOT_VERIFIED);
    assert(bk_board_mark_cap_hardware_verified(BK_BOARD_CAP_PSRAM_PERIPHERAL_DMA_DIRECT) == BK_OK);
    assert(bk_board_require_cap(BK_BOARD_CAP_PSRAM_PERIPHERAL_DMA_DIRECT, true) == BK_OK);
    bk_board_deinit();
}

int main(void)
{
    bk_port_reset_mem_stats();
    test_cp_template_isolation_contract();
    test_capability_verification_enforcement();

    bk_health_thresholds_t thresh = {
        .warn_allocated_bytes = 512U,
        .critical_allocated_bytes = 2048U,
        .max_active_allocations = 4U,
    };
    bk_health_snapshot_t snap = {0};
    assert(bk_health_evaluate(&thresh, &snap) == BK_OK);
    assert(snap.status == BK_HEALTH_STATUS_OK);

    void *p1 = bk_port_malloc(1024U, BK_MEM_CAP_SRAM, "test_warn");
    assert(bk_health_evaluate(&thresh, &snap) == BK_OK);
    assert(snap.status == BK_HEALTH_STATUS_WARNING);

    void *p2 = bk_port_malloc(2048U, BK_MEM_CAP_PSRAM, "test_crit");
    assert(bk_health_evaluate(&thresh, &snap) == BK_OK);
    assert(snap.status == BK_HEALTH_STATUS_CRITICAL);

    bk_port_free(p2);
    bk_port_free(p1);
    assert(bk_health_evaluate(&thresh, &snap) == BK_OK);
    assert(snap.status == BK_HEALTH_STATUS_OK);

    printf("PASS: test_board_caps (CP template isolation, unverified capability rejection, health watermark evaluation)\n");
    return 0;
}
