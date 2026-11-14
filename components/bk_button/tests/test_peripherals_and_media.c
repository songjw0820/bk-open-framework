/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_audio_mixer.h"
#include "bk_audio_session.h"
#include "bk_board.h"
#include "bk_button.h"
#include "bk_led_effect.h"
#include "bk_port_cache.h"
#include "bk_port_mem.h"
#include "bk_ui_core.h"
#include "bk_video_frame_pool.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_button_debounce_and_clicks(void)
{
    bk_button_config_t cfg = {
        .button_id = 1,
        .debounce_ms = 20,
        .double_click_window_ms = 200,
        .long_press_ms = 800,
    };
    bk_button_handle_t btn = NULL;
    assert(bk_button_create(&cfg, &btn) == BK_OK);

    bk_button_event_t evt = BK_BUTTON_EVT_NONE;
    /* Glitch < 20ms must be ignored */
    assert(bk_button_feed_sample(btn, true, 10, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, false, 20, &evt) == BK_OK);
    assert(evt == BK_BUTTON_EVT_NONE);

    /* Single click: press @ 100ms, debounce @ 125ms, release @ 160ms, debounce @ 185ms, window expires @ 400ms */
    assert(bk_button_feed_sample(btn, true, 100, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, true, 125, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, false, 160, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, false, 185, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, false, 400, &evt) == BK_OK);
    assert(evt == BK_BUTTON_EVT_SINGLE_CLICK);

    /* Double click: press1 500..560, press2 620..680 */
    assert(bk_button_feed_sample(btn, true, 500, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, true, 525, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, false, 560, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, false, 585, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, true, 620, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, true, 645, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, false, 680, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, false, 705, &evt) == BK_OK);
    assert(evt == BK_BUTTON_EVT_DOUBLE_CLICK);

    /* Long press: press @ 1000ms, held past 1825ms */
    assert(bk_button_feed_sample(btn, true, 1000, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, true, 1025, &evt) == BK_OK);
    assert(bk_button_feed_sample(btn, true, 1830, &evt) == BK_OK);
    assert(evt == BK_BUTTON_EVT_LONG_PRESS);

    bk_button_destroy(btn);

    /* ADC ladder key with Schmitt trigger hysteresis */
    bk_button_config_t adc_cfg = {
        .button_id = 2,
        .debounce_ms = 20,
        .double_click_window_ms = 200,
        .long_press_ms = 800,
        .adc_target_mv = 1650U,
        .adc_tolerance_mv = 80U,
    };
    bk_button_handle_t adc_btn = NULL;
    assert(bk_button_create(&adc_cfg, &adc_btn) == BK_OK);
    /* Press at 1680mV (within 1650 +/- 80mV), slight drift to 1745mV (within 80+25=105mV hysteresis) stays pressed */
    assert(bk_button_feed_adc_mv(adc_btn, 1680U, 100U, &evt) == BK_OK);
    assert(bk_button_feed_adc_mv(adc_btn, 1745U, 125U, &evt) == BK_OK);
    assert(bk_button_is_pressed(adc_btn));
    assert(bk_button_feed_adc_mv(adc_btn, 3300U, 160U, &evt) == BK_OK);
    assert(bk_button_feed_adc_mv(adc_btn, 3300U, 185U, &evt) == BK_OK);
    assert(bk_button_feed_adc_mv(adc_btn, 3300U, 400U, &evt) == BK_OK);
    assert(evt == BK_BUTTON_EVT_SINGLE_CLICK);
    assert(bk_button_get_repeat_count(adc_btn) == 1U);
    bk_button_destroy(adc_btn);
}

static void test_led_priority_preemption(void)
{
    bk_led_handle_t led = NULL;
    assert(bk_led_create(&led) == BK_OK);
    assert(bk_led_get_active_pattern(led) == BK_LED_PATTERN_OFF);

    assert(bk_led_set_effect(led, BK_LED_PRIO_LOW, BK_LED_PATTERN_BREATHE) == BK_OK);
    assert(bk_led_get_active_pattern(led) == BK_LED_PATTERN_BREATHE);

    /* Timed high-priority fast blink for 500ms @ t=1000ms */
    assert(bk_led_set_effect_timed(led, BK_LED_PRIO_CRITICAL, BK_LED_PATTERN_FAST_BLINK, 1000ULL, 500U) == BK_OK);
    assert(bk_led_get_active_pattern(led) == BK_LED_PATTERN_FAST_BLINK);

    uint16_t pwm = 0U;
    assert(bk_led_tick_ms(led, 1050ULL, &pwm) == BK_OK && pwm == 1000U);
    assert(bk_led_tick_ms(led, 1150ULL, &pwm) == BK_OK && pwm == 0U);

    /* At t=1500ms, timed CRITICAL effect auto-expires and restores LOW BREATHE */
    assert(bk_led_tick_ms(led, 1500ULL, &pwm) == BK_OK);
    assert(bk_led_get_active_pattern(led) == BK_LED_PATTERN_BREATHE);
    assert(bk_led_tick_ms(led, 1000ULL, &pwm) == BK_OK && pwm == 1000U); /* Peak of 2000ms breathe */

    bk_led_destroy(led);
}

