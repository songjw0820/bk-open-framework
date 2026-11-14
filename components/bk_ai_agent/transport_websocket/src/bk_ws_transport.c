/* SPDX-License-Identifier: Apache-2.0 */
#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L

#include "bk_ws_transport.h"
#include "bk_port_mem.h"
#include "bk_port_os.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(BK_OF_BUILDING_TARGET_BK7259)
#include <components/system.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define BK_WS_GUID               "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define BK_WS_MAX_RX_FRAME_BYTES 8192U

struct bk_ws_transport {
    bk_ws_transport_config_t cfg;
    bool is_tls;
    char host[128];
    uint16_t port;
    char path[256];
    int sock_fd;
    bk_ws_state_t state;
    uint32_t disconnect_count;

    /* RFC 6455 fragmented message reassembly state */
    uint8_t frag_opcode;
    uint8_t *frag_buf;
    size_t frag_len;

#if defined(BK_OF_BUILDING_TARGET_BK7259)
    bool mbedtls_active;
    mbedtls_net_context tls_net;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config tls_conf;
    mbedtls_x509_crt ca_cert;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
#endif
};

/* --- Minimal portable SHA-1 (RFC 3174) & Base64 (RFC 4648) for RFC 6455 --- */

typedef struct {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
} bk_sha1_ctx_t;

static uint32_t sha1_rol32(uint32_t val, unsigned int bits)
{
    return (val << bits) | (val >> (32U - bits));
}

static void sha1_transform(uint32_t state[5], const uint8_t block[64])
{
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               ((uint32_t)block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i) {
        w[i] = sha1_rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1U);
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];

    for (int i = 0; i < 80; ++i) {
        uint32_t f = 0;
        uint32_t k = 0;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999U;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1U;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCU;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6U;
        }
        uint32_t temp = sha1_rol32(a, 5U) + f + e + k + w[i];
        e = d;
        d = c;
        c = sha1_rol32(b, 30U);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

static void sha1_init(bk_sha1_ctx_t *ctx)
{
    ctx->state[0] = 0x67452301U;
    ctx->state[1] = 0xEFCDAB89U;
    ctx->state[2] = 0x98BADCFEU;
    ctx->state[3] = 0x10325476U;
    ctx->state[4] = 0xC3D2E1F0U;
    ctx->count = 0U;
}

static void sha1_update(bk_sha1_ctx_t *ctx, const uint8_t *data, size_t len)
{
    size_t idx = (size_t)((ctx->count >> 3) & 0x3FU);
    ctx->count += (uint64_t)len << 3;
    for (size_t i = 0; i < len; ++i) {
        ctx->buffer[idx++] = data[i];
        if (idx == 64U) {
            sha1_transform(ctx->state, ctx->buffer);
            idx = 0U;
        }
    }
}

static void sha1_final(bk_sha1_ctx_t *ctx, uint8_t digest[20])
{
    uint8_t bits[8];
    for (int i = 0; i < 8; ++i) {
        bits[i] = (uint8_t)((ctx->count >> ((7 - i) * 8)) & 0xFFU);
    }
    uint8_t pad = 0x80U;
    sha1_update(ctx, &pad, 1U);
    uint8_t zero = 0x00U;
    while (((ctx->count >> 3) & 0x3FU) != 56U) {
        sha1_update(ctx, &zero, 1U);
    }
    sha1_update(ctx, bits, 8U);
    for (int i = 0; i < 5; ++i) {
        digest[i * 4]     = (uint8_t)((ctx->state[i] >> 24) & 0xFFU);
        digest[i * 4 + 1] = (uint8_t)((ctx->state[i] >> 16) & 0xFFU);
        digest[i * 4 + 2] = (uint8_t)((ctx->state[i] >> 8) & 0xFFU);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i] & 0xFFU);
    }
}

static bk_err_t base64_encode(const uint8_t *in, size_t in_len, char *out, size_t out_cap)
{
    static const char b64_tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t needed = ((in_len + 2U) / 3U) * 4U + 1U;
    if (out == NULL || out_cap < needed) {
        return BK_ERR_PAYLOAD_TOO_LARGE;
    }
    size_t j = 0U;
    for (size_t i = 0U; i < in_len; i += 3U) {
        uint32_t a = in[i];
        uint32_t b = (i + 1U < in_len) ? in[i + 1U] : 0U;
        uint32_t c = (i + 2U < in_len) ? in[i + 2U] : 0U;
        uint32_t triple = (a << 16) | (b << 8) | c;

        out[j++] = b64_tbl[(triple >> 18) & 0x3FU];
        out[j++] = b64_tbl[(triple >> 12) & 0x3FU];
        out[j++] = (i + 1U < in_len) ? b64_tbl[(triple >> 6) & 0x3FU] : '=';
        out[j++] = (i + 2U < in_len) ? b64_tbl[triple & 0x3FU] : '=';
    }
    out[j] = '\0';
    return BK_OK;
}

bk_err_t bk_ws_compute_accept_key(const char *sec_ws_key,
                                  char *out_accept,
                                  size_t out_cap)
{
    if (sec_ws_key == NULL || out_accept == NULL || sec_ws_key[0] == '\0') {
        return BK_ERR_INVALID_ARG;
    }
    bk_sha1_ctx_t ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, (const uint8_t *)sec_ws_key, strlen(sec_ws_key));
    sha1_update(&ctx, (const uint8_t *)BK_WS_GUID, sizeof(BK_WS_GUID) - 1U);
    uint8_t digest[20];
    sha1_final(&ctx, digest);
    return base64_encode(digest, sizeof(digest), out_accept, out_cap);
}

