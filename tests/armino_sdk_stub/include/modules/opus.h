/* SPDX-License-Identifier: Apache-2.0 */
#ifndef ARMINO_STUB_MODULES_OPUS_H
#define ARMINO_STUB_MODULES_OPUS_H

#include <stdint.h>

#define OPUS_OK               0
#define OPUS_APPLICATION_VOIP 2048

typedef struct OpusEncoder OpusEncoder;
typedef struct OpusDecoder OpusDecoder;

OpusEncoder *opus_encoder_create(int32_t Fs, int channels, int application, int *error);
int32_t opus_encode(OpusEncoder *st,
                    const int16_t *pcm,
                    int frame_size,
                    unsigned char *data,
                    int32_t max_data_bytes);
void opus_encoder_destroy(OpusEncoder *st);

OpusDecoder *opus_decoder_create(int32_t Fs, int channels, int *error);
int32_t opus_decode(OpusDecoder *st,
                    const unsigned char *data,
                    int32_t len,
                    int16_t *pcm,
                    int frame_size,
                    int decode_fec);
void opus_decoder_destroy(OpusDecoder *st);

#endif /* ARMINO_STUB_MODULES_OPUS_H */