static void test_audio_session_and_mixer(void)
{
    assert(bk_board_init(BK_BOARD_PROFILE_ROBOT) == BK_OK);

    bk_audio_session_config_t acfg = {
        .downlink_ring_frames = 2,
        .enable_aec = true,
    };
    bk_audio_session_handle_t sess = NULL;
    assert(bk_audio_session_create(&acfg, &sess) == BK_OK);
    assert(bk_audio_session_is_aec_enabled(sess));

    int16_t voice[BK_AUDIO_PCM_20MS_SAMPLES];
    int16_t prompt[BK_AUDIO_PCM_20MS_SAMPLES];
    int16_t mixed[BK_AUDIO_PCM_20MS_SAMPLES];
    for (size_t i = 0; i < BK_AUDIO_PCM_20MS_SAMPLES; ++i) {
        voice[i] = 24000;
        prompt[i] = 20000;
    }

    /* Ducking with 1.0x + 1.0x must saturate at 32767 without overflow wrap */
    assert(bk_audio_mixer_mix_20ms(
               BK_AUDIO_MIX_MODE_DUCKING, voice, prompt, 256U, 256U, mixed) == BK_OK);
    assert(mixed[0] == 32767);

    /* Write 2 frames succeeds; 3rd frame with timeout 0 returns BK_ERR_AUDIO_RINGBUF_FULL */
    assert(bk_audio_session_write_downlink_20ms(sess, mixed, 0) == BK_OK);
    assert(bk_audio_session_write_downlink_20ms(sess, mixed, 0) == BK_OK);
    assert(bk_audio_session_write_downlink_20ms(sess, mixed, 0) == BK_ERR_AUDIO_RINGBUF_FULL);

    int16_t out_pcm[BK_AUDIO_PCM_20MS_SAMPLES] = {0};
    assert(bk_audio_session_read_downlink_20ms(sess, out_pcm) == BK_OK);
    assert(out_pcm[0] == 32767);

    bk_audio_session_destroy(sess);
    bk_board_deinit();
}

static void test_video_frame_pool_and_ui_lock(void)
{
    assert(bk_board_init(BK_BOARD_PROFILE_ROBOT) == BK_OK);
    bk_port_reset_cache_stats();

    bk_video_frame_pool_config_t vcfg = {
        .frame_count = 2,
        .frame_size_bytes = 153600, /* 320x240 RGB565 */
        .width = 320,
        .height = 240,
    };
    bk_video_frame_pool_handle_t pool = NULL;
    assert(bk_video_frame_pool_create(&vcfg, &pool) == BK_OK);
    assert(bk_video_frame_pool_get_free_count(pool) == 2U);

    bk_video_frame_t *f1 = NULL;
    bk_video_frame_t *f2 = NULL;
    bk_video_frame_t *f3 = NULL;
    assert(bk_video_frame_acquire(pool, &f1) == BK_OK);
    assert(bk_video_frame_acquire(pool, &f2) == BK_OK);
    assert(bk_video_frame_acquire(pool, &f3) == BK_ERR_NO_MEM);

    assert(((uintptr_t)f1->data % BK_FRAME_QUEUE_ALIGN_BYTES) == 0U);
    assert(bk_frame_retain(f1) == BK_OK);
    assert(bk_frame_get_refcount(f1) == 2U);

    f1->valid_len = 4096U;
    assert(bk_frame_sync_for_device(f1) == BK_OK);
    assert(bk_frame_sync_for_cpu(f1) == BK_OK);

    bk_port_cache_stats_t cstats;
    bk_port_get_cache_stats(&cstats);
    assert(cstats.clean_count == 1U && cstats.last_clean_bytes == 4096U);
    assert(cstats.invalidate_count == 1U && cstats.last_invalidate_bytes == 153600U);

    assert(bk_frame_release(f1) == BK_OK);
    assert(bk_frame_get_refcount(f1) == 1U);
    assert(bk_frame_release(f1) == BK_OK);
    assert(bk_frame_get_refcount(f1) == 0U);
    assert(bk_frame_release(f2) == BK_OK);
    assert(bk_video_frame_pool_get_free_count(pool) == 2U);

    bk_ui_core_handle_t ui = NULL;
    assert(bk_ui_core_create(&ui) == BK_OK);
    assert(bk_ui_lock(ui, 100) == BK_OK);
    assert(bk_ui_lock(ui, 100) == BK_OK);
    assert(bk_ui_get_lock_depth(ui) == 2U);
    assert(bk_ui_unlock(ui) == BK_OK);
    assert(bk_ui_unlock(ui) == BK_OK);
    assert(bk_ui_get_lock_depth(ui) == 0U);
    bk_ui_core_destroy(ui);

    bk_video_frame_pool_destroy(pool);
    bk_board_deinit();
}

int main(void)
{
    bk_port_reset_mem_stats();
    test_button_debounce_and_clicks();
    test_led_priority_preemption();
    test_audio_session_and_mixer();
    test_video_frame_pool_and_ui_lock();

    bk_port_mem_stats_t mstats;
    bk_port_get_mem_stats(&mstats);
    assert(mstats.current_allocated_bytes == 0U);
    assert(mstats.active_allocations == 0U);

    printf("PASS: test_peripherals_and_media (button FSM, LED prio, 20ms audio/mixer, 64B video pool, UI lock)\n");
    return 0;
}