static bool has_crlf(const char *s)
{
    if (s == NULL) {
        return false;
    }
    for (; *s != '\0'; ++s) {
        if (*s == '\r' || *s == '\n') {
            return true;
        }
    }
    return false;
}

static bk_err_t parse_ws_uri(const char *uri,
                             bool *out_is_tls,
                             char *out_host,
                             size_t host_cap,
                             uint16_t *out_port,
                             char *out_path,
                             size_t path_cap)
{
    if (uri == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    const char *p = NULL;
    if (strncmp(uri, "wss://", 6) == 0) {
        *out_is_tls = true;
        *out_port = 443U;
        p = uri + 6;
    } else if (strncmp(uri, "ws://", 5) == 0) {
        *out_is_tls = false;
        *out_port = 80U;
        p = uri + 5;
    } else {
        return BK_ERR_INVALID_ARG;
    }

    const char *slash = strchr(p, '/');
    const char *host_end = (slash != NULL) ? slash : (p + strlen(p));
    const char *colon = NULL;
    for (const char *c = p; c < host_end; ++c) {
        if (*c == ':') {
            colon = c;
            break;
        }
    }

    size_t host_len = (colon != NULL) ? (size_t)(colon - p) : (size_t)(host_end - p);
    if (host_len == 0U || host_len + 1U > host_cap) {
        return BK_ERR_INVALID_ARG;
    }
    memcpy(out_host, p, host_len);
    out_host[host_len] = '\0';

    if (colon != NULL) {
        char port_buf[16];
        size_t port_len = (size_t)(host_end - (colon + 1));
        if (port_len == 0U || port_len >= sizeof(port_buf)) {
            return BK_ERR_INVALID_ARG;
        }
        memcpy(port_buf, colon + 1, port_len);
        port_buf[port_len] = '\0';
        char *endptr = NULL;
        long v = strtol(port_buf, &endptr, 10);
        if (endptr == port_buf || *endptr != '\0' || v <= 0 || v > 65535) {
            return BK_ERR_INVALID_ARG;
        }
        *out_port = (uint16_t)v;
    }

    if (slash != NULL) {
        size_t path_len = strlen(slash);
        if (path_len + 1U > path_cap) {
            return BK_ERR_INVALID_ARG;
        }
        memcpy(out_path, slash, path_len + 1U);
    } else {
        if (path_cap < 2U) {
            return BK_ERR_INVALID_ARG;
        }
        out_path[0] = '/';
        out_path[1] = '\0';
    }
    return BK_OK;
}

static void reset_frag_buffer(struct bk_ws_transport *ws)
{
    if (ws->frag_buf != NULL) {
        bk_port_free(ws->frag_buf);
        ws->frag_buf = NULL;
    }
    ws->frag_len = 0U;
    ws->frag_opcode = 0U;
}

#if defined(BK_OF_BUILDING_TARGET_BK7259)
static int bk7259_entropy_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
    (void)data;
    if (bk_port_get_random(output, len) != BK_OK) {
        return -1;
    }
    *olen = len;
    return 0;
}

static void close_bk7259_mbedtls(struct bk_ws_transport *ws)
{
    if (ws->mbedtls_active) {
        (void)mbedtls_ssl_close_notify(&ws->ssl);
        mbedtls_x509_crt_free(&ws->ca_cert);
        mbedtls_ssl_free(&ws->ssl);
        mbedtls_ssl_config_free(&ws->tls_conf);
        mbedtls_ctr_drbg_free(&ws->ctr_drbg);
        mbedtls_entropy_free(&ws->entropy);
        ws->mbedtls_active = false;
    }
}

