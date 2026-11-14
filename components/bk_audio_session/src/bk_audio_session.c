/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_audio_session.h"
#include "bk_board.h"
#include "bk_port_mem.h"
#include "bk_port_os.h"

#include <string.h>

struct bk_audio_session {
    bk_port_mutex_t lock;
    int16_t *ring_samples;
    size_t capacity_frames;
    size_t head;
    size_t tail;
    size_t count;
    bool aec_enabled;
};

bk_err_t bk_audio_session_create(const bk_audio_session_config_t *cfg,
                                 bk_audio_session_handle_t *out_sess)
{
    if (cfg == NULL || out_sess == NULL || cfg->downlink_ring_frames == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    bk_err_t cap_err = bk_board_require_cap(BK_BOARD_CAP_AUDIO_CODEC, false);
    if (cap_err != BK_OK) {
        return cap_err;
    }

    struct bk_audio_session *s = (struct bk_audio_session *)bk_port_calloc(
        1, sizeof(*s), BK_MEM_CAP_DEFAULT, "bk_audio_sess");
    if (s == NULL) {
        return BK_ERR_NO_MEM;
    }

    s->ring_samples = (int16_t *)bk_port_calloc(
        cfg->downlink_ring_frames * BK_AUDIO_PCM_20MS_SAMPLES,
        sizeof(int16_t),
        BK_MEM_CAP_SRAM,
        "bk_audio_ring");
    if (s->ring_samples == NULL) {
        bk_port_free(s);
        return BK_ERR_NO_MEM;
    }

    bk_err_t err = bk_port_mutex_create(false, &s->lock);
    if (err != BK_OK) {
        bk_port_free(s->ring_samples);
        bk_port_free(s);
        return err;
    }

    s->capacity_frames = cfg->downlink_ring_frames;
    s->aec_enabled = cfg->enable_aec;
    *out_sess = s;
    return BK_OK;
}

void bk_audio_session_destroy(bk_audio_session_handle_t sess)
{
    if (sess == NULL) {
        return;
    }
    if (sess->lock != NULL) {
        bk_port_mutex_destroy(sess->lock);
    }
    bk_port_free(sess->ring_samples);
    bk_port_free(sess);
}

bk_err_t bk_audio_session_write_downlink_20ms(bk_audio_session_handle_t sess,
                                              const int16_t *pcm_320,
                                              uint32_t timeout_ms)
{
    if (sess == NULL || pcm_320 == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    uint64_t deadline = bk_port_get_time_ms() + timeout_ms;
    for (;;) {
        bk_port_mutex_lock(sess->lock, UINT32_MAX);
        if (sess->count < sess->capacity_frames) {
            int16_t *dst = sess->ring_samples + (sess->tail * BK_AUDIO_PCM_20MS_SAMPLES);
            memcpy(dst, pcm_320, BK_AUDIO_PCM_20MS_BYTES);
            sess->tail = (sess->tail + 1U) % sess->capacity_frames;
            sess->count += 1U;
            bk_port_mutex_unlock(sess->lock);
            return BK_OK;
        }
        bk_port_mutex_unlock(sess->lock);

        if (timeout_ms == 0U || bk_port_get_time_ms() >= deadline) {
            return BK_ERR_AUDIO_RINGBUF_FULL;
        }
        bk_port_sleep_ms(1);
    }
}

bk_err_t bk_audio_session_read_downlink_20ms(bk_audio_session_handle_t sess,
                                             int16_t *out_pcm_320)
{
    if (sess == NULL || out_pcm_320 == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_port_mutex_lock(sess->lock, UINT32_MAX);
    if (sess->count == 0U) {
        bk_port_mutex_unlock(sess->lock);
        return BK_ERR_NOT_FOUND;
    }
    const int16_t *src = sess->ring_samples + (sess->head * BK_AUDIO_PCM_20MS_SAMPLES);
    memcpy(out_pcm_320, src, BK_AUDIO_PCM_20MS_BYTES);
    sess->head = (sess->head + 1U) % sess->capacity_frames;
    sess->count -= 1U;
    bk_port_mutex_unlock(sess->lock);
    return BK_OK;
}

size_t bk_audio_session_get_queued_frames(bk_audio_session_handle_t sess)
{
    if (sess == NULL) {
        return 0U;
    }
    bk_port_mutex_lock(sess->lock, UINT32_MAX);
    size_t c = sess->count;
    bk_port_mutex_unlock(sess->lock);
    return c;
}

bool bk_audio_session_is_aec_enabled(bk_audio_session_handle_t sess)
{
    return sess != NULL && sess->aec_enabled;
}
