#include "bk_board.h"

#include <stddef.h>
#include <string.h>

#include "pins.h"

static int g_bk_board_initialized;
static const bk_board_info_t g_bk_board_info = {
    .name = "bk7259_robot",
    .chip = "BK7259",
    .profile = "robot",
};

bk_err_t bk_board_init(const bk_board_config_t *config)
{
    if (config == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (g_bk_board_initialized) {
        return BK_ERR_ALREADY_INITIALIZED;
    }
    if (config->board_name != NULL && config->board_name[0] != '\0' &&
        strcmp(config->board_name, "bk7259_robot") != 0) {
        return BK_ERR_INVALID_ARG;
    }
    g_bk_board_initialized = 1;
    return BK_OK;
}

bool bk_board_has_capability(bk_board_capability_t capability)
{
    return g_bk_board_initialized && capability == BK_BOARD_CAPABILITY_CP_UART0;
}

const bk_board_info_t *bk_board_get_info(void)
{
    return g_bk_board_initialized ? &g_bk_board_info : NULL;
}

bk_err_t bk_board_get_pin(bk_board_pin_id_t id, bk_gpio_pin_t *pin)
{
    if (pin == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (!g_bk_board_initialized) {
        return BK_ERR_NOT_INITIALIZED;
    }
    switch (id) {
    case BK_BOARD_PIN_CP_UART0_RX:
        *pin = BK7259_ROBOT_CP_UART0_RX_GPIO;
        return BK_OK;
    case BK_BOARD_PIN_CP_UART0_TX:
        *pin = BK7259_ROBOT_CP_UART0_TX_GPIO;
        return BK_OK;
    default:
        return BK_ERR_NOT_FOUND;
    }
}
