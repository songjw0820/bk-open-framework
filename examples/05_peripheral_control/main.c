/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_board.h"
#include "bk_button.h"
#include "bk_led_effect.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(bk_board_init(BK_BOARD_PROFILE_MINIMAL) == BK_OK);
    assert(bk_board_require_cap(BK_BOARD_CAP_BUTTON_GPIO, false) == BK_OK);
    assert(bk_board_require_cap(BK_BOARD_CAP_LED_GPIO, false) == BK_OK);

    bk_button_config_t bcfg = {
        .button_id = 0,
        .debounce_ms = 20,
        .double_click_window_ms = 200,
        .long_press_ms = 800,
    };
    bk_button_handle_t btn = NULL;
    assert(bk_button_create(&bcfg, &btn) == BK_OK);

    bk_led_handle_t led = NULL;
    assert(bk_led_create(&led) == BK_OK);

    bk_button_event_t evt = BK_BUTTON_EVT_NONE;
    assert(bk_button_feed_sample(btn, true, 10, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, true, 35, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, false, 60, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, false, 85, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, false, 300, &evt) == BK_OK);
    assert(evt == BK_BUTTON_EVT_SINGLE_CLICK);

    assert(bk_led_set_effect(led, BK_LED_PRIO_NORMAL, BK_LED_PATTERN_SOLID) == BK_OK);
    assert(bk_led_get_active_pattern(led) == BK_LED_PATTERN_SOLID);

    bk_led_destroy(led);
    bk_button_destroy(btn);
    bk_board_deinit();

    printf("PASS: example 05_peripheral_control (button debounce FSM + priority LED effect)\n");
    return 0;
}