static bk_err_t setup_bk7259_mbedtls(struct bk_ws_transport *ws)
{
    if (ws->cfg.verify_cert_chain) {
        if (ws->cfg.ca_cert_pem == NULL || ws->cfg.ca_cert_len == 0U) {
            return BK_ERR_NET_TLS_VERIFY_FAILED;
        }
    }

    mbedtls_ssl_init(&ws->ssl);
    mbedtls_ssl_config_init(&ws->tls_conf);
    mbedtls_x509_crt_init(&ws->ca_cert);
    mbedtls_ctr_drbg_init(&ws->ctr_drbg);
    mbedtls_entropy_init(&ws->entropy);
    ws->mbedtls_active = true;

    if (mbedtls_entropy_add_source(&ws->entropy,
                                   bk7259_entropy_poll,
                                   NULL,
                                   MBEDTLS_ENTROPY_MAX_GATHER,
                                   MBEDTLS_ENTROPY_SOURCE_STRONG) != 0) {
        close_bk7259_mbedtls(ws);
        return BK_ERR_NET_TLS_VERIFY_FAILED;
    }

    const char *pers = "bk_of_wss";
    if (mbedtls_ctr_drbg_seed(&ws->ctr_drbg,
                              mbedtls_entropy_func,
                              &ws->entropy,
                              (const unsigned char *)pers,
                              strlen(pers)) != 0) {
        close_bk7259_mbedtls(ws);
        return BK_ERR_NET_TLS_VERIFY_FAILED;
    }

    if (mbedtls_ssl_config_defaults(&ws->tls_conf,
                                    MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM,
                                    MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
        close_bk7259_mbedtls(ws);
        return BK_ERR_NET_TLS_VERIFY_FAILED;
    }

    if (ws->cfg.verify_cert_chain) {
        if (mbedtls_x509_crt_parse(&ws->ca_cert,
                                   ws->cfg.ca_cert_pem,
                                   ws->cfg.ca_cert_len) < 0) {
            close_bk7259_mbedtls(ws);
            return BK_ERR_NET_TLS_VERIFY_FAILED;
        }
        mbedtls_ssl_conf_ca_chain(&ws->tls_conf, &ws->ca_cert, NULL);
        mbedtls_ssl_conf_authmode(&ws->tls_conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    } else {
        close_bk7259_mbedtls(ws);
        return BK_ERR_NET_TLS_VERIFY_FAILED;
    }

    mbedtls_ssl_conf_rng(&ws->tls_conf, mbedtls_ctr_drbg_random, &ws->ctr_drbg);

    if (mbedtls_ssl_setup(&ws->ssl, &ws->tls_conf) != 0) {
        close_bk7259_mbedtls(ws);
        return BK_ERR_NET_TLS_VERIFY_FAILED;
    }

    if (ws->cfg.verify_sni) {
        const char *sni = (ws->cfg.sni_hostname != NULL && ws->cfg.sni_hostname[0] != '\0')
                              ? ws->cfg.sni_hostname
                              : ws->host;
        if (mbedtls_ssl_set_hostname(&ws->ssl, sni) != 0) {
            close_bk7259_mbedtls(ws);
            return BK_ERR_NET_TLS_VERIFY_FAILED;
        }
    } else {
        close_bk7259_mbedtls(ws);
        return BK_ERR_NET_TLS_VERIFY_FAILED;
    }

    ws->tls_net.fd = ws->sock_fd;
    mbedtls_ssl_set_bio(&ws->ssl, &ws->tls_net, mbedtls_net_send, mbedtls_net_recv, NULL);

    uint64_t deadline = bk_port_get_time_ms() +
                        (uint64_t)((ws->cfg.connect_timeout_ms > 0U) ? ws->cfg.connect_timeout_ms : 5000U);
    for (;;) {
        int hs = mbedtls_ssl_handshake(&ws->ssl);
        if (hs == 0) {
            break;
        }
        if (hs != MBEDTLS_ERR_SSL_WANT_READ && hs != MBEDTLS_ERR_SSL_WANT_WRITE) {
            close_bk7259_mbedtls(ws);
            return BK_ERR_NET_TLS_VERIFY_FAILED;
        }
        if (bk_port_get_time_ms() >= deadline) {
            close_bk7259_mbedtls(ws);
            return BK_ERR_TIMEOUT;
        }
        bk_port_sleep_ms(1U);
    }

    if (mbedtls_ssl_get_verify_result(&ws->ssl) != 0U) {
        close_bk7259_mbedtls(ws);
        return BK_ERR_NET_TLS_VERIFY_FAILED;
    }
    return BK_OK;
}
#endif

/* --- Low-level stream I/O (POSIX/lwIP socket, BK7259 mbedTLS, or custom io_ops) --- */

static void close_stream(struct bk_ws_transport *ws)
{
    reset_frag_buffer(ws);
    if (ws->cfg.io_ops != NULL && ws->cfg.io_ops->close != NULL) {
        ws->cfg.io_ops->close(ws->cfg.io_ops->io_ctx);
    }
#if defined(BK_OF_BUILDING_TARGET_BK7259)
    close_bk7259_mbedtls(ws);
#endif
    if (ws->sock_fd >= 0) {
        close(ws->sock_fd);
        ws->sock_fd = -1;
    }
}

static bk_err_t connect_socket_with_timeout(const char *host,
                                            uint16_t port,
                                            uint32_t timeout_ms,
                                            int *out_fd)
{
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", (unsigned int)port);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *res = NULL;
    if (getaddrinfo(host, port_str, &hints, &res) != 0 || res == NULL) {
        return BK_ERR_IO;
    }

    int connected_fd = -1;
    bk_err_t last_err = BK_ERR_IO;
    for (struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next) {
        int fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) {
            continue;
        }
#ifdef SO_NOSIGPIPE
        int set_nosigpipe = 1;
        (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &set_nosigpipe, sizeof(set_nosigpipe));
#endif
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags >= 0) {
            (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        int rc = connect(fd, rp->ai_addr, rp->ai_addrlen);
        if (rc < 0) {
            if (errno == EINPROGRESS || errno == EALREADY || errno == EWOULDBLOCK) {
                fd_set wfds;
                FD_ZERO(&wfds);
                FD_SET(fd, &wfds);
                struct timeval tv;
                tv.tv_sec = (time_t)(timeout_ms / 1000U);
                tv.tv_usec = (suseconds_t)((timeout_ms % 1000U) * 1000U);
                int sel = select(fd + 1, NULL, &wfds, NULL, &tv);
                if (sel == 0) {
                    last_err = BK_ERR_TIMEOUT;
                    close(fd);
                    continue;
                }
                if (sel < 0) {
                    close(fd);
                    continue;
                }
                int so_err = 0;
                socklen_t so_len = (socklen_t)sizeof(so_err);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_err, &so_len) < 0 || so_err != 0) {
                    close(fd);
                    continue;
                }
            } else {
                close(fd);
                continue;
            }
        }

        if (flags >= 0) {
            (void)fcntl(fd, F_SETFL, flags);
        }
        connected_fd = fd;
        break;
    }

    freeaddrinfo(res);
    if (connected_fd < 0) {
        return last_err;
    }
    *out_fd = connected_fd;
    return BK_OK;
}

