#include "bk_peripheral.h"

#include <string.h>
#include "bk_board.h"

#if (defined(BEKEN_PLATFORM) || defined(CONFIG_CMAKE)) && !defined(CONFIG_SOC_SMP) && !defined(BK_ARMINO_SUBSYSTEM_AP)
#define BK_PERIPHERAL_HAS_CP_UART0_HW 1
#include <driver/uart.h>
#define BK_PERIPHERAL_UART_TIMEOUT_MS 100U
#else
#define BK_PERIPHERAL_HAS_CP_UART0_HW 0
#endif

#define BK_PERIPHERAL_MAX_OPEN 4U

struct bk_peripheral_handle {
    bool in_use;
    bool hw_initialized;
    bk_peripheral_id_t id;
    bk_peripheral_config_t config;
};

static struct bk_peripheral_handle g_handles[BK_PERIPHERAL_MAX_OPEN];

#if BK_PERIPHERAL_HAS_CP_UART0_HW
static bk_err_t armino_uart0_read(void *context, void *data,
                                  size_t size, size_t *read_size)
{
    int count;
    (void)context;
    if (size == 0U) {
        if (read_size != NULL) {
            *read_size = 0U;
        }
        return BK_OK;
    }
    count = bk_uart_read_bytes(UART_ID_0, data, (uint32_t)size,
                               BK_PERIPHERAL_UART_TIMEOUT_MS);
    if (count < 0) {
        return (bk_err_t)count;
    }
    if (read_size != NULL) {
        *read_size = (size_t)count;
    }
    return BK_OK;
}

static bk_err_t armino_uart0_write(void *context, const void *data,
                                   size_t size, size_t *written_size)
{
    bk_err_t err;
    (void)context;
    if (size == 0U) {
        if (written_size != NULL) {
            *written_size = 0U;
        }
        return BK_OK;
    }
    err = bk_uart_write_bytes(UART_ID_0, data, (uint32_t)size);
    if (err == BK_OK && written_size != NULL) {
        *written_size = size;
    }
    return err;
}

static const bk_peripheral_config_t g_armino_uart0_config = {
    .context = NULL,
    .read = armino_uart0_read,
    .write = armino_uart0_write,
};
#endif

const bk_peripheral_config_t *bk_peripheral_default_config(bk_peripheral_id_t id)
{
#if BK_PERIPHERAL_HAS_CP_UART0_HW
    if (id == BK_PERIPHERAL_CP_UART0) {
        return &g_armino_uart0_config;
    }
#else
    (void)id;
#endif
    return NULL;
}

