#ifndef BK_PERIPHERAL_H
#define BK_PERIPHERAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Names describe a logical device; chip and board macros do not leak here. */
typedef enum {
    BK_PERIPHERAL_CP_UART0 = 0,
    BK_PERIPHERAL_AUDIO,
    BK_PERIPHERAL_CAMERA,
    BK_PERIPHERAL_DISPLAY,
    BK_PERIPHERAL_INPUT,
    BK_PERIPHERAL_LED,
    BK_PERIPHERAL_VIBRATION,
    BK_PERIPHERAL_TOF,
    BK_PERIPHERAL_NFC,
    BK_PERIPHERAL_COUNT
} bk_peripheral_id_t;

#define BK_PERIPHERAL_UART0 BK_PERIPHERAL_CP_UART0

typedef struct bk_peripheral_handle bk_peripheral_handle_t;

typedef bk_err_t (*bk_peripheral_read_fn)(void *context, void *data,
                                           size_t size, size_t *read_size);
typedef bk_err_t (*bk_peripheral_write_fn)(void *context, const void *data,
                                            size_t size, size_t *written_size);

typedef struct {
    void *context;
    bk_peripheral_read_fn read;
    bk_peripheral_write_fn write;
} bk_peripheral_config_t;

typedef struct {
    bk_peripheral_id_t id;
    bool available;
    bool open;
} bk_peripheral_info_t;

bool bk_peripheral_has_capability(bk_peripheral_id_t id);
const bk_peripheral_config_t *bk_peripheral_default_config(bk_peripheral_id_t id);
bk_err_t bk_peripheral_query(bk_peripheral_id_t id, bk_peripheral_info_t *info);
bk_err_t bk_peripheral_open(bk_peripheral_id_t id,
                            const bk_peripheral_config_t *config,
                            bk_peripheral_handle_t **handle);
bk_err_t bk_peripheral_read(bk_peripheral_handle_t *handle, void *data,
                            size_t size, size_t *read_size);
bk_err_t bk_peripheral_write(bk_peripheral_handle_t *handle, const void *data,
                             size_t size, size_t *written_size);
bk_err_t bk_peripheral_close(bk_peripheral_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif
