#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bk_board.h"
#include "bk_peripheral.h"

static bk_err_t read_callback(void *context, void *data, size_t size, size_t *done)
{
    (void)data;
    if (context == NULL) return BK_ERR_INVALID_ARG;
    if (done != NULL) *done = size;
    return *(const bk_err_t *)context;
}

static bk_err_t write_callback(void *context, const void *data, size_t size, size_t *done)
{
    (void)data;
    if (context == NULL) return BK_ERR_INVALID_ARG;
    if (done != NULL) *done = size;
    return *(const bk_err_t *)context;
}

static void test_capabilities(void)
{
    bk_peripheral_info_t info;
    assert(!bk_peripheral_has_capability(BK_PERIPHERAL_CP_UART0));
    assert(bk_peripheral_query(BK_PERIPHERAL_AUDIO, &info) == BK_ERR_NOT_INITIALIZED);
    assert(bk_peripheral_query((bk_peripheral_id_t)-1, &info) == BK_ERR_NOT_FOUND);
    assert(bk_peripheral_query(BK_PERIPHERAL_AUDIO, NULL) == BK_ERR_INVALID_ARG);
    assert(bk_board_init(&(bk_board_config_t){0}) == BK_OK);
    assert(bk_peripheral_has_capability(BK_PERIPHERAL_CP_UART0));
    assert(bk_peripheral_query(BK_PERIPHERAL_CP_UART0, &info) == BK_OK);
    assert(info.available && !info.open);
}

static void test_lifecycle_and_callback_error_recovery(void)
{
    bk_peripheral_handle_t *handle = NULL;
    bk_err_t callback_result = BK_ERR_BUSY;
    bk_peripheral_config_t config = {
        .context = &callback_result,
        .read = read_callback,
        .write = write_callback,
    };
    size_t done = 123;
    char data[4] = {0};
    assert(bk_peripheral_open(BK_PERIPHERAL_CP_UART0, NULL, &handle) == BK_ERR_INVALID_ARG);
    assert(bk_peripheral_open(BK_PERIPHERAL_CP_UART0, &config, &handle) == BK_OK);
    assert(handle != NULL);
    assert(bk_peripheral_open(BK_PERIPHERAL_CP_UART0, &config, &handle) == BK_ERR_ALREADY_EXISTS);
    assert(bk_peripheral_read(handle, data, sizeof(data), &done) == BK_ERR_BUSY);
    assert(done == sizeof(data));
    callback_result = BK_OK;
    assert(bk_peripheral_write(handle, data, sizeof(data), &done) == BK_OK);
    assert(done == sizeof(data));
    assert(bk_peripheral_read(handle, NULL, 1, &done) == BK_ERR_INVALID_ARG);
    assert(bk_peripheral_close(handle) == BK_OK);
    assert(bk_peripheral_close(handle) == BK_ERR_NOT_FOUND);
}

static void test_host_mock_and_board_peripherals(void)
{
    bk_peripheral_handle_t *uart = NULL;
    const char tx[] = "PING_CP_UART0";
    char rx[32] = {0};
    size_t done = 0;
    uint8_t level = 0;

    assert(bk_peripheral_open(BK_PERIPHERAL_CP_UART0,
                              bk_peripheral_host_mock_config(BK_PERIPHERAL_CP_UART0),
                              &uart) == BK_OK);
    assert(bk_peripheral_write(uart, tx, sizeof(tx), &done) == BK_OK);
    assert(done == sizeof(tx));
    assert(bk_peripheral_read(uart, rx, sizeof(rx), &done) == BK_OK);
    assert(done == sizeof(tx));
    assert(strcmp(rx, tx) == 0);
    assert(bk_peripheral_close(uart) == BK_OK);

    assert(bk_peripheral_gpio_set_level(12U, 1U) == BK_OK);
    assert(bk_peripheral_gpio_get_level(12U, &level) == BK_OK);
    assert(level == 1U);
    assert(bk_peripheral_gpio_set_level(99U, 1U) == BK_ERR_INVALID_ARG);

    assert(bk_peripheral_set_led(BK_LED_STATE_ON) == BK_ERR_NOT_SUPPORTED);
    assert(bk_peripheral_set_motor(75U) == BK_ERR_NOT_SUPPORTED);

    assert(bk_board_set_capability_enabled(BK_BOARD_CAPABILITY_LED, true) == BK_OK);
    assert(bk_board_set_capability_enabled(BK_BOARD_CAPABILITY_VIBRATION, true) == BK_OK);

    assert(bk_peripheral_set_led(BK_LED_STATE_ON) == BK_OK);
    assert(bk_peripheral_get_led() == BK_LED_STATE_ON);
    assert(bk_peripheral_set_motor(75U) == BK_OK);
    assert(bk_peripheral_get_motor() == 75U);
    assert(bk_peripheral_set_motor(101U) == BK_ERR_INVALID_ARG);
}

int main(void)
{
    test_capabilities();
    test_lifecycle_and_callback_error_recovery();
    test_host_mock_and_board_peripherals();
    puts("test_peripheral_caps: PASS");
    return 0;
}
