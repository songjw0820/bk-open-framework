/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_audio_mixer.h"

#include <string.h>

static inline int16_t clamp_s16(int32_t v)
{
    if (v > 32767) {
        return 32767;
    }
    if (v < -32768) {
        return -32768;
    }
    return (int16_t)v;
}

bk_err_t bk_audio_mixer_mix_20ms(bk_audio_mix_mode_t mode,
                                 const int16_t *voice_320,
                                 const int16_t *prompt_320,
                                 uint16_t voice_gain_q8,
                                 uint16_t prompt_gain_q8,
                                 int16_t *out_mixed_320)
{
    if (out_mixed_320 == NULL || (voice_320 == NULL && prompt_320 == NULL)) {
        return BK_ERR_INVALID_ARG;
    }

    if (mode == BK_AUDIO_MIX_MODE_PREEMPT && prompt_320 != NULL) {
        for (size_t i = 0; i < BK_AUDIO_PCM_20MS_SAMPLES; ++i) {
            int32_t p = ((int32_t)prompt_320[i] * (int32_t)prompt_gain_q8) / 256;
            out_mixed_320[i] = clamp_s16(p);
        }
        return BK_OK;
    }

    for (size_t i = 0; i < BK_AUDIO_PCM_20MS_SAMPLES; ++i) {
        int32_t v = voice_320 != NULL
                        ? (((int32_t)voice_320[i] * (int32_t)voice_gain_q8) / 256)
                        : 0;
        int32_t p = prompt_320 != NULL
                        ? (((int32_t)prompt_320[i] * (int32_t)prompt_gain_q8) / 256)
                        : 0;
        out_mixed_320[i] = clamp_s16(v + p);
    }
    return BK_OK;
}
