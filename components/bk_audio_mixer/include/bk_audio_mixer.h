/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_AUDIO_MIXER_H
#define BK_OPEN_FRAMEWORK_BK_AUDIO_MIXER_H

#include <stddef.h>
#include <stdint.h>
#include "bk_audio_session.h"
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BK_AUDIO_MIX_MODE_DUCKING = 0,
    BK_AUDIO_MIX_MODE_PREEMPT = 1,
} bk_audio_mix_mode_t;

/*
 * Mixes 20ms voice PCM and 20ms prompt-tone PCM (320 int16_t samples each).
 * Gains are in Q8 fixed-point (256 = 1.0x, 64 = 0.25x).
 * Applies [-32768, 32767] saturation arithmetic.
 */
bk_err_t bk_audio_mixer_mix_20ms(bk_audio_mix_mode_t mode,
                                 const int16_t *voice_320,
                                 const int16_t *prompt_320,
                                 uint16_t voice_gain_q8,
                                 uint16_t prompt_gain_q8,
                                 int16_t *out_mixed_320);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_AUDIO_MIXER_H */
