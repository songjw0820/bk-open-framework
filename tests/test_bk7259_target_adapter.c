/* SPDX-License-Identifier: Apache-2.0 */
#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L

#include "bk_audio_session.h"
#include "bk_board.h"
#include "bk_ota.h"
#include "bk_plugin_agora.h"
#include "bk_port_cache.h"
#include "bk_port_mem.h"
#include "bk_port_os.h"
#include "bk_protocol_xiaozhi.h"
#include "bk_robot_product.h"
#include "bk_ws_transport.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include <components/system.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <modules/opus.h>
#include <os/mem.h>
#include <os/os.h>

#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* --- Armino SDK stub implementations for target adapter verification --- */

static uint32_t g_mock_verify_result = 0U;
static char g_last_sni_hostname[128] = {0};
static int g_last_authmode = 0;
static int g_opus_encode_calls = 0;
static int g_opus_decode_calls = 0;

void *os_malloc(size_t size)       { return malloc(size); }
void *os_zalloc(size_t size)       { return calloc(1, size); }
void *os_sram_malloc(size_t size)  { return malloc(size); }
void *psram_malloc(size_t size)    { return malloc(size); }
void *psram_cache_malloc(size_t s) { return malloc(s); }
void os_free(void *ptr)            { free(ptr); }

bool bk_dcache_addr_is_cacheable(const void *addr) { return addr != NULL; }
void bk_dcache_clean_for_producer(void *addr, uint32_t size) { (void)addr; (void)size; }
void bk_dcache_invalidate_for_consumer(void *addr, uint32_t size) { (void)addr; (void)size; }

int bk_rand(void)
{
    static uint32_t s = 0xA5A55A5AU;
    s = s * 1664525U + 1013904223U;
    return (int)s;
}

int bk_fill_rand(void *buf, size_t len)
{
    if (buf == NULL || len == 0U) {
        return -1;
    }
    uint8_t *out = (uint8_t *)buf;
    for (size_t i = 0U; i < len; ++i) {
        out[i] = (uint8_t)bk_rand();
    }
    return 0;
}

bk_err_t rtos_init_mutex(beken_mutex_t *mutex)
{
    pthread_mutex_t *m = (pthread_mutex_t *)calloc(1, sizeof(*m));
    pthread_mutex_init(m, NULL);
    *mutex = m;
    return BK_OK;
}

bk_err_t rtos_init_recursive_mutex(beken_mutex_t *mutex)
{
    pthread_mutex_t *m = (pthread_mutex_t *)calloc(1, sizeof(*m));
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &attr);
    pthread_mutexattr_destroy(&attr);
    *mutex = m;
    return BK_OK;
}

bk_err_t rtos_lock_mutex_timeout(beken_mutex_t *mutex, uint32_t timeout_ms)
{
    (void)timeout_ms;
    return pthread_mutex_lock((pthread_mutex_t *)*mutex) == 0 ? BK_OK : BK_ERR_TIMEOUT;
}

bk_err_t rtos_lock_recursive_mutex(beken_mutex_t *mutex)
{
    return pthread_mutex_lock((pthread_mutex_t *)*mutex) == 0 ? BK_OK : BK_ERR_TIMEOUT;
}

bk_err_t rtos_unlock_mutex(beken_mutex_t *mutex)
{
    return pthread_mutex_unlock((pthread_mutex_t *)*mutex) == 0 ? BK_OK : BK_ERR_INVALID_STATE;
}

bk_err_t rtos_unlock_recursive_mutex(beken_mutex_t *mutex)
{
    return pthread_mutex_unlock((pthread_mutex_t *)*mutex) == 0 ? BK_OK : BK_ERR_INVALID_STATE;
}

bk_err_t rtos_deinit_mutex(beken_mutex_t *mutex)
{
    pthread_mutex_destroy((pthread_mutex_t *)*mutex);
    free(*mutex);
    return BK_OK;
}

bk_err_t rtos_deinit_recursive_mutex(beken_mutex_t *mutex)
{
    return rtos_deinit_mutex(mutex);
}