static bk_err_t map_capability(bk_peripheral_id_t id,
                                bk_board_capability_t *capability)
{
    if (capability == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    switch (id) {
    case BK_PERIPHERAL_CP_UART0:
        *capability = BK_BOARD_CAPABILITY_CP_UART0;
        return BK_OK;
    case BK_PERIPHERAL_AUDIO:
        *capability = BK_BOARD_CAPABILITY_AUDIO;
        return BK_OK;
    case BK_PERIPHERAL_CAMERA:
        *capability = BK_BOARD_CAPABILITY_CAMERA;
        return BK_OK;
    case BK_PERIPHERAL_DISPLAY:
        *capability = BK_BOARD_CAPABILITY_DISPLAY;
        return BK_OK;
    case BK_PERIPHERAL_INPUT:
        *capability = BK_BOARD_CAPABILITY_INPUT;
        return BK_OK;
    case BK_PERIPHERAL_LED:
        *capability = BK_BOARD_CAPABILITY_LED;
        return BK_OK;
    case BK_PERIPHERAL_VIBRATION:
        *capability = BK_BOARD_CAPABILITY_VIBRATION;
        return BK_OK;
    case BK_PERIPHERAL_TOF:
        *capability = BK_BOARD_CAPABILITY_TOF;
        return BK_OK;
    case BK_PERIPHERAL_NFC:
        *capability = BK_BOARD_CAPABILITY_NFC;
        return BK_OK;
    default:
        return BK_ERR_NOT_FOUND;
    }
}

bool bk_peripheral_has_capability(bk_peripheral_id_t id)
{
    bk_board_capability_t capability;
    return map_capability(id, &capability) == BK_OK &&
           bk_board_has_capability(capability);
}

bk_err_t bk_peripheral_query(bk_peripheral_id_t id, bk_peripheral_info_t *info)
{
    if (info == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (map_capability(id, &(bk_board_capability_t){0}) != BK_OK) {
        return BK_ERR_NOT_FOUND;
    }
    if (bk_board_get_info() == NULL) {
        return BK_ERR_NOT_INITIALIZED;
    }
    info->id = id;
    info->available = bk_peripheral_has_capability(id);
    info->open = false;
    for (size_t i = 0; i < BK_PERIPHERAL_MAX_OPEN; ++i) {
        if (g_handles[i].in_use && g_handles[i].id == id) {
            info->open = true;
            break;
        }
    }
    return info->available ? BK_OK : BK_ERR_NOT_SUPPORTED;
}

bk_err_t bk_peripheral_open(bk_peripheral_id_t id,
                            const bk_peripheral_config_t *config,
                            bk_peripheral_handle_t **handle)
{
    if (handle == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (!bk_peripheral_has_capability(id)) {
        bk_board_capability_t capability;
        if (map_capability(id, &capability) == BK_OK &&
            bk_board_get_info() == NULL) {
            return BK_ERR_NOT_INITIALIZED;
        }
        return map_capability(id, &capability) == BK_ERR_NOT_FOUND
            ? BK_ERR_NOT_FOUND : BK_ERR_NOT_SUPPORTED;
    }
    if (config == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    for (size_t i = 0; i < BK_PERIPHERAL_MAX_OPEN; ++i) {
        if (g_handles[i].in_use && g_handles[i].id == id) {
            return BK_ERR_ALREADY_EXISTS;
        }
    }
    for (size_t i = 0; i < BK_PERIPHERAL_MAX_OPEN; ++i) {
        if (!g_handles[i].in_use) {
            bool hw_init = false;
#if BK_PERIPHERAL_HAS_CP_UART0_HW
            if (id == BK_PERIPHERAL_CP_UART0 &&
                config == &g_armino_uart0_config) {
                const uart_config_t uart_cfg = {
                    .baud_rate = UART_BAUDRATE_115200,
                    .data_bits = UART_DATA_8_BITS,
                    .parity = UART_PARITY_NONE,
                    .stop_bits = UART_STOP_BITS_1,
                    .flow_ctrl = UART_FLOWCTRL_DISABLE,
                    .src_clk = UART_SCLK_XTAL_26M,
                };
                bk_err_t sdk_err = bk_uart_driver_init();
                if (sdk_err != BK_OK) {
                    return sdk_err;
                }
                sdk_err = bk_uart_init(UART_ID_0, &uart_cfg);
                if (sdk_err != BK_OK) {
                    return sdk_err;
                }
                hw_init = true;
            }
#endif
            g_handles[i].in_use = true;
            g_handles[i].hw_initialized = hw_init;
            g_handles[i].id = id;
            g_handles[i].config = *config;
            *handle = &g_handles[i];
            return BK_OK;
        }
    }
    return BK_ERR_LIMIT;
}

static bk_err_t validate_io(const bk_peripheral_handle_t *handle,
                            const void *data, size_t size)
{
    if (handle == NULL || !handle->in_use) {
        return BK_ERR_NOT_FOUND;
    }
    if (size != 0U && data == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    return BK_OK;
}

bk_err_t bk_peripheral_read(bk_peripheral_handle_t *handle, void *data,
                            size_t size, size_t *read_size)
{
    bk_err_t err = validate_io(handle, data, size);
    if (read_size != NULL) {
        *read_size = 0U;
    }
    if (err != BK_OK) {
        return err;
    }
    if (handle->config.read == NULL) {
        return BK_ERR_NOT_SUPPORTED;
    }
    return handle->config.read(handle->config.context, data, size, read_size);
}

bk_err_t bk_peripheral_write(bk_peripheral_handle_t *handle, const void *data,
                             size_t size, size_t *written_size)
{
    bk_err_t err = validate_io(handle, data, size);
    if (written_size != NULL) {
        *written_size = 0U;
    }
    if (err != BK_OK) {
        return err;
    }
    if (handle->config.write == NULL) {
        return BK_ERR_NOT_SUPPORTED;
    }
    return handle->config.write(handle->config.context, data, size, written_size);
}

bk_err_t bk_peripheral_close(bk_peripheral_handle_t *handle)
{
    if (handle == NULL || !handle->in_use) {
        return BK_ERR_NOT_FOUND;
    }
#if BK_PERIPHERAL_HAS_CP_UART0_HW
    if (handle->hw_initialized && handle->id == BK_PERIPHERAL_CP_UART0) {
        (void)bk_uart_deinit(UART_ID_0);
    }
#endif
    memset(handle, 0, sizeof(*handle));
    return BK_OK;
}
