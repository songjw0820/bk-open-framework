/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_protocol_xiaozhi.h"
#include "bk_port_mem.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(BK_OF_BUILDING_TARGET_BK7259)
#include <modules/opus.h>
#else
#include <dlfcn.h>
#endif

#define BK_OPUS_APPLICATION_VOIP 2048
#define BK_OPUS_OK               0

typedef struct OpusEncoder OpusEncoder;
typedef struct OpusDecoder OpusDecoder;

typedef OpusEncoder *(*fn_opus_encoder_create_t)(int32_t Fs, int channels, int application, int *error);
typedef int32_t (*fn_opus_encode_t)(OpusEncoder *st, const int16_t *pcm, int frame_size, unsigned char *data, int32_t max_data_bytes);
typedef void (*fn_opus_encoder_destroy_t)(OpusEncoder *st);

typedef OpusDecoder *(*fn_opus_decoder_create_t)(int32_t Fs, int channels, int *error);
typedef int (*fn_opus_decode_t)(OpusDecoder *st, const unsigned char *data, int32_t len, int16_t *pcm, int frame_size, int decode_fec);
typedef void (*fn_opus_decoder_destroy_t)(OpusDecoder *st);

struct bk_xiaozhi_session {
    bk_xiaozhi_config_t cfg;
    char session_id[BK_XIAOZHI_MAX_SESSION_ID_LEN];
    uint32_t negotiated_frame_ms;
    bool handshake_complete;
    int16_t uplink_accum[BK_XIAOZHI_OPUS_60MS_PCM_SAMPLES];
    uint32_t accum_20ms_count;

    /* Native libopus state */
    void *dl_handle;
    fn_opus_encoder_create_t fn_enc_create;
    fn_opus_encode_t fn_encode;
    fn_opus_encoder_destroy_t fn_enc_destroy;
    fn_opus_decoder_create_t fn_dec_create;
    fn_opus_decode_t fn_decode;
    fn_opus_decoder_destroy_t fn_dec_destroy;
    OpusEncoder *enc;
    OpusDecoder *dec;
    bool native_opus_ready;
};

/* --- Strictly bounded JSON scanner (never reads at or past end) --- */

static const char *skip_ws(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
        ++p;
    }
    return p;
}

/* Given p pointing at '"', returns pointer just after closing '"', or NULL on error */
static const char *skip_json_string_literal(const char *p, const char *end)
{
    if (p >= end || *p != '"') {
        return NULL;
    }
    ++p;
    while (p < end) {
        if (*p == '\\') {
            p += 2;
            if (p > end) {
                return NULL;
            }
            continue;
        }
        if (*p == '"') {
            return p + 1;
        }
        ++p;
    }
    return NULL;
}

/* Given p pointing at first char of a JSON value, returns pointer just after the value */
static const char *skip_json_value(const char *p, const char *end)
{
    p = skip_ws(p, end);
    if (p >= end) {
        return NULL;
    }
    if (*p == '"') {
        return skip_json_string_literal(p, end);
    }
    if (*p == '{' || *p == '[') {
        char open_ch = *p;
        char close_ch = (open_ch == '{') ? '}' : ']';
        int depth = 0;
        while (p < end) {
            if (*p == '"') {
                p = skip_json_string_literal(p, end);
                if (p == NULL) {
                    return NULL;
                }
                continue;
            }
            if (*p == open_ch) {
                ++depth;
            } else if (*p == close_ch) {
                --depth;
                if (depth == 0) {
                    return p + 1;
                }
            }
            ++p;
        }
        return NULL;
    }
    /* Primitive: number, true, false, null */
    while (p < end && *p != ',' && *p != '}' && *p != ']' &&
           *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
        ++p;
    }
    return p;
}

/*
 * Finds a direct child key of the JSON object in [obj_start, obj_end)
 * without descending into nested objects/arrays.
 * Returns true and sets [*val_start, *val_end) to the value slice.
 */