bk_err_t rtos_init_semaphore(beken_semaphore_t *semaphore, int max_count)
{
    (void)max_count;
    *semaphore = calloc(1, sizeof(int));
    return BK_OK;
}

bk_err_t rtos_set_semaphore(beken_semaphore_t *semaphore)
{
    *(int *)(*semaphore) = 1;
    return BK_OK;
}

bk_err_t rtos_get_semaphore(beken_semaphore_t *semaphore, uint32_t timeout_ms)
{
    (void)semaphore;
    (void)timeout_ms;
    return BK_OK;
}

bk_err_t rtos_deinit_semaphore(beken_semaphore_t *semaphore)
{
    free(*semaphore);
    return BK_OK;
}

bk_err_t rtos_create_thread(beken_thread_t *thread,
                            uint8_t priority,
                            const char *name,
                            beken_thread_function_t function,
                            uint32_t stack_size,
                            beken_thread_arg_t arg)
{
    (void)priority;
    (void)name;
    (void)stack_size;
    *thread = NULL;
    function(arg);
    return BK_OK;
}

bk_err_t rtos_delete_thread(beken_thread_t *thread)
{
    (void)thread;
    return BK_OK;
}

uint32_t rtos_get_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000U + ts.tv_nsec / 1000000U);
}

bk_err_t rtos_delay_milliseconds(uint32_t num_ms)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(num_ms / 1000U);
    ts.tv_nsec = (long)((num_ms % 1000U) * 1000000L);
    nanosleep(&ts, NULL);
    return BK_OK;
}

/* --- mbedTLS stub implementations --- */

int mbedtls_net_send(void *ctx, const unsigned char *buf, size_t len)
{
    mbedtls_net_context *net = (mbedtls_net_context *)ctx;
    return (int)send(net->fd, buf, len, 0);
}

int mbedtls_net_recv(void *ctx, unsigned char *buf, size_t len)
{
    mbedtls_net_context *net = (mbedtls_net_context *)ctx;
    return (int)recv(net->fd, buf, len, 0);
}

void mbedtls_x509_crt_init(mbedtls_x509_crt *crt) { memset(crt, 0, sizeof(*crt)); }
void mbedtls_x509_crt_free(mbedtls_x509_crt *crt) { memset(crt, 0, sizeof(*crt)); }

int mbedtls_x509_crt_parse(mbedtls_x509_crt *chain, const unsigned char *buf, size_t buflen)
{
    if (chain == NULL || buf == NULL || buflen < 16U ||
        strstr((const char *)buf, "BEGIN CERTIFICATE") == NULL) {
        return -1;
    }
    chain->parsed_ok = 1;
    return 0;
}

void mbedtls_entropy_init(mbedtls_entropy_context *ctx) { memset(ctx, 0, sizeof(*ctx)); }
void mbedtls_entropy_free(mbedtls_entropy_context *ctx) { memset(ctx, 0, sizeof(*ctx)); }

int mbedtls_entropy_add_source(mbedtls_entropy_context *ctx,
                               mbedtls_entropy_f_source_ptr f_source,
                               void *p_source,
                               size_t threshold,
                               int strong)
{
    (void)threshold;
    (void)strong;
    ctx->source_fn = f_source;
    ctx->source_data = p_source;
    return 0;
}

int mbedtls_entropy_func(void *data, unsigned char *output, size_t len)
{
    mbedtls_entropy_context *ctx = (mbedtls_entropy_context *)data;
    size_t olen = 0U;
    return ctx->source_fn ? ctx->source_fn(ctx->source_data, output, len, &olen) : -1;
}

void mbedtls_ctr_drbg_init(mbedtls_ctr_drbg_context *ctx) { memset(ctx, 0, sizeof(*ctx)); }
void mbedtls_ctr_drbg_free(mbedtls_ctr_drbg_context *ctx) { memset(ctx, 0, sizeof(*ctx)); }

