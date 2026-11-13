#include "bk_board.h"
#include "bk_core.h"
#include "bk_peripheral.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    bk_core_config_t core_cfg = {.max_subscribers = BK_CORE_DEFAULT_MAX_SUBSCRIBERS};
    bk_board_config_t board_cfg = {.board_name = "bk7259_robot"};
    const bk_peripheral_config_t *uart_cfg;
    bk_peripheral_handle_t *uart_handle = NULL;
    const uint8_t tx_cmd[] = "AT+ROBOT_STATUS?\r\n";
    uint8_t rx_buf[32] = {0};
    size_t written = 0;
    size_t read_bytes = 0;
    uint8_t gpio_level = 0;

    if (bk_core_init(&core_cfg) != BK_OK || bk_core_start() != BK_OK) {
        return 1;
    }
    if (bk_board_init(&board_cfg) != BK_OK) {
        return 2;
    }

    uart_cfg = bk_peripheral_default_config(BK_PERIPHERAL_CP_UART0);
    if (uart_cfg == NULL) {
        uart_cfg = bk_peripheral_host_mock_config(BK_PERIPHERAL_CP_UART0);
    }
    if (bk_peripheral_open(BK_PERIPHERAL_CP_UART0, uart_cfg, &uart_handle) != BK_OK ||
        bk_peripheral_write(uart_handle, tx_cmd, sizeof(tx_cmd) - 1, &written) != BK_OK ||
        written != sizeof(tx_cmd) - 1 ||
        bk_peripheral_read(uart_handle, rx_buf, sizeof(rx_buf) - 1, &read_bytes) != BK_OK ||
        read_bytes != written ||
        bk_peripheral_close(uart_handle) != BK_OK) {
        return 3;
    }

    if (bk_board_set_capability_enabled(BK_BOARD_CAPABILITY_LED, true) != BK_OK ||
        bk_board_set_capability_enabled(BK_BOARD_CAPABILITY_VIBRATION, true) != BK_OK) {
        return 4;
    }

    if (bk_peripheral_gpio_set_level(12, 1) != BK_OK ||
        bk_peripheral_gpio_get_level(12, &gpio_level) != BK_OK ||
        gpio_level != 1) {
        return 5;
    }

    if (bk_peripheral_set_led(BK_LED_STATE_ON) != BK_OK ||
        bk_peripheral_get_led() != BK_LED_STATE_ON) {
        return 6;
    }

    if (bk_peripheral_set_motor(80) != BK_OK ||
        bk_peripheral_get_motor() != 80) {
        return 7;
    }

    printf("05_peripheral_control: uart_echo=%zu gpio12=%u led=%d motor=%u%%\n",
           read_bytes,
           (unsigned)gpio_level,
           (int)bk_peripheral_get_led(),
           (unsigned)bk_peripheral_get_motor());

    if (bk_board_deinit() != BK_OK || bk_core_deinit() != BK_OK) {
        return 8;
    }
    return 0;
}