static bool json_find_top_level_member(const char *obj_start,
                                       const char *obj_end,
                                       const char *key,
                                       const char **out_val_start,
                                       const char **out_val_end)
{
    const char *p = skip_ws(obj_start, obj_end);
    if (p >= obj_end || *p != '{') {
        return false;
    }
    size_t key_len = strlen(key);
    ++p;

    while (p < obj_end) {
        p = skip_ws(p, obj_end);
        if (p < obj_end && *p == '}') {
            return false;
        }
        if (p >= obj_end || *p != '"') {
            return false;
        }
        const char *k_start = p + 1;
        const char *k_after = skip_json_string_literal(p, obj_end);
        if (k_after == NULL) {
            return false;
        }
        size_t k_len = (size_t)((k_after - 1) - k_start);

        p = skip_ws(k_after, obj_end);
        if (p >= obj_end || *p != ':') {
            return false;
        }
        ++p;

        const char *v_start = skip_ws(p, obj_end);
        const char *v_end = skip_json_value(v_start, obj_end);
        if (v_end == NULL) {
            return false;
        }

        if (k_len == key_len && memcmp(k_start, key, key_len) == 0) {
            *out_val_start = v_start;
            *out_val_end = v_end;
            return true;
        }

        p = skip_ws(v_end, obj_end);
        if (p < obj_end && *p == ',') {
            ++p;
        } else if (p < obj_end && *p == '}') {
            return false;
        } else {
            return false;
        }
    }
    return false;
}

static bool json_decode_string_slice(const char *val_start,
                                     const char *val_end,
                                     char *out_str,
                                     size_t out_cap)
{
    if (val_start == NULL || val_end == NULL || val_end - val_start < 2 ||
        *val_start != '"' || *(val_end - 1) != '"') {
        return false;
    }
    const char *p = val_start + 1;
    const char *end = val_end - 1;
    size_t j = 0U;
    while (p < end) {
        char ch = *p++;
        if (ch == '\\') {
            if (p >= end) {
                return false;
            }
            char esc = *p++;
            switch (esc) {
            case '"':  ch = '"'; break;
            case '\\': ch = '\\'; break;
            case '/':  ch = '/'; break;
            case 'n':  ch = '\n'; break;
            case 'r':  ch = '\r'; break;
            case 't':  ch = '\t'; break;
            default:   ch = esc; break;
            }
        }
        if (j + 1U >= out_cap) {
            return false;
        }
        out_str[j++] = ch;
    }
    out_str[j] = '\0';
    return true;
}

static bool json_decode_int_slice(const char *val_start,
                                  const char *val_end,
                                  int32_t *out_int)
{
    if (val_start == NULL || val_end == NULL || val_start >= val_end) {
        return false;
    }
    size_t len = (size_t)(val_end - val_start);
    if (len == 0U || len >= 24U) {
        return false;
    }
    char num_buf[24];
    memcpy(num_buf, val_start, len);
    num_buf[len] = '\0';
    if (num_buf[0] != '-' && !isdigit((unsigned char)num_buf[0])) {
        return false;
    }
    char *endptr = NULL;
    long v = strtol(num_buf, &endptr, 10);
    if (endptr == num_buf || *endptr != '\0') {
        return false;
    }
    *out_int = (int32_t)v;
    return true;
}

static bool json_escape_string(const char *in_str, char *out_buf, size_t out_cap)
{
    size_t j = 0U;
    for (size_t i = 0U; in_str[i] != '\0'; ++i) {
        unsigned char c = (unsigned char)in_str[i];
        if (c == '"' || c == '\\') {
            if (j + 2U >= out_cap) {
                return false;
            }
            out_buf[j++] = '\\';
            out_buf[j++] = (char)c;
        } else if (c < 0x20U) {
            if (j + 6U >= out_cap) {
                return false;
            }
            int n = snprintf(&out_buf[j], out_cap - j, "\\u%04x", (unsigned int)c);
            if (n != 6) {
                return false;
            }
            j += 6U;
        } else {
            if (j + 1U >= out_cap) {
                return false;
            }
            out_buf[j++] = (char)c;
        }
    }
    if (j >= out_cap) {
        return false;
    }
    out_buf[j] = '\0';
    return true;
}

/* --- Native libopus initialization --- */

