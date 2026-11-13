#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bk_board.h"
#include "pins.h"

static void test_uninitialized(void)
{
    bk_gpio_pin_t pin = 0xff;
    assert(!bk_board_has_capability(BK_BOARD_CAPABILITY_CP_UART0));
    assert(bk_board_get_info() == NULL);
    assert(bk_board_get_pin(BK_BOARD_PIN_CP_UART0_RX, &pin) == BK_ERR_NOT_INITIALIZED);
}

static void test_init_and_verified_capabilities(void)
{
    bk_board_config_t config = {0};
    bk_board_config_t wrong_board = {.board_name = "other_board"};
    const bk_board_info_t *info;
    bk_gpio_pin_t pin = 0xff;

    assert(bk_board_init(NULL) == BK_ERR_INVALID_ARG);
    assert(bk_board_init(&wrong_board) == BK_ERR_INVALID_ARG);
    assert(bk_board_init(&config) == BK_OK);
    assert(bk_board_init(&config) == BK_ERR_ALREADY_INITIALIZED);
    assert(bk_board_has_capability(BK_BOARD_CAPABILITY_CP_UART0));
    assert(!bk_board_has_capability(BK_BOARD_CAPABILITY_AUDIO));
    assert(!bk_board_has_capability(BK_BOARD_CAPABILITY_CAMERA));
    assert(!bk_board_has_capability((bk_board_capability_t)-1));

    info = bk_board_get_info();
    assert(info != NULL);
    assert(strcmp(info->name, "bk7259_robot") == 0);
    assert(strcmp(info->chip, "BK7259") == 0);
    assert(strcmp(info->profile, "robot") == 0);
    assert(bk_board_get_pin(BK_BOARD_PIN_CP_UART0_RX, &pin) == BK_OK);
    assert(pin == BK7259_ROBOT_CP_UART0_RX_GPIO);
    assert(bk_board_get_pin(BK_BOARD_PIN_CP_UART0_TX, &pin) == BK_OK);
    assert(pin == BK7259_ROBOT_CP_UART0_TX_GPIO);
    assert(bk_board_get_pin(BK_BOARD_PIN_COUNT, &pin) == BK_ERR_NOT_FOUND);
    assert(bk_board_get_pin((bk_board_pin_id_t)-1, &pin) == BK_ERR_NOT_FOUND);
    assert(bk_board_get_pin(BK_BOARD_PIN_CP_UART0_RX, NULL) == BK_ERR_INVALID_ARG);
}

int main(void)
{
    test_uninitialized();
    test_init_and_verified_capabilities();
    puts("test_capabilities: PASS");
    return 0;
}
