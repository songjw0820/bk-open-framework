/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_audio_mixer.h"
#include "bk_audio_session.h"
#include "bk_board.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(bk_board_init(BK_BOARD_PROFILE_ROBOT) == BK_OK);

    bk_audio_session_config_t cfg = {
        .downlink_ring_frames = 4,
        .enable_aec = true,
    };
    bk_audio_session_handle_t sess = NULL;
    assert(bk_audio_session_create(&cfg, &sess) == BK_OK);

    int16_t voice[BK_AUDIO_PCM_20MS_SAMPLES] = {1000};
    int16_t prompt[BK_AUDIO_PCM_20MS_SAMPLES] = {2000};
    int16_t mixed[BK_AUDIO_PCM_20MS_SAMPLES] = {0};

    assert(bk_audio_mixer_mix_20ms(
               BK_AUDIO_MIX_MODE_DUCKING, voice, prompt, 64U, 256U, mixed) == BK_OK);
    assert(bk_audio_session_write_downlink_20ms(sess, mixed, 10) == BK_OK);
    assert(bk_audio_session_read_downlink_20ms(sess, mixed) == BK_OK);

    bk_audio_session_destroy(sess);
    bk_board_deinit();
    printf("PASS: example 03_audio_player (20ms PCM session + ducking mixer)\n");
    return 0;
}