int mbedtls_ctr_drbg_seed(mbedtls_ctr_drbg_context *ctx,
                          int (*f_entropy)(void *, unsigned char *, size_t),
                          void *p_entropy,
                          const unsigned char *custom,
                          size_t len)
{
    (void)custom;
    (void)len;
    unsigned char seed[16];
    if (f_entropy(p_entropy, seed, sizeof(seed)) != 0) {
        return -1;
    }
    ctx->seeded = 1;
    return 0;
}

int mbedtls_ctr_drbg_random(void *p_rng, unsigned char *output, size_t output_len)
{
    (void)p_rng;
    return bk_port_get_random(output, output_len) == BK_OK ? 0 : -1;
}

void mbedtls_ssl_init(mbedtls_ssl_context *ssl) { memset(ssl, 0, sizeof(*ssl)); }
void mbedtls_ssl_free(mbedtls_ssl_context *ssl) { memset(ssl, 0, sizeof(*ssl)); }
void mbedtls_ssl_config_init(mbedtls_ssl_config *conf) { memset(conf, 0, sizeof(*conf)); }
void mbedtls_ssl_config_free(mbedtls_ssl_config *conf) { memset(conf, 0, sizeof(*conf)); }

int mbedtls_ssl_config_defaults(mbedtls_ssl_config *conf,
                                int endpoint,
                                int transport,
                                int preset)
{
    (void)conf;
    (void)endpoint;
    (void)transport;
    (void)preset;
    return 0;
}

void mbedtls_ssl_conf_authmode(mbedtls_ssl_config *conf, int authmode)
{
    conf->authmode = authmode;
    g_last_authmode = authmode;
}

void mbedtls_ssl_conf_ca_chain(mbedtls_ssl_config *conf,
                               mbedtls_x509_crt *ca_chain,
                               void *ca_crl)
{
    (void)ca_crl;
    conf->ca_chain = ca_chain;
}

void mbedtls_ssl_conf_rng(mbedtls_ssl_config *conf,
                          int (*f_rng)(void *, unsigned char *, size_t),
                          void *p_rng)
{
    (void)conf;
    (void)f_rng;
    (void)p_rng;
}

int mbedtls_ssl_setup(mbedtls_ssl_context *ssl, const mbedtls_ssl_config *conf)
{
    ssl->conf = conf;
    return 0;
}

int mbedtls_ssl_set_hostname(mbedtls_ssl_context *ssl, const char *hostname)
{
    if (hostname == NULL || hostname[0] == '\0') {
        return -1;
    }
    snprintf(ssl->hostname, sizeof(ssl->hostname), "%s", hostname);
    snprintf(g_last_sni_hostname, sizeof(g_last_sni_hostname), "%s", hostname);
    return 0;
}

void mbedtls_ssl_set_bio(mbedtls_ssl_context *ssl,
                         void *p_bio,
                         mbedtls_ssl_send_t *f_send,
                         mbedtls_ssl_recv_t *f_recv,
                         mbedtls_ssl_recv_timeout_t *f_recv_timeout)
{
    (void)f_recv_timeout;
    ssl->p_bio = p_bio;
    ssl->f_send = f_send;
    ssl->f_recv = f_recv;
}

int mbedtls_ssl_handshake(mbedtls_ssl_context *ssl)
{
    if (ssl->conf == NULL || ssl->conf->ca_chain == NULL || !ssl->conf->ca_chain->parsed_ok) {
        return -1;
    }
    ssl->handshake_done = 1;
    return 0;
}

uint32_t mbedtls_ssl_get_verify_result(const mbedtls_ssl_context *ssl)
{
    (void)ssl;
    return g_mock_verify_result;
}

size_t mbedtls_ssl_get_bytes_avail(const mbedtls_ssl_context *ssl)
{
    (void)ssl;
    return 0U;
}

static int g_want_read_remaining = 0;

