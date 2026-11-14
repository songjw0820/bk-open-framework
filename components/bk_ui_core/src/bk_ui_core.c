/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_ui_core.h"
#include "bk_board.h"
#include "bk_port_mem.h"
#include "bk_port_os.h"

struct bk_ui_core {
    bk_port_mutex_t recursive_lock;
    uint32_t depth;
};

bk_err_t bk_ui_core_create(bk_ui_core_handle_t *out_ui)
{
    if (out_ui == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_err_t cap_err = bk_board_require_cap(BK_BOARD_CAP_DISPLAY_LVGL, false);
    if (cap_err != BK_OK) {
        return cap_err;
    }
    struct bk_ui_core *ui = (struct bk_ui_core *)bk_port_calloc(
        1, sizeof(*ui), BK_MEM_CAP_DEFAULT, "bk_ui_core");
    if (ui == NULL) {
        return BK_ERR_NO_MEM;
    }
    bk_err_t err = bk_port_mutex_create(true, &ui->recursive_lock);
    if (err != BK_OK) {
        bk_port_free(ui);
        return err;
    }
    *out_ui = ui;
    return BK_OK;
}

void bk_ui_core_destroy(bk_ui_core_handle_t ui)
{
    if (ui == NULL) {
        return;
    }
    if (ui->recursive_lock != NULL) {
        bk_port_mutex_destroy(ui->recursive_lock);
    }
    bk_port_free(ui);
}

bk_err_t bk_ui_lock(bk_ui_core_handle_t ui, uint32_t timeout_ms)
{
    if (ui == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_err_t err = bk_port_mutex_lock(ui->recursive_lock, timeout_ms);
    if (err == BK_OK) {
        ui->depth += 1U;
    }
    return err;
}

bk_err_t bk_ui_unlock(bk_ui_core_handle_t ui)
{
    if (ui == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (ui->depth == 0U) {
        return BK_ERR_INVALID_STATE;
    }
    ui->depth -= 1U;
    return bk_port_mutex_unlock(ui->recursive_lock);
}

uint32_t bk_ui_get_lock_depth(bk_ui_core_handle_t ui)
{
    if (ui == NULL) {
        return 0U;
    }
    return ui->depth;
}
