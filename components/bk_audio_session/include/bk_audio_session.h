/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_AUDIO_SESSION_H
#define BK_OPEN_FRAMEWORK_BK_AUDIO_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bk_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BK_AUDIO_SAMPLE_RATE_HZ     16000U
#define BK_AUDIO_PCM_20MS_SAMPLES   320U
#define BK_AUDIO_PCM_20MS_BYTES     640U

typedef struct bk_audio_session *bk_audio_session_handle_t;

typedef struct {
    size_t downlink_ring_frames;
    bool enable_aec;
} bk_audio_session_config_t;

bk_err_t bk_audio_session_create(const bk_audio_session_config_t *cfg,
                                 bk_audio_session_handle_t *out_sess);
void bk_audio_session_destroy(bk_audio_session_handle_t sess);

/*
 * Writes a fixed 20ms PCM frame (640 bytes = 320 int16_t samples @ 16kHz Mono)
 * into the downlink ringbuffer. Returns BK_ERR_AUDIO_RINGBUF_FULL on backpressure.
 */
bk_err_t bk_audio_session_write_downlink_20ms(bk_audio_session_handle_t sess,
                                              const int16_t *pcm_320,
                                              uint32_t timeout_ms);

bk_err_t bk_audio_session_read_downlink_20ms(bk_audio_session_handle_t sess,
                                             int16_t *out_pcm_320);

size_t bk_audio_session_get_queued_frames(bk_audio_session_handle_t sess);
bool bk_audio_session_is_aec_enabled(bk_audio_session_handle_t sess);

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_AUDIO_SESSION_H */