static bk_err_t stream_write_all(struct bk_ws_transport *ws, const uint8_t *buf, size_t len)
{
    size_t sent = 0U;
    while (sent < len) {
        int n = -1;
        if (ws->cfg.io_ops != NULL && ws->cfg.io_ops->send != NULL) {
            n = ws->cfg.io_ops->send(buf + sent, len - sent, ws->cfg.io_ops->io_ctx);
        }
#if defined(BK_OF_BUILDING_TARGET_BK7259)
        else if (ws->mbedtls_active) {
            n = mbedtls_ssl_write(&ws->ssl, buf + sent, len - sent);
            if (n == MBEDTLS_ERR_SSL_WANT_WRITE || n == MBEDTLS_ERR_SSL_WANT_READ) {
                continue;
            }
        }
#endif
        else {
            if (ws->sock_fd < 0) {
                return BK_ERR_INVALID_STATE;
            }
#ifdef MSG_NOSIGNAL
            n = (int)send(ws->sock_fd, buf + sent, len - sent, MSG_NOSIGNAL);
#else
            n = (int)send(ws->sock_fd, buf + sent, len - sent, 0);
#endif
        }
        if (n <= 0) {
            return BK_ERR_IO;
        }
        sent += (size_t)n;
    }
    return BK_OK;
}

static int stream_read_some(struct bk_ws_transport *ws,
                            uint8_t *buf,
                            size_t len,
                            uint32_t timeout_ms)
{
    if (ws->cfg.io_ops != NULL && ws->cfg.io_ops->recv != NULL) {
        return ws->cfg.io_ops->recv(buf, len, timeout_ms, ws->cfg.io_ops->io_ctx);
    }
    if (ws->sock_fd < 0) {
        return -1;
    }
#if defined(BK_OF_BUILDING_TARGET_BK7259)
    if (ws->mbedtls_active && mbedtls_ssl_get_bytes_avail(&ws->ssl) > 0U) {
        int ret = mbedtls_ssl_read(&ws->ssl, buf, len);
        return (ret > 0) ? ret : -1;
    }
#endif
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(ws->sock_fd, &rfds);
    struct timeval tv;
    tv.tv_sec = (time_t)(timeout_ms / 1000U);
    tv.tv_usec = (suseconds_t)((timeout_ms % 1000U) * 1000U);
    int r = select(ws->sock_fd + 1, &rfds, NULL, NULL, &tv);
    if (r == 0) {
        return -2; /* timeout */
    }
    if (r < 0) {
        return -1;
    }
#if defined(BK_OF_BUILDING_TARGET_BK7259)
    if (ws->mbedtls_active) {
        int ret = mbedtls_ssl_read(&ws->ssl, buf, len);
        return (ret > 0) ? ret : -1;
    }
#endif
    ssize_t n = recv(ws->sock_fd, buf, len, 0);
    return (int)n;
}

static bk_err_t stream_read_exact(struct bk_ws_transport *ws,
                                  uint8_t *buf,
                                  size_t len,
                                  uint32_t timeout_ms)
{
    size_t got = 0U;
    while (got < len) {
        int n = stream_read_some(ws, buf + got, len - got, timeout_ms);
        if (n == -2) {
            return (got == 0U) ? BK_ERR_TIMEOUT : BK_ERR_IO;
        }
        if (n <= 0) {
            return BK_ERR_IO;
        }
        got += (size_t)n;
    }
    return BK_OK;
}

