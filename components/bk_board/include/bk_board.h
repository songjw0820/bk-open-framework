#ifndef BK_BOARD_H
#define BK_BOARD_H

#include <stdbool.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t bk_gpio_pin_t;

typedef enum {
    BK_BOARD_CAPABILITY_CP_UART0 = 0,
    BK_BOARD_CAPABILITY_AUDIO,
    BK_BOARD_CAPABILITY_CAMERA,
    BK_BOARD_CAPABILITY_DISPLAY,
    BK_BOARD_CAPABILITY_INPUT,
    BK_BOARD_CAPABILITY_LED,
    BK_BOARD_CAPABILITY_VIBRATION,
    BK_BOARD_CAPABILITY_TOF,
    BK_BOARD_CAPABILITY_NFC,
    BK_BOARD_CAPABILITY_COUNT
} bk_board_capability_t;

#define BK_BOARD_CAP_UART0 BK_BOARD_CAPABILITY_CP_UART0
#define BK_BOARD_CAPABILITY_UART0 BK_BOARD_CAPABILITY_CP_UART0

typedef enum {
    BK_BOARD_PIN_CP_UART0_RX = 0,
    BK_BOARD_PIN_CP_UART0_TX,
    BK_BOARD_PIN_COUNT
} bk_board_pin_id_t;

#define BK_BOARD_PIN_UART0_RX BK_BOARD_PIN_CP_UART0_RX
#define BK_BOARD_PIN_UART0_TX BK_BOARD_PIN_CP_UART0_TX

typedef struct {
    const char *board_name;
} bk_board_config_t;

typedef struct {
    const char *name;
    const char *chip;
    const char *profile;
} bk_board_info_t;

bk_err_t bk_board_init(const bk_board_config_t *config);
bk_err_t bk_board_deinit(void);
bool bk_board_has_capability(bk_board_capability_t capability);
bk_err_t bk_board_set_capability_enabled(bk_board_capability_t capability, bool enabled);
const bk_board_info_t *bk_board_get_info(void);
bk_err_t bk_board_get_pin(bk_board_pin_id_t id, bk_gpio_pin_t *pin);

#ifdef __cplusplus
}
#endif

#endif