static bk_err_t init_native_opus(struct bk_xiaozhi_session *xz)
{
#if defined(BK_OF_BUILDING_TARGET_BK7259)
    xz->fn_enc_create = opus_encoder_create;
    xz->fn_encode = opus_encode;
    xz->fn_enc_destroy = opus_encoder_destroy;
    xz->fn_dec_create = opus_decoder_create;
    xz->fn_decode = opus_decode;
    xz->fn_dec_destroy = opus_decoder_destroy;
#else
    static const char *const candidate_libs[] = {
        "libopus.so.0",
        "libopus.so",
        "/usr/lib/x86_64-linux-gnu/libopus.so.0",
        "/usr/lib/aarch64-linux-gnu/libopus.so.0",
        "libopus.0.dylib",
        "libopus.dylib",
        "/opt/homebrew/lib/libopus.0.dylib",
        "/opt/homebrew/lib/libopus.dylib",
        "/usr/local/lib/libopus.0.dylib",
        "/usr/local/lib/libopus.dylib",
    };
    for (size_t i = 0U; i < sizeof(candidate_libs) / sizeof(candidate_libs[0]); ++i) {
        xz->dl_handle = dlopen(candidate_libs[i], RTLD_LAZY | RTLD_LOCAL);
        if (xz->dl_handle != NULL) {
            break;
        }
    }
    if (xz->dl_handle == NULL) {
        return BK_ERR_NOT_SUPPORTED;
    }
    *(void **)(&xz->fn_enc_create)  = dlsym(xz->dl_handle, "opus_encoder_create");
    *(void **)(&xz->fn_encode)      = dlsym(xz->dl_handle, "opus_encode");
    *(void **)(&xz->fn_enc_destroy) = dlsym(xz->dl_handle, "opus_encoder_destroy");
    *(void **)(&xz->fn_dec_create)  = dlsym(xz->dl_handle, "opus_decoder_create");
    *(void **)(&xz->fn_decode)      = dlsym(xz->dl_handle, "opus_decode");
    *(void **)(&xz->fn_dec_destroy) = dlsym(xz->dl_handle, "opus_decoder_destroy");

    if (xz->fn_enc_create == NULL || xz->fn_encode == NULL || xz->fn_enc_destroy == NULL ||
        xz->fn_dec_create == NULL || xz->fn_decode == NULL || xz->fn_dec_destroy == NULL) {
        dlclose(xz->dl_handle);
        xz->dl_handle = NULL;
        return BK_ERR_NOT_SUPPORTED;
    }
#endif

    int enc_err = -1;
    int dec_err = -1;
    xz->enc = xz->fn_enc_create(
        (int32_t)BK_XIAOZHI_SAMPLE_RATE_HZ, (int)BK_XIAOZHI_CHANNELS, BK_OPUS_APPLICATION_VOIP, &enc_err);
    xz->dec = xz->fn_dec_create(
        (int32_t)BK_XIAOZHI_SAMPLE_RATE_HZ, (int)BK_XIAOZHI_CHANNELS, &dec_err);
    if (xz->enc == NULL || xz->dec == NULL || enc_err != BK_OPUS_OK || dec_err != BK_OPUS_OK) {
        if (xz->enc != NULL) {
            xz->fn_enc_destroy(xz->enc);
            xz->enc = NULL;
        }
        if (xz->dec != NULL) {
            xz->fn_dec_destroy(xz->dec);
            xz->dec = NULL;
        }
#if !defined(BK_OF_BUILDING_TARGET_BK7259)
        if (xz->dl_handle != NULL) {
            dlclose(xz->dl_handle);
            xz->dl_handle = NULL;
        }
#endif
        return BK_ERR_INVALID_STATE;
    }
    xz->native_opus_ready = true;
    return BK_OK;
}