bk_err_t bk_ws_transport_create(const bk_ws_transport_config_t *cfg,
                                bk_ws_transport_handle_t *out_ws)
{
    if (cfg == NULL || out_ws == NULL || cfg->uri == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (has_crlf(cfg->auth_token) || has_crlf(cfg->device_id) || has_crlf(cfg->client_id)) {
        return BK_ERR_INVALID_ARG;
    }
    if (cfg->require_xiaozhi_headers) {
        if (cfg->auth_token == NULL || cfg->auth_token[0] == '\0' ||
            cfg->device_id == NULL || cfg->device_id[0] == '\0' ||
            cfg->client_id == NULL || cfg->client_id[0] == '\0') {
            return BK_ERR_INVALID_ARG;
        }
    }

    struct bk_ws_transport *ws = (struct bk_ws_transport *)bk_port_calloc(
        1, sizeof(*ws), BK_MEM_CAP_DEFAULT, "bk_ws");
    if (ws == NULL) {
        return BK_ERR_NO_MEM;
    }

    bk_err_t err = parse_ws_uri(cfg->uri,
                                &ws->is_tls,
                                ws->host,
                                sizeof(ws->host),
                                &ws->port,
                                ws->path,
                                sizeof(ws->path));
    if (err != BK_OK) {
        bk_port_free(ws);
        return err;
    }

    ws->cfg = *cfg;
    ws->sock_fd = -1;
    ws->state = BK_WS_STATE_DISCONNECTED;
    *out_ws = ws;
    return BK_OK;
}

void bk_ws_transport_destroy(bk_ws_transport_handle_t ws)
{
    if (ws != NULL) {
        close_stream(ws);
        bk_port_free(ws);
    }
}

bk_err_t bk_ws_transport_build_upgrade_request(bk_ws_transport_handle_t ws,
                                               const char *sec_ws_key,
                                               char *out_req,
                                               size_t out_cap,
                                               size_t *out_len)
{
    if (ws == NULL || sec_ws_key == NULL || out_req == NULL || out_cap == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    uint32_t proto_ver = (ws->cfg.protocol_version > 0U) ? ws->cfg.protocol_version : 1U;
    const char *token = (ws->cfg.auth_token != NULL && ws->cfg.auth_token[0] != '\0')
                            ? ws->cfg.auth_token
                            : "anonymous";
    const char *dev_id = (ws->cfg.device_id != NULL && ws->cfg.device_id[0] != '\0')
                             ? ws->cfg.device_id
                             : "00:00:00:00:00:00";
    const char *cli_id = (ws->cfg.client_id != NULL && ws->cfg.client_id[0] != '\0')
                             ? ws->cfg.client_id
                             : "00000000-0000-4000-8000-000000000000";

    int n = snprintf(out_req,
                     out_cap,
                     "GET %s HTTP/1.1\r\n"
                     "Host: %s:%u\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "Sec-WebSocket-Key: %s\r\n"
                     "Sec-WebSocket-Version: 13\r\n"
                     "Authorization: Bearer %s\r\n"
                     "Protocol-Version: %u\r\n"
                     "Device-Id: %s\r\n"
                     "Client-Id: %s\r\n"
                     "\r\n",
                     ws->path,
                     ws->host,
                     (unsigned int)ws->port,
                     sec_ws_key,
                     token,
                     (unsigned int)proto_ver,
                     dev_id,
                     cli_id);
    if (n < 0 || (size_t)n >= out_cap) {
        return BK_ERR_PAYLOAD_TOO_LARGE;
    }
    if (out_len != NULL) {
        *out_len = (size_t)n;
    }
    return BK_OK;
}

static bool find_header_value_ci(const char *headers,
                                 const char *header_name,
                                 char *out_val,
                                 size_t out_cap)
{
    size_t name_len = strlen(header_name);
    const char *line = headers;
    while (*line != '\0') {
        const char *line_end = strstr(line, "\r\n");
        if (line_end == NULL) {
            break;
        }
        size_t line_len = (size_t)(line_end - line);
        if (line_len > name_len && line[name_len] == ':') {
            bool match = true;
            for (size_t i = 0; i < name_len; ++i) {
                if (tolower((unsigned char)line[i]) != tolower((unsigned char)header_name[i])) {
                    match = false;
                    break;
                }
            }
            if (match) {
                const char *v = line + name_len + 1;
                while (v < line_end && (*v == ' ' || *v == '\t')) {
                    ++v;
                }
                size_t v_len = (size_t)(line_end - v);
                while (v_len > 0U && (v[v_len - 1U] == ' ' || v[v_len - 1U] == '\t')) {
                    --v_len;
                }
                if (v_len + 1U > out_cap) {
                    return false;
                }
                memcpy(out_val, v, v_len);
                out_val[v_len] = '\0';
                return true;
            }
        }
        line = line_end + 2;
    }
    return false;
}

static bool header_value_equals_ci(const char *value, const char *expected)
{
    if (value == NULL || expected == NULL) {
        return false;
    }
    while (*value != '\0' && *expected != '\0') {
        if (tolower((unsigned char)*value) != tolower((unsigned char)*expected)) {
            return false;
        }
        ++value;
        ++expected;
    }
    return *value == '\0' && *expected == '\0';
}

bk_err_t bk_ws_transport_connect(bk_ws_transport_handle_t ws)
{
    if (ws == NULL) {
        return BK_ERR_INVALID_ARG;
    }

    if (ws->is_tls) {
        if (ws->cfg.net != NULL) {
            bk_err_t tls_err = bk_net_require_tls_ready(
                ws->cfg.net, ws->cfg.verify_cert_chain, ws->cfg.verify_sni);
            if (tls_err != BK_OK) {
                return tls_err;
            }
        } else if (!ws->cfg.verify_cert_chain || !ws->cfg.verify_sni) {
            return BK_ERR_NET_TLS_VERIFY_FAILED;
        }
#if !defined(BK_OF_BUILDING_TARGET_BK7259)
        if (ws->cfg.io_ops == NULL) {
            return BK_ERR_NOT_SUPPORTED;
        }
#endif
    } else {
        if (!ws->cfg.allow_plain_ws) {
            return BK_ERR_NET_TLS_VERIFY_FAILED;
        }
    }

    close_stream(ws);
    ws->state = BK_WS_STATE_CONNECTING;
    uint32_t timeout_ms = (ws->cfg.connect_timeout_ms > 0U) ? ws->cfg.connect_timeout_ms : 5000U;

    if (ws->cfg.io_ops != NULL) {
        if (ws->cfg.io_ops->connect != NULL) {
            bk_err_t cerr = ws->cfg.io_ops->connect(
                ws->host, ws->port, timeout_ms, ws->cfg.io_ops->io_ctx);
            if (cerr != BK_OK) {
                ws->state = BK_WS_STATE_DISCONNECTED;
                return cerr;
            }
        }
    } else {
        bk_err_t serr = connect_socket_with_timeout(ws->host, ws->port, timeout_ms, &ws->sock_fd);
        if (serr != BK_OK) {
            ws->state = BK_WS_STATE_DISCONNECTED;
            return serr;
        }
#if defined(BK_OF_BUILDING_TARGET_BK7259)
        if (ws->is_tls) {
            bk_err_t terr = setup_bk7259_mbedtls(ws);
            if (terr != BK_OK) {
                close_stream(ws);
                ws->state = BK_WS_STATE_DISCONNECTED;
                return terr;
            }
        }
#endif
    }

    /* Generate 16-byte CSPRNG nonce and Base64 Sec-WebSocket-Key */
    uint8_t nonce[16];
    bk_err_t rerr_rng = bk_port_get_random(nonce, sizeof(nonce));
    if (rerr_rng != BK_OK) {
        close_stream(ws);
        ws->state = BK_WS_STATE_DISCONNECTED;
        return rerr_rng;
    }
    char sec_ws_key[32];
    (void)base64_encode(nonce, sizeof(nonce), sec_ws_key, sizeof(sec_ws_key));

    char expected_accept[40];
    bk_err_t aerr = bk_ws_compute_accept_key(sec_ws_key, expected_accept, sizeof(expected_accept));
    if (aerr != BK_OK) {
        close_stream(ws);
        ws->state = BK_WS_STATE_DISCONNECTED;
        return aerr;
    }

    char req[1024];
    size_t req_len = 0U;
    bk_err_t berr = bk_ws_transport_build_upgrade_request(ws, sec_ws_key, req, sizeof(req), &req_len);
    if (berr != BK_OK) {
        close_stream(ws);
        ws->state = BK_WS_STATE_DISCONNECTED;
        return berr;
    }

    bk_err_t werr = stream_write_all(ws, (const uint8_t *)req, req_len);
    if (werr != BK_OK) {
        close_stream(ws);
        ws->state = BK_WS_STATE_DISCONNECTED;
        return werr;
    }

    /* Read HTTP response headers up to \r\n\r\n */
    char resp[1024];
    size_t resp_len = 0U;
    bool found_eoh = false;
    while (resp_len + 1U < sizeof(resp)) {
        uint8_t ch = 0;
        bk_err_t rerr = stream_read_exact(ws, &ch, 1U, timeout_ms);
        if (rerr != BK_OK) {
            close_stream(ws);
            ws->state = BK_WS_STATE_DISCONNECTED;
            return rerr;
        }
        resp[resp_len++] = (char)ch;
        resp[resp_len] = '\0';
        if (resp_len >= 4U && memcmp(&resp[resp_len - 4U], "\r\n\r\n", 4U) == 0) {
            found_eoh = true;
            break;
        }
    }
    if (!found_eoh) {
        close_stream(ws);
        ws->state = BK_WS_STATE_DISCONNECTED;
        return BK_ERR_CORRUPTED;
    }

    if (strncmp(resp, "HTTP/1.1 101", 12) != 0 && strncmp(resp, "HTTP/1.0 101", 12) != 0) {
        close_stream(ws);
        ws->state = BK_WS_STATE_DISCONNECTED;
        return BK_ERR_CORRUPTED;
    }

    char got_upgrade[32] = {0};
    char got_connection[32] = {0};
    char got_accept[64] = {0};
    if (!find_header_value_ci(resp, "Upgrade", got_upgrade, sizeof(got_upgrade)) ||
        !header_value_equals_ci(got_upgrade, "websocket") ||
        !find_header_value_ci(resp, "Connection", got_connection, sizeof(got_connection)) ||
        !header_value_equals_ci(got_connection, "Upgrade") ||
        !find_header_value_ci(resp, "Sec-WebSocket-Accept", got_accept, sizeof(got_accept)) ||
        strcmp(got_accept, expected_accept) != 0) {
        close_stream(ws);
        ws->state = BK_WS_STATE_DISCONNECTED;
        return BK_ERR_CORRUPTED;
    }

    ws->state = BK_WS_STATE_CONNECTED;
    return BK_OK;
}

static bk_err_t send_masked_frame(struct bk_ws_transport *ws,
                                  bk_ws_frame_type_t type,
                                  const uint8_t *payload,
                                  size_t len)
{
    if (ws == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (ws->state != BK_WS_STATE_CONNECTED) {
        return BK_ERR_INVALID_STATE;
    }
    if (len > 65535U) {
        return BK_ERR_PAYLOAD_TOO_LARGE;
    }

    uint8_t hdr[14];
    size_t hdr_len = 0U;
    hdr[hdr_len++] = (uint8_t)(0x80U | ((uint8_t)type & 0x0FU)); /* FIN=1, opcode */

    if (len <= 125U) {
        hdr[hdr_len++] = (uint8_t)(0x80U | (uint8_t)len); /* MASK=1 */
    } else {
        hdr[hdr_len++] = (uint8_t)(0x80U | 126U); /* MASK=1, 16-bit extended len */
        hdr[hdr_len++] = (uint8_t)((len >> 8) & 0xFFU);
        hdr[hdr_len++] = (uint8_t)(len & 0xFFU);
    }

    uint8_t mask[4];
    bk_err_t rerr = bk_port_get_random(mask, sizeof(mask));
    if (rerr != BK_OK) {
        return rerr;
    }
    for (int i = 0; i < 4; ++i) {
        hdr[hdr_len++] = mask[i];
    }

    bk_err_t err = stream_write_all(ws, hdr, hdr_len);
    if (err != BK_OK) {
        return err;
    }

    if (len > 0U && payload != NULL) {
        uint8_t *masked = (uint8_t *)bk_port_malloc(len, BK_MEM_CAP_DEFAULT, "bk_ws_tx");
        if (masked == NULL) {
            return BK_ERR_NO_MEM;
        }
        for (size_t i = 0U; i < len; ++i) {
            masked[i] = payload[i] ^ mask[i & 3U];
        }
        err = stream_write_all(ws, masked, len);
        bk_port_free(masked);
        if (err != BK_OK) {
            return err;
        }
    }

    if (ws->cfg.tx_hook != NULL) {
        (void)ws->cfg.tx_hook(type, payload, len, ws->cfg.tx_hook_ctx);
    }
    return BK_OK;
}

bk_err_t bk_ws_transport_disconnect(bk_ws_transport_handle_t ws)
{
    if (ws == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (ws->state == BK_WS_STATE_CONNECTED) {
        uint8_t close_code[2] = {0x03U, 0xE8U}; /* 1000 Normal Closure */
        (void)send_masked_frame(ws, BK_WS_FRAME_CLOSE, close_code, sizeof(close_code));
    }
    close_stream(ws);
    ws->state = BK_WS_STATE_DISCONNECTED;
    return BK_OK;
}

bk_err_t bk_ws_transport_reconnect(bk_ws_transport_handle_t ws)
{
    if (ws == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    ws->state = BK_WS_STATE_RECONNECTING;
    close_stream(ws);
    return bk_ws_transport_connect(ws);
}

bool bk_ws_transport_is_connected(bk_ws_transport_handle_t ws)
{
    return ws != NULL && ws->state == BK_WS_STATE_CONNECTED;
}

bk_ws_state_t bk_ws_transport_get_state(bk_ws_transport_handle_t ws)
{
    return ws != NULL ? ws->state : BK_WS_STATE_DISCONNECTED;
}

uint32_t bk_ws_transport_get_disconnect_count(bk_ws_transport_handle_t ws)
{
    return ws != NULL ? ws->disconnect_count : 0U;
}

bk_err_t bk_ws_transport_send_text(bk_ws_transport_handle_t ws,
                                   const char *text,
                                   size_t len)
{
    if (ws == NULL || text == NULL || len == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    return send_masked_frame(ws, BK_WS_FRAME_TEXT, (const uint8_t *)text, len);
}

bk_err_t bk_ws_transport_send_binary(bk_ws_transport_handle_t ws,
                                     const uint8_t *data,
                                     size_t len)
{
    if (ws == NULL || data == NULL || len == 0U) {
        return BK_ERR_INVALID_ARG;
    }
    return send_masked_frame(ws, BK_WS_FRAME_BINARY, data, len);
}

bk_err_t bk_ws_transport_send_ping(bk_ws_transport_handle_t ws,
                                   const uint8_t *payload,
                                   size_t len)
{
    if (ws == NULL || len > 125U || (len > 0U && payload == NULL)) {
        return BK_ERR_INVALID_ARG;
    }
    return send_masked_frame(ws, BK_WS_FRAME_PING, payload, len);
}

bk_err_t bk_ws_transport_send_pong(bk_ws_transport_handle_t ws,
                                   const uint8_t *payload,
                                   size_t len)
{
    if (ws == NULL || len > 125U || (len > 0U && payload == NULL)) {
        return BK_ERR_INVALID_ARG;
    }
    return send_masked_frame(ws, BK_WS_FRAME_PONG, payload, len);
}

static void notify_remote_disconnect(struct bk_ws_transport *ws, int reason_code)
{
    close_stream(ws);
    ws->state = BK_WS_STATE_DISCONNECTED;
    ws->disconnect_count += 1U;
    if (ws->cfg.disconnect_cb != NULL) {
        ws->cfg.disconnect_cb(ws, reason_code, ws->cfg.disconnect_cb_ctx);
    }
}

static bool is_valid_opcode(uint8_t opcode)
{
    return opcode == (uint8_t)BK_WS_FRAME_CONTINUATION ||
           opcode == (uint8_t)BK_WS_FRAME_TEXT ||
           opcode == (uint8_t)BK_WS_FRAME_BINARY ||
           opcode == (uint8_t)BK_WS_FRAME_CLOSE ||
           opcode == (uint8_t)BK_WS_FRAME_PING ||
           opcode == (uint8_t)BK_WS_FRAME_PONG;
}

bk_err_t bk_ws_transport_poll(bk_ws_transport_handle_t ws, uint32_t timeout_ms)
{
    if (ws == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    if (ws->state != BK_WS_STATE_CONNECTED) {
        return BK_ERR_INVALID_STATE;
    }

    uint8_t hdr2[2];
    bk_err_t err = stream_read_exact(ws, hdr2, 2U, timeout_ms);
    if (err == BK_ERR_TIMEOUT) {
        return BK_ERR_TIMEOUT;
    }
    if (err != BK_OK) {
        notify_remote_disconnect(ws, 1006);
        return err;
    }

    bool fin = (hdr2[0] & 0x80U) != 0U;
    uint8_t rsv = (uint8_t)(hdr2[0] & 0x70U);
    uint8_t opcode = (uint8_t)(hdr2[0] & 0x0FU);
    bool masked = (hdr2[1] & 0x80U) != 0U;
    uint64_t payload_len = (uint64_t)(hdr2[1] & 0x7FU);

    /*
     * RFC 6455 Section 5.1 / 5.2 / 5.5 validation:
     * - RSV1/RSV2/RSV3 MUST be 0 when no extension was negotiated
     * - Server-to-client frames MUST NOT be masked
     * - Opcode MUST be a defined RFC 6455 opcode
     * - Control frames (opcode & 0x08) MUST have FIN == 1 and payload_len <= 125
     * - CLOSE frame (0x8) MUST NOT have a 1-byte payload
     */
    bool is_control = (opcode & 0x08U) != 0U;
    if (rsv != 0U || masked || !is_valid_opcode(opcode)) {
        notify_remote_disconnect(ws, 1002);
        return BK_ERR_CORRUPTED;
    }
    if (is_control && (!fin || payload_len > 125U)) {
        notify_remote_disconnect(ws, 1002);
        return BK_ERR_CORRUPTED;
    }
    if (opcode == (uint8_t)BK_WS_FRAME_CLOSE && payload_len == 1U) {
        notify_remote_disconnect(ws, 1002);
        return BK_ERR_CORRUPTED;
    }

    if (payload_len == 126U) {
        uint8_t ext[2];
        if (stream_read_exact(ws, ext, 2U, timeout_ms) != BK_OK) {
            notify_remote_disconnect(ws, 1006);
            return BK_ERR_IO;
        }
        payload_len = ((uint64_t)ext[0] << 8) | (uint64_t)ext[1];
    } else if (payload_len == 127U) {
        uint8_t ext[8];
        if (stream_read_exact(ws, ext, 8U, timeout_ms) != BK_OK) {
            notify_remote_disconnect(ws, 1006);
            return BK_ERR_IO;
        }
        payload_len = 0U;
        for (int i = 0; i < 8; ++i) {
            payload_len = (payload_len << 8) | (uint64_t)ext[i];
        }
    }

    if (payload_len > BK_WS_MAX_RX_FRAME_BYTES) {
        notify_remote_disconnect(ws, 1009);
        return BK_ERR_PAYLOAD_TOO_LARGE;
    }

    uint8_t *payload = (uint8_t *)bk_port_malloc(
        (size_t)payload_len + 1U, BK_MEM_CAP_DEFAULT, "bk_ws_rx");
    if (payload == NULL) {
        return BK_ERR_NO_MEM;
    }

    if (payload_len > 0U) {
        if (stream_read_exact(ws, payload, (size_t)payload_len, timeout_ms) != BK_OK) {
            bk_port_free(payload);
            notify_remote_disconnect(ws, 1006);
            return BK_ERR_IO;
        }
    }
    payload[payload_len] = 0U;

    /* Control frames (may be interleaved during fragmented message) */
    if (opcode == (uint8_t)BK_WS_FRAME_PING) {
        (void)bk_ws_transport_send_pong(ws, payload, (size_t)payload_len);
        if (ws->cfg.rx_cb != NULL) {
            (void)ws->cfg.rx_cb(BK_WS_FRAME_PING, payload, (size_t)payload_len, ws->cfg.rx_cb_ctx);
        }
        bk_port_free(payload);
        return BK_OK;
    }

    if (opcode == (uint8_t)BK_WS_FRAME_PONG) {
        if (ws->cfg.rx_cb != NULL) {
            (void)ws->cfg.rx_cb(BK_WS_FRAME_PONG, payload, (size_t)payload_len, ws->cfg.rx_cb_ctx);
        }
        bk_port_free(payload);
        return BK_OK;
    }

    if (opcode == (uint8_t)BK_WS_FRAME_CLOSE) {
        int reason = 1000;
        if (payload_len >= 2U) {
            reason = (int)(((uint16_t)payload[0] << 8) | (uint16_t)payload[1]);
        }
        bk_port_free(payload);
        notify_remote_disconnect(ws, reason);
        return BK_OK;
    }

    /* Data & Continuation frame fragmentation state machine (RFC 6455 Section 5.4) */
    if (opcode == (uint8_t)BK_WS_FRAME_CONTINUATION) {
        if (ws->frag_opcode == 0U) {
            bk_port_free(payload);
            notify_remote_disconnect(ws, 1002);
            return BK_ERR_CORRUPTED;
        }
        if (ws->frag_len + (size_t)payload_len > BK_WS_MAX_RX_FRAME_BYTES) {
            bk_port_free(payload);
            notify_remote_disconnect(ws, 1009);
            return BK_ERR_PAYLOAD_TOO_LARGE;
        }
        uint8_t *new_buf = (uint8_t *)bk_port_malloc(
            ws->frag_len + (size_t)payload_len + 1U, BK_MEM_CAP_DEFAULT, "bk_ws_frag");
        if (new_buf == NULL) {
            bk_port_free(payload);
            return BK_ERR_NO_MEM;
        }
        if (ws->frag_len > 0U && ws->frag_buf != NULL) {
            memcpy(new_buf, ws->frag_buf, ws->frag_len);
        }
        if (payload_len > 0U) {
            memcpy(new_buf + ws->frag_len, payload, (size_t)payload_len);
        }
        bk_port_free(payload);
        if (ws->frag_buf != NULL) {
            bk_port_free(ws->frag_buf);
        }
        ws->frag_buf = new_buf;
        ws->frag_len += (size_t)payload_len;
        ws->frag_buf[ws->frag_len] = 0U;

        if (!fin) {
            return BK_OK;
        }

        bk_ws_frame_type_t completed_type = (bk_ws_frame_type_t)ws->frag_opcode;
        uint8_t *completed_buf = ws->frag_buf;
        size_t completed_len = ws->frag_len;
        ws->frag_buf = NULL;
        ws->frag_len = 0U;
        ws->frag_opcode = 0U;

        bk_err_t cb_err = BK_OK;
        if (ws->cfg.rx_cb != NULL) {
            cb_err = ws->cfg.rx_cb(completed_type, completed_buf, completed_len, ws->cfg.rx_cb_ctx);
        }
        bk_port_free(completed_buf);
        return cb_err;
    }

    /* Initial data frame (TEXT or BINARY) */
    if (ws->frag_opcode != 0U) {
        /* New data frame arrived before previous fragmented message finished */
        bk_port_free(payload);
        notify_remote_disconnect(ws, 1002);
        return BK_ERR_CORRUPTED;
    }

    if (!fin) {
        ws->frag_opcode = opcode;
        ws->frag_buf = payload;
        ws->frag_len = (size_t)payload_len;
        return BK_OK;
    }

    bk_err_t cb_err = BK_OK;
    if (ws->cfg.rx_cb != NULL) {
        cb_err = ws->cfg.rx_cb(
            (bk_ws_frame_type_t)opcode, payload, (size_t)payload_len, ws->cfg.rx_cb_ctx);
    }
    bk_port_free(payload);
    return cb_err;
}