int mbedtls_ssl_read(mbedtls_ssl_context *ssl, unsigned char *buf, size_t len)
{
    if (g_want_read_remaining > 0) {
        g_want_read_remaining -= 1;
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    return ssl->f_recv(ssl->p_bio, buf, len);
}

int mbedtls_ssl_write(mbedtls_ssl_context *ssl, const unsigned char *buf, size_t len)
{
    return ssl->f_send(ssl->p_bio, buf, len);
}

int mbedtls_ssl_close_notify(mbedtls_ssl_context *ssl)
{
    (void)ssl;
    return 0;
}

/* --- Armino <modules/opus.h> stub implementations --- */

struct OpusEncoder {
    int32_t fs;
    int channels;
};

struct OpusDecoder {
    int32_t fs;
    int channels;
};

OpusEncoder *opus_encoder_create(int32_t Fs, int channels, int application, int *error)
{
    (void)application;
    if (Fs != 16000 || channels != 1) {
        if (error) *error = -1;
        return NULL;
    }
    OpusEncoder *e = (OpusEncoder *)calloc(1, sizeof(*e));
    e->fs = Fs;
    e->channels = channels;
    if (error) *error = OPUS_OK;
    return e;
}

int32_t opus_encode(OpusEncoder *st,
                    const int16_t *pcm,
                    int frame_size,
                    unsigned char *data,
                    int32_t max_data_bytes)
{
    if (st == NULL || pcm == NULL || frame_size != 960 || max_data_bytes < 32) {
        return -1;
    }
    g_opus_encode_calls += 1;
    /* Emit RFC 6716 TOC byte (config 19: SILK WB 60ms mono = 0x98) + 24-byte compressed frame */
    data[0] = 0x98U;
    for (int i = 1; i < 24; ++i) {
        data[i] = (unsigned char)((pcm[i * 10] >> 8) & 0xFF);
    }
    return 24;
}

void opus_encoder_destroy(OpusEncoder *st) { free(st); }

OpusDecoder *opus_decoder_create(int32_t Fs, int channels, int *error)
{
    if (Fs != 16000 || channels != 1) {
        if (error) *error = -1;
        return NULL;
    }
    OpusDecoder *d = (OpusDecoder *)calloc(1, sizeof(*d));
    d->fs = Fs;
    d->channels = channels;
    if (error) *error = OPUS_OK;
    return d;
}

int32_t opus_decode(OpusDecoder *st,
                    const unsigned char *data,
                    int32_t len,
                    int16_t *pcm,
                    int frame_size,
                    int decode_fec)
{
    (void)decode_fec;
    if (st == NULL || data == NULL || len < 2 || frame_size != 960) {
        return -1;
    }
    g_opus_decode_calls += 1;
    for (int i = 0; i < 960; ++i) {
        pcm[i] = (int16_t)((int16_t)data[1 + (i % (len - 1))] << 8);
    }
    return 960;
}

void opus_decoder_destroy(OpusDecoder *st) { free(st); }

/* --- Loopback server for BK7259 mbedTLS wss:// verification --- */

static void *bk7259_wss_server_thread(void *arg)
{
    int listen_fd = *(int *)arg;
    int cfd = accept(listen_fd, NULL, NULL);
    if (cfd < 0) {
        return NULL;
    }
    char req[1024];
    size_t rlen = 0U;
    while (rlen + 1U < sizeof(req)) {
        char c = 0;
        if (recv(cfd, &c, 1, 0) <= 0) {
            close(cfd);
            return NULL;
        }
        req[rlen++] = c;
        req[rlen] = '\0';
        if (rlen >= 4U && memcmp(&req[rlen - 4U], "\r\n\r\n", 4U) == 0) {
            break;
        }
    }
    const char *k = strstr(req, "Sec-WebSocket-Key: ");
    assert(k != NULL);
    k += strlen("Sec-WebSocket-Key: ");
    const char *ke = strstr(k, "\r\n");
    assert(ke != NULL);
    char sec_key[64] = {0};
    memcpy(sec_key, k, (size_t)(ke - k));
    char accept_key[64] = {0};
    assert(bk_ws_compute_accept_key(sec_key, accept_key, sizeof(accept_key)) == BK_OK);

    char resp[512];
    int n = snprintf(resp,
                     sizeof(resp),
                     "HTTP/1.1 101 Switching Protocols\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "Sec-WebSocket-Accept: %s\r\n\r\n",
                     accept_key);
    (void)send(cfd, resp, (size_t)n, 0);
    close(cfd);
    return NULL;
}

int main(void)
{
    assert(bk_port_get_random(NULL, 0U) == BK_OK);
    assert(bk_port_malloc(SIZE_MAX, BK_MEM_CAP_DEFAULT, "overflow") == NULL);
    assert(bk_port_calloc(SIZE_MAX, 2U, BK_MEM_CAP_DEFAULT, "overflow") == NULL);
    assert(bk_board_init(BK_BOARD_PROFILE_ROBOT) == BK_OK);

    static const uint8_t k_valid_ca_pem[] =
        "-----BEGIN CERTIFICATE-----\n"
        "MIIB...BK7259_ROOT_CA_MOCK...\n"
        "-----END CERTIFICATE-----\n";

    /* 1. Verify wss:// without CA PEM fails closed with BK_ERR_NET_TLS_VERIFY_FAILED */
    int lfd0 = socket(AF_INET, SOCK_STREAM, 0);
    assert(lfd0 >= 0);
    struct sockaddr_in addr0;
    memset(&addr0, 0, sizeof(addr0));
    addr0.sin_family = AF_INET;
    assert(inet_pton(AF_INET, "127.0.0.1", &addr0.sin_addr) == 1);
    addr0.sin_port = 0;
    assert(bind(lfd0, (struct sockaddr *)&addr0, sizeof(addr0)) == 0);
    assert(listen(lfd0, 1) == 0);
    socklen_t alen0 = sizeof(addr0);
    assert(getsockname(lfd0, (struct sockaddr *)&addr0, &alen0) == 0);

    char uri[128];
    snprintf(uri, sizeof(uri), "wss://127.0.0.1:%u/xiaozhi/v1/", (unsigned int)ntohs(addr0.sin_port));

    bk_ws_transport_config_t no_ca_cfg = {
        .uri = uri,
        .verify_cert_chain = true,
        .verify_sni = true,
        .ca_cert_pem = NULL,
        .ca_cert_len = 0U,
    };
    bk_ws_transport_handle_t ws = NULL;
    assert(bk_ws_transport_create(&no_ca_cfg, &ws) == BK_OK);
    assert(bk_ws_transport_connect(ws) == BK_ERR_NET_TLS_VERIFY_FAILED);
    bk_ws_transport_destroy(ws);
    close(lfd0);

    /* 2. Verify wss:// with certificate verification failure (expired/untrusted) is rejected */
    int lfd1 = socket(AF_INET, SOCK_STREAM, 0);
    assert(lfd1 >= 0);
    memset(&addr0, 0, sizeof(addr0));
    addr0.sin_family = AF_INET;
    assert(inet_pton(AF_INET, "127.0.0.1", &addr0.sin_addr) == 1);
    assert(bind(lfd1, (struct sockaddr *)&addr0, sizeof(addr0)) == 0);
    assert(listen(lfd1, 1) == 0);
    alen0 = sizeof(addr0);
    assert(getsockname(lfd1, (struct sockaddr *)&addr0, &alen0) == 0);
    snprintf(uri, sizeof(uri), "wss://127.0.0.1:%u/xiaozhi/v1/", (unsigned int)ntohs(addr0.sin_port));

    g_mock_verify_result = 0x08U; /* MBEDTLS_X509_BADCERT_NOT_TRUSTED */
    bk_ws_transport_config_t bad_cert_cfg = {
        .uri = uri,
        .verify_cert_chain = true,
        .verify_sni = true,
        .ca_cert_pem = k_valid_ca_pem,
        .ca_cert_len = sizeof(k_valid_ca_pem),
        .sni_hostname = "api.tenclass.net",
    };
    assert(bk_ws_transport_create(&bad_cert_cfg, &ws) == BK_OK);
    assert(bk_ws_transport_connect(ws) == BK_ERR_NET_TLS_VERIFY_FAILED);
    bk_ws_transport_destroy(ws);
    close(lfd1);

    /* 3. Verify wss:// with valid CA PEM + SNI + mbedTLS + <modules/opus.h> succeeds */
    g_mock_verify_result = 0U;
    g_want_read_remaining = 3;
    int lfd2 = socket(AF_INET, SOCK_STREAM, 0);
    assert(lfd2 >= 0);
    memset(&addr0, 0, sizeof(addr0));
    addr0.sin_family = AF_INET;
    assert(inet_pton(AF_INET, "127.0.0.1", &addr0.sin_addr) == 1);
    assert(bind(lfd2, (struct sockaddr *)&addr0, sizeof(addr0)) == 0);
    assert(listen(lfd2, 1) == 0);
    alen0 = sizeof(addr0);
    assert(getsockname(lfd2, (struct sockaddr *)&addr0, &alen0) == 0);
    snprintf(uri, sizeof(uri), "wss://127.0.0.1:%u/xiaozhi/v1/", (unsigned int)ntohs(addr0.sin_port));

    pthread_t srv_tid;
    assert(pthread_create(&srv_tid, NULL, bk7259_wss_server_thread, &lfd2) == 0);

    bk_ws_transport_config_t good_wss_cfg = {
        .uri = uri,
        .verify_cert_chain = true,
        .verify_sni = true,
        .ca_cert_pem = k_valid_ca_pem,
        .ca_cert_len = sizeof(k_valid_ca_pem),
        .sni_hostname = "api.tenclass.net",
        .auth_token = "bk7259-wss-token",
        .protocol_version = 1U,
        .device_id = "c8:47:8c:72:59:99",
        .client_id = "72590000-0000-4000-8000-000000007259",
        .require_xiaozhi_headers = true,
    };
    assert(bk_ws_transport_create(&good_wss_cfg, &ws) == BK_OK);
    assert(bk_ws_transport_connect(ws) == BK_OK);
    assert(bk_ws_transport_is_connected(ws));
    assert(g_want_read_remaining == 0);
    assert(g_last_authmode == MBEDTLS_SSL_VERIFY_REQUIRED);
    assert(strcmp(g_last_sni_hostname, "api.tenclass.net") == 0);

    pthread_join(srv_tid, NULL);
    close(lfd2);

    /* Verify <modules/opus.h> direct target linkage in bk_protocol_xiaozhi.c */
    bk_audio_session_config_t acfg = {.downlink_ring_frames = 6, .enable_aec = true};
    bk_audio_session_handle_t audio = NULL;
    assert(bk_audio_session_create(&acfg, &audio) == BK_OK);

    bk_xiaozhi_config_t xcfg = {
        .ws = ws,
        .audio_session = audio,
    };
    bk_xiaozhi_handle_t xz = NULL;
    assert(bk_xiaozhi_create(&xcfg, &xz) == BK_OK);
    assert(bk_xiaozhi_is_native_libopus_loaded(xz));

    const char *srv_hello = "{\"type\":\"hello\",\"session_id\":\"bk7259-tls\",\"audio_params\":{\"frame_duration\":60}}";
    assert(bk_xiaozhi_on_text_frame(xz, srv_hello, strlen(srv_hello)) == BK_OK);

    uint8_t dummy_opus[24] = {0x98U, 1, 2, 3};
    assert(bk_xiaozhi_on_binary_opus_60ms(xz, dummy_opus, sizeof(dummy_opus)) == BK_OK);
    assert(g_opus_decode_calls == 1);
    assert(bk_audio_session_get_queued_frames(audio) == 3U);

    bk_xiaozhi_destroy(xz);
    bk_audio_session_destroy(audio);

    /* Verify bk_robot_product controller & MCP tools under BK7259 target build */
    bk_robot_product_app_t robot_app;
    assert(bk_robot_product_init(&robot_app, ws) == BK_OK);
    char mcp_out[256] = {0};
    assert(bk_robot_product_mcp_dispatch("tools/list", "{}", mcp_out, sizeof(mcp_out), &robot_app) == BK_OK);
    assert(strstr(mcp_out, "\"reset_head\"") != NULL);
    assert(strstr(mcp_out, "\"set_emotion\"") != NULL);
    assert(bk_robot_product_mcp_dispatch(
               "tools/call", "{\"name\":\"set_emotion\",\"arguments\":{\"emotion\":\"surprised\"}}",
               mcp_out, sizeof(mcp_out), &robot_app) == BK_OK);
    assert(strstr(mcp_out, "\"surprised\"") != NULL);
    assert(bk_robot_product_mcp_dispatch(
               "tools/call", "{\"name\":\"nod_head\"}", mcp_out, sizeof(mcp_out), &robot_app) == BK_OK);
    assert(strstr(mcp_out, "\"status\":\"ok\"") != NULL);
    assert(bk_robot_product_tick(&robot_app, 3900U, false, robot_app.preset_start_ms + 180ULL) == BK_OK);
    assert(robot_app.motion_allowed);
    assert(bk_robot_product_mcp_dispatch(
               "tools/call", "{\"name\":\"reset_head\"}", mcp_out, sizeof(mcp_out), &robot_app) == BK_OK);
    assert(bk_robot_product_mcp_dispatch(
               "tools/call", "{\"name\":\"get_battery_status\"}", mcp_out, sizeof(mcp_out), &robot_app) == BK_OK);
    assert(strstr(mcp_out, "\"soc_pct\"") != NULL);
    assert(bk_robot_product_mcp_dispatch(
               "tools/ping", "{}", mcp_out, sizeof(mcp_out), &robot_app) == BK_OK);
    assert(strstr(mcp_out, "\"unsupported_method\"") != NULL);
    assert(bk_robot_product_mcp_dispatch(
               "tools/call", "{\"name\":\"nonexistent\"}", mcp_out, sizeof(mcp_out), &robot_app) == BK_OK);
    assert(strstr(mcp_out, "\"unknown_tool\"") != NULL);
    assert(bk_robot_product_mcp_dispatch(
               "tools/call", "{\"name\":\"set_emotion\"}", mcp_out, sizeof(mcp_out), &robot_app) == BK_ERR_INVALID_ARG);
    for (int i = 0; i < 4; ++i) {
        assert(bk_robot_product_tick(&robot_app, 3250U, false, 5000ULL + (uint64_t)i * 100ULL) == BK_OK);
    }
    assert(!robot_app.motion_allowed);
    assert(bk_robot_product_mcp_dispatch(
               "tools/call", "{\"name\":\"nod_head\"}", mcp_out, sizeof(mcp_out), &robot_app) == BK_OK);
    assert(strstr(mcp_out, "\"blocked_low_battery\"") != NULL);
    bk_robot_product_deinit(&robot_app);

    /* Verify plugin_agora, bk_ota, and bk_esp_compat target symbols */
    assert(!bk_plugin_agora_is_compiled_in());
    assert(!bk_plugin_agora_is_sdk_linked());
    uint8_t dummy_staging[16] = {0};
    bk_ota_config_t bad_ota_cfg = {
        .strategy = BK_OTA_STRATEGY_OVERWRITE,
        .ota_partition_bytes = 264U * 1024U,
        .has_control_partition = false,
        .staging_buf = dummy_staging,
        .staging_buf_cap = sizeof(dummy_staging),
    };
    bk_ota_handle_t ota_handle = NULL;
    assert(bk_ota_create(&bad_ota_cfg, &ota_handle) == BK_ERR_NOT_VERIFIED);
    assert(nvs_flash_init() == ESP_OK);
    assert(esp_timer_get_time() >= 0);

    bk_ws_transport_destroy(ws);

    printf("PASS: test_bk7259_target_adapter (BK7259 mbedTLS CA/SNI verify, rtos/psram/dcache port, <modules/opus.h>, robot product, ota, esp_compat)\n");
    return 0;
}