bk_err_t bk_xiaozhi_create(const bk_xiaozhi_config_t *cfg, bk_xiaozhi_handle_t *out_xz)
{
    if (cfg == NULL || out_xz == NULL || cfg->ws == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    struct bk_xiaozhi_session *xz = (struct bk_xiaozhi_session *)bk_port_calloc(
        1, sizeof(*xz), BK_MEM_CAP_DEFAULT, "bk_xiaozhi");
    if (xz == NULL) {
        return BK_ERR_NO_MEM;
    }
    xz->cfg = *cfg;
    xz->negotiated_frame_ms = 60U;

    if (xz->cfg.opus_ops == NULL) {
        bk_err_t oerr = init_native_opus(xz);
        if (oerr != BK_OK) {
            bk_port_free(xz);
            return oerr;
        }
    }

    *out_xz = xz;
    return BK_OK;
}

void bk_xiaozhi_destroy(bk_xiaozhi_handle_t xz)
{
    if (xz != NULL) {
        if (xz->enc != NULL && xz->fn_enc_destroy != NULL) {
            xz->fn_enc_destroy(xz->enc);
        }
        if (xz->dec != NULL && xz->fn_dec_destroy != NULL) {
            xz->fn_dec_destroy(xz->dec);
        }
#if !defined(BK_OF_BUILDING_TARGET_BK7259)
        if (xz->dl_handle != NULL) {
            dlclose(xz->dl_handle);
        }
#endif
        bk_port_free(xz);
    }
}

bool bk_xiaozhi_is_native_libopus_loaded(bk_xiaozhi_handle_t xz)
{
    return xz != NULL && xz->native_opus_ready;
}

bk_err_t bk_xiaozhi_send_hello(bk_xiaozhi_handle_t xz)
{
    if (xz == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    static const char hello_msg[] =
        "{\"type\":\"hello\",\"version\":1,\"transport\":\"websocket\","
        "\"features\":{\"mcp\":true},"
        "\"audio_params\":{\"format\":\"opus\",\"sample_rate\":16000,"
        "\"channels\":1,\"frame_duration\":60}}";
    return bk_ws_transport_send_text(xz->cfg.ws, hello_msg, sizeof(hello_msg) - 1U);
}

bk_err_t bk_xiaozhi_on_text_frame(bk_xiaozhi_handle_t xz,
                                  const char *json_text,
                                  size_t len)
{
    if (xz == NULL || json_text == NULL || len == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    const char *doc_start = json_text;
    const char *doc_end = json_text + len;

    const char *type_s = NULL;
    const char *type_e = NULL;
    char msg_type[32] = {0};
    if (!json_find_top_level_member(doc_start, doc_end, "type", &type_s, &type_e) ||
        !json_decode_string_slice(type_s, type_e, msg_type, sizeof(msg_type))) {
        return BK_ERR_INVALID_ARG;
    }

    if (strcmp(msg_type, "hello") == 0) {
        const char *sid_s = NULL;
        const char *sid_e = NULL;
        if (json_find_top_level_member(doc_start, doc_end, "session_id", &sid_s, &sid_e)) {
            if (!json_decode_string_slice(sid_s, sid_e, xz->session_id, sizeof(xz->session_id))) {
                return BK_ERR_INVALID_ARG;
            }
        }
        const char *ap_s = NULL;
        const char *ap_e = NULL;
        if (json_find_top_level_member(doc_start, doc_end, "audio_params", &ap_s, &ap_e)) {
            const char *fd_s = NULL;
            const char *fd_e = NULL;
            int32_t f_ms = 60;
            if (json_find_top_level_member(ap_s, ap_e, "frame_duration", &fd_s, &fd_e) &&
                json_decode_int_slice(fd_s, fd_e, &f_ms) && f_ms > 0) {
                xz->negotiated_frame_ms = (uint32_t)f_ms;
            }
        }
        xz->handshake_complete = true;
        return BK_OK;
    }

    if (strcmp(msg_type, "iot") == 0) {
        if (!xz->cfg.enable_legacy_iot_compat) {
            return BK_ERR_NOT_SUPPORTED;
        }
        return BK_OK;
    }

    if (strcmp(msg_type, "mcp") == 0) {
        const char *pay_s = NULL;
        const char *pay_e = NULL;
        if (!json_find_top_level_member(doc_start, doc_end, "payload", &pay_s, &pay_e) ||
            pay_s >= pay_e || *pay_s != '{') {
            return BK_ERR_INVALID_ARG;
        }

        /* 1. Verify "jsonrpc": "2.0" inside payload */
        const char *ver_s = NULL;
        const char *ver_e = NULL;
        char jsonrpc_ver[16] = {0};
        if (!json_find_top_level_member(pay_s, pay_e, "jsonrpc", &ver_s, &ver_e) ||
            !json_decode_string_slice(ver_s, ver_e, jsonrpc_ver, sizeof(jsonrpc_ver)) ||
            strcmp(jsonrpc_ver, "2.0") != 0) {
            return BK_ERR_INVALID_ARG;
        }

        /* 2. Extract "id" inside payload (supports integer OR string id) */
        const char *id_s = NULL;
        const char *id_e = NULL;
        if (!json_find_top_level_member(pay_s, pay_e, "id", &id_s, &id_e)) {
            return BK_ERR_INVALID_ARG;
        }
        bool id_is_string = false;
        int32_t rpc_id_int = 0;
        char rpc_id_str[64] = {0};
        if (*id_s == '"') {
            if (!json_decode_string_slice(id_s, id_e, rpc_id_str, sizeof(rpc_id_str))) {
                return BK_ERR_INVALID_ARG;
            }
            id_is_string = true;
        } else if (!json_decode_int_slice(id_s, id_e, &rpc_id_int)) {
            return BK_ERR_INVALID_ARG;
        }

        /* 3. Extract "method" strictly at top level of payload */
        const char *m_s = NULL;
        const char *m_e = NULL;
        char method[64] = {0};
        if (!json_find_top_level_member(pay_s, pay_e, "method", &m_s, &m_e) ||
            !json_decode_string_slice(m_s, m_e, method, sizeof(method)) ||
            method[0] == '\0') {
            return BK_ERR_INVALID_ARG;
        }

        /* 4. Extract "params" slice inside payload (defaults to "{}" if omitted) */
        const char *prm_s = NULL;
        const char *prm_e = NULL;
        char params_buf[384] = "{}";
        if (json_find_top_level_member(pay_s, pay_e, "params", &prm_s, &prm_e)) {
            size_t prm_len = (size_t)(prm_e - prm_s);
            if (prm_len == 0U || prm_len + 1U > sizeof(params_buf) ||
                (*prm_s != '{' && *prm_s != '[')) {
                return BK_ERR_INVALID_ARG;
            }
            memcpy(params_buf, prm_s, prm_len);
            params_buf[prm_len] = '\0';
        }

        char result_json[256] = "{\"ok\":true}";
        if (xz->cfg.mcp_cb != NULL) {
            bk_err_t cb_err = xz->cfg.mcp_cb(
                method, params_buf, result_json, sizeof(result_json), xz->cfg.mcp_user_ctx);
            if (cb_err != BK_OK) {
                return cb_err;
            }
        }

        char escaped_sid[BK_XIAOZHI_MAX_SESSION_ID_LEN * 2U];
        if (!json_escape_string(xz->session_id, escaped_sid, sizeof(escaped_sid))) {
            return BK_ERR_PAYLOAD_TOO_LARGE;
        }

        char reply[640];
        int n = -1;
        if (id_is_string) {
            char escaped_id[128];
            if (!json_escape_string(rpc_id_str, escaped_id, sizeof(escaped_id))) {
                return BK_ERR_PAYLOAD_TOO_LARGE;
            }
            n = snprintf(reply,
                         sizeof(reply),
                         "{\"session_id\":\"%s\",\"type\":\"mcp\",\"payload\":"
                         "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"result\":%s}}",
                         escaped_sid,
                         escaped_id,
                         result_json);
        } else {
            n = snprintf(reply,
                         sizeof(reply),
                         "{\"session_id\":\"%s\",\"type\":\"mcp\",\"payload\":"
                         "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":%s}}",
                         escaped_sid,
                         (int)rpc_id_int,
                         result_json);
        }
        if (n < 0 || (size_t)n >= sizeof(reply)) {
            return BK_ERR_PAYLOAD_TOO_LARGE;
        }
        return bk_ws_transport_send_text(xz->cfg.ws, reply, (size_t)n);
    }

    return BK_OK;
}

bk_err_t bk_xiaozhi_push_uplink_pcm_20ms(bk_xiaozhi_handle_t xz,
                                         const int16_t *pcm_320,
                                         bool *out_packet_sent)
{
    if (xz == NULL || pcm_320 == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (out_packet_sent != NULL) {
        *out_packet_sent = false;
    }
    if (!xz->handshake_complete) {
        return BK_ERR_INVALID_STATE;
    }

    memcpy(&xz->uplink_accum[xz->accum_20ms_count * BK_AUDIO_PCM_20MS_SAMPLES],
           pcm_320,
           BK_AUDIO_PCM_20MS_BYTES);
    xz->accum_20ms_count += 1U;

    if (xz->accum_20ms_count < 3U) {
        return BK_OK;
    }

    xz->accum_20ms_count = 0U;

    uint8_t opus_pkt[BK_XIAOZHI_MAX_OPUS_PACKET_BYTES];
    int encoded_bytes = -1;

    if (xz->cfg.opus_ops != NULL && xz->cfg.opus_ops->encode_60ms != NULL) {
        encoded_bytes = xz->cfg.opus_ops->encode_60ms(
            xz->uplink_accum, opus_pkt, sizeof(opus_pkt), xz->cfg.opus_ops->codec_ctx);
    } else if (xz->native_opus_ready && xz->enc != NULL && xz->fn_encode != NULL) {
        encoded_bytes = (int)xz->fn_encode(xz->enc,
                                           xz->uplink_accum,
                                           (int)BK_XIAOZHI_OPUS_60MS_PCM_SAMPLES,
                                           opus_pkt,
                                           (int32_t)sizeof(opus_pkt));
    } else {
        return BK_ERR_NOT_SUPPORTED;
    }

    if (encoded_bytes <= 0) {
        return BK_ERR_CORRUPTED;
    }

    bk_err_t err = bk_ws_transport_send_binary(xz->cfg.ws, opus_pkt, (size_t)encoded_bytes);
    if (err == BK_OK && out_packet_sent != NULL) {
        *out_packet_sent = true;
    }
    return err;
}

bk_err_t bk_xiaozhi_on_binary_opus_60ms(bk_xiaozhi_handle_t xz,
                                        const uint8_t *opus_pkt,
                                        size_t pkt_len)
{
    if (xz == NULL || opus_pkt == NULL || pkt_len == 0U ||
        pkt_len > BK_XIAOZHI_MAX_OPUS_PACKET_BYTES) {
        return BK_ERR_INVALID_ARG;
    }
    if (xz->cfg.audio_session == NULL) {
        return BK_ERR_INVALID_STATE;
    }

    int16_t decoded_pcm[BK_XIAOZHI_OPUS_60MS_PCM_SAMPLES];
    int decoded_samples = -1;

    if (xz->cfg.opus_ops != NULL && xz->cfg.opus_ops->decode_60ms != NULL) {
        decoded_samples = xz->cfg.opus_ops->decode_60ms(
            opus_pkt, pkt_len, decoded_pcm, xz->cfg.opus_ops->codec_ctx);
    } else if (xz->native_opus_ready && xz->dec != NULL && xz->fn_decode != NULL) {
        decoded_samples = (int)xz->fn_decode(xz->dec,
                                             opus_pkt,
                                             (int32_t)pkt_len,
                                             decoded_pcm,
                                             (int)BK_XIAOZHI_OPUS_60MS_PCM_SAMPLES,
                                             0);
    } else {
        return BK_ERR_NOT_SUPPORTED;
    }

    if (decoded_samples != (int)BK_XIAOZHI_OPUS_60MS_PCM_SAMPLES) {
        return BK_ERR_CORRUPTED;
    }

    /* Split decoded 960-sample (60ms) buffer into 3 x 20ms (320-sample) frames */
    for (size_t f = 0U; f < 3U; ++f) {
        bk_err_t err = bk_audio_session_write_downlink_20ms(
            xz->cfg.audio_session,
            &decoded_pcm[f * BK_AUDIO_PCM_20MS_SAMPLES],
            0U);
        if (err != BK_OK) {
            return err;
        }
    }
    return BK_OK;
}

const char *bk_xiaozhi_get_session_id(bk_xiaozhi_handle_t xz)
{
    return xz != NULL ? xz->session_id : "";
}

uint32_t bk_xiaozhi_get_negotiated_frame_ms(bk_xiaozhi_handle_t xz)
{
    return xz != NULL ? xz->negotiated_frame_ms : 0U;
}
