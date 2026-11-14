/* SPDX-License-Identifier: Apache-2.0 */
#include "bk_storage.h"
#include "bk_port_mem.h"
#include "bk_port_os.h"

#include <string.h>

#define BK_STORAGE_REC_MAGIC   0x424B5354U /* 'BKST' */
#define BK_STORAGE_COMMIT_TAG  0x434D4954U /* 'CMIT' */
#define BK_STORAGE_REC_PUT     1U
#define BK_STORAGE_REC_TOMB    2U

typedef struct {
    uint32_t magic;
    uint8_t op;
    uint8_t reserved[3];
    uint32_t flags;
    char ns[BK_STORAGE_MAX_NS_LEN];
    char key[BK_STORAGE_MAX_KEY_LEN];
    uint32_t val_len;
    uint32_t payload_crc32;
} bk_storage_rec_hdr_t;

typedef struct {
    uint32_t header_crc32;
    uint32_t commit_tag;
} bk_storage_rec_WaveTail_t;

typedef struct bk_storage_entry {
    char ns[BK_STORAGE_MAX_NS_LEN];
    char key[BK_STORAGE_MAX_KEY_LEN];
    uint8_t val[BK_STORAGE_MAX_VAL_LEN];
    size_t val_len;
    uint32_t flags;
    size_t flash_offset;
    size_t flash_total_len;
    struct bk_storage_entry *next;
} bk_storage_entry_t;

struct bk_storage {
    bk_port_mutex_t lock;
    uint8_t *flash_buf;
    size_t flash_size;
    bool owns_flash_buf;
    size_t write_offset;
    size_t recovered_tail_records;
    bk_storage_cipher_fn_t cipher_fn;
    void *cipher_ctx;
    bk_storage_entry_t *entries;
};

static void secure_zero(void *ptr, size_t len)
{
    volatile uint8_t *p = (volatile uint8_t *)ptr;
    while (len-- > 0) {
        *p++ = 0U;
    }
}

uint32_t bk_storage_crc32(const void *data, size_t len)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0; i < len; ++i) {
        crc ^= bytes[i];
        for (int b = 0; b < 8; ++b) {
            uint32_t mask = -(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

static bk_err_t default_cipher_fn(bool encrypt,
                                  const uint8_t *in_buf,
                                  uint8_t *out_buf,
                                  size_t len,
                                  void *user_ctx)
{
    (void)encrypt;
    (void)user_ctx;
    /* Deterministic platform keystream mask for sensitive blobs when no custom hook is passed */
    static const uint8_t key[16] = {
        0x5A, 0xA5, 0x72, 0x59, 0xC3, 0x3C, 0x19, 0x84,
        0xE7, 0x18, 0x9D, 0xD9, 0x4B, 0xB4, 0x2F, 0xF2
    };
    for (size_t i = 0; i < len; ++i) {
        out_buf[i] = (uint8_t)(in_buf[i] ^ key[i % sizeof(key)] ^ (uint8_t)((i * 31U) & 0xFFU));
    }
    return BK_OK;
}

static bk_storage_entry_t *find_entry(struct bk_storage *st, const char *ns, const char *key)
{
    for (bk_storage_entry_t *cur = st->entries; cur != NULL; cur = cur->next) {
        if (strncmp(cur->ns, ns, BK_STORAGE_MAX_NS_LEN) == 0 &&
            strncmp(cur->key, key, BK_STORAGE_MAX_KEY_LEN) == 0) {
            return cur;
        }
    }
    return NULL;
}

static void remove_entry(struct bk_storage *st, const char *ns, const char *key, bool scrub_flash)
{
    bk_storage_entry_t *prev = NULL;
    bk_storage_entry_t *cur = st->entries;
    while (cur != NULL) {
        if (strncmp(cur->ns, ns, BK_STORAGE_MAX_NS_LEN) == 0 &&
            (key == NULL || strncmp(cur->key, key, BK_STORAGE_MAX_KEY_LEN) == 0)) {
            bk_storage_entry_t *to_del = cur;
            if (prev == NULL) {
                st->entries = cur->next;
            } else {
                prev->next = cur->next;
            }
            cur = cur->next;
            if (scrub_flash && to_del->flash_offset + to_del->flash_total_len <= st->flash_size) {
                secure_zero(st->flash_buf + to_del->flash_offset, to_del->flash_total_len);
            }
            secure_zero(to_del, sizeof(*to_del));
            bk_port_free(to_del);
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
}

static bk_err_t replay_log(struct bk_storage *st)
{
    size_t off = 0;
    while (off + sizeof(bk_storage_rec_hdr_t) <= st->flash_size) {
        bk_storage_rec_hdr_t hdr;
        memcpy(&hdr, st->flash_buf + off, sizeof(hdr));

        if (hdr.magic == 0xFFFFFFFFU || hdr.magic == 0x00000000U) {
            /* Check if skipped zeroed region has more valid records after it */
            size_t probe = off + 1U;
            while (probe < st->flash_size &&
                   (st->flash_buf[probe] == 0x00U || st->flash_buf[probe] == 0xFFU)) {
                probe++;
            }
            if (probe >= st->flash_size) {
                break;
            }
            if (probe + sizeof(bk_storage_rec_hdr_t) <= st->flash_size) {
                uint32_t next_magic = 0;
                memcpy(&next_magic, st->flash_buf + probe, sizeof(next_magic));
                if (next_magic == BK_STORAGE_REC_MAGIC) {
                    off = probe;
                    continue;
                }
            }
            /* Unrecognized tail garbage from interrupted write: roll back tail */
            secure_zero(st->flash_buf + off, st->flash_size - off);
            st->recovered_tail_records += 1U;
            break;
        }

        if (hdr.magic != BK_STORAGE_REC_MAGIC || hdr.val_len > BK_STORAGE_MAX_VAL_LEN) {
            secure_zero(st->flash_buf + off, st->flash_size - off);
            st->recovered_tail_records += 1U;
            break;
        }

        size_t rec_total = sizeof(bk_storage_rec_hdr_t) + hdr.val_len + sizeof(bk_storage_rec_WaveTail_t);
        if (off + rec_total > st->flash_size) {
            secure_zero(st->flash_buf + off, st->flash_size - off);
            st->recovered_tail_records += 1U;
            break;
        }

        const uint8_t *payload_ptr = st->flash_buf + off + sizeof(bk_storage_rec_hdr_t);
        bk_storage_rec_WaveTail_t tail;
        memcpy(&tail, payload_ptr + hdr.val_len, sizeof(tail));

        uint32_t calc_hdr_crc = bk_storage_crc32(&hdr, sizeof(hdr));
        uint32_t calc_pay_crc = bk_storage_crc32(payload_ptr, hdr.val_len);
        if (tail.commit_tag != BK_STORAGE_COMMIT_TAG ||
            tail.header_crc32 != calc_hdr_crc ||
            hdr.payload_crc32 != calc_pay_crc) {
            /* Mid-write power loss or corrupted record: roll back incomplete record */
            secure_zero(st->flash_buf + off, st->flash_size - off);
            st->recovered_tail_records += 1U;
            break;
        }

        hdr.ns[BK_STORAGE_MAX_NS_LEN - 1U] = '\0';
        hdr.key[BK_STORAGE_MAX_KEY_LEN - 1U] = '\0';

        if (hdr.op == BK_STORAGE_REC_TOMB) {
            remove_entry(st, hdr.ns, hdr.key, false);
        } else if (hdr.op == BK_STORAGE_REC_PUT) {
            bk_storage_entry_t *entry = find_entry(st, hdr.ns, hdr.key);
            if (entry == NULL) {
                entry = (bk_storage_entry_t *)bk_port_calloc(
                    1, sizeof(*entry), BK_MEM_CAP_DEFAULT, "bk_storage_ent");
                if (entry == NULL) {
                    return BK_ERR_NO_MEM;
                }
                strncpy(entry->ns, hdr.ns, BK_STORAGE_MAX_NS_LEN - 1U);
                strncpy(entry->key, hdr.key, BK_STORAGE_MAX_KEY_LEN - 1U);
                entry->next = st->entries;
                st->entries = entry;
            } else if (entry->flags & BK_STORAGE_FLAG_SENSITIVE) {
                /* Scrub superseded sensitive record in flash */
                secure_zero(st->flash_buf + entry->flash_offset, entry->flash_total_len);
            }

            if (hdr.flags & BK_STORAGE_FLAG_SENSITIVE) {
                st->cipher_fn(false, payload_ptr, entry->val, hdr.val_len, st->cipher_ctx);
            } else if (hdr.val_len > 0) {
                memcpy(entry->val, payload_ptr, hdr.val_len);
            }
            entry->val_len = hdr.val_len;
            entry->flags = hdr.flags;
            entry->flash_offset = off;
            entry->flash_total_len = rec_total;
        }

        off += rec_total;
    }

    st->write_offset = off;
    return BK_OK;
}

bk_err_t bk_storage_open(const bk_storage_config_t *cfg, bk_storage_handle_t *out_handle)
{
    if (out_handle == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    struct bk_storage *st = (struct bk_storage *)bk_port_calloc(
        1, sizeof(*st), BK_MEM_CAP_DEFAULT, "bk_storage");
    if (st == NULL) {
        return BK_ERR_NO_MEM;
    }

    if (cfg != NULL && cfg->flash_sim_buf != NULL && cfg->flash_sim_size >= 256U) {
        st->flash_buf = cfg->flash_sim_buf;
        st->flash_size = cfg->flash_sim_size;
        st->owns_flash_buf = false;
    } else {
        st->flash_size = 8192U;
        st->flash_buf = (uint8_t *)bk_port_calloc(
            1, st->flash_size, BK_MEM_CAP_DEFAULT, "bk_storage_flash");
        if (st->flash_buf == NULL) {
            bk_port_free(st);
            return BK_ERR_NO_MEM;
        }
        st->owns_flash_buf = true;
    }

    st->cipher_fn = (cfg != NULL && cfg->cipher_fn != NULL) ? cfg->cipher_fn : default_cipher_fn;
    st->cipher_ctx = (cfg != NULL) ? cfg->cipher_ctx : NULL;

    bk_err_t err = bk_port_mutex_create(false, &st->lock);
    if (err != BK_OK) {
        if (st->owns_flash_buf) {
            bk_port_free(st->flash_buf);
        }
        bk_port_free(st);
        return err;
    }

    err = replay_log(st);
    if (err != BK_OK) {
        bk_storage_close(st);
        return err;
    }

    *out_handle = st;
    return BK_OK;
}

void bk_storage_close(bk_storage_handle_t handle)
{
    if (handle == NULL) {
        return;
    }
    remove_entry(handle, "", NULL, false);
    while (handle->entries != NULL) {
        bk_storage_entry_t *next = handle->entries->next;
        secure_zero(handle->entries, sizeof(*handle->entries));
        bk_port_free(handle->entries);
        handle->entries = next;
    }
    if (handle->lock != NULL) {
        bk_port_mutex_destroy(handle->lock);
    }
    if (handle->owns_flash_buf && handle->flash_buf != NULL) {
        secure_zero(handle->flash_buf, handle->flash_size);
        bk_port_free(handle->flash_buf);
    }
    bk_port_free(handle);
}

static bool valid_identifier(const char *s, size_t max_len)
{
    if (s == NULL || s[0] == '\0') {
        return false;
    }
    size_t len = strlen(s);
    return len > 0 && len < max_len;
}

bk_err_t bk_storage_set_blob(bk_storage_handle_t handle,
                             const char *ns,
                             const char *key,
                             const void *value,
                             size_t len,
                             uint32_t flags)
{
    if (handle == NULL || !valid_identifier(ns, BK_STORAGE_MAX_NS_LEN) ||
        !valid_identifier(key, BK_STORAGE_MAX_KEY_LEN)) {
        return BK_ERR_INVALID_ARG;
    }
    if ((len > 0 && value == NULL) || len > BK_STORAGE_MAX_VAL_LEN) {
        return BK_ERR_INVALID_ARG;
    }

    size_t rec_total = sizeof(bk_storage_rec_hdr_t) + len + sizeof(bk_storage_rec_WaveTail_t);

    bk_port_mutex_lock(handle->lock, UINT32_MAX);
    if (handle->write_offset + rec_total > handle->flash_size) {
        bk_port_mutex_unlock(handle->lock);
        return BK_ERR_NO_MEM;
    }

    uint8_t encoded[BK_STORAGE_MAX_VAL_LEN];
    if (len > 0) {
        if (flags & BK_STORAGE_FLAG_SENSITIVE) {
            bk_err_t cerr = handle->cipher_fn(
                true, (const uint8_t *)value, encoded, len, handle->cipher_ctx);
            if (cerr != BK_OK) {
                bk_port_mutex_unlock(handle->lock);
                return cerr;
            }
        } else {
            memcpy(encoded, value, len);
        }
    }

    bk_storage_rec_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = BK_STORAGE_REC_MAGIC;
    hdr.op = BK_STORAGE_REC_PUT;
    hdr.flags = flags;
    strncpy(hdr.ns, ns, BK_STORAGE_MAX_NS_LEN - 1U);
    strncpy(hdr.key, key, BK_STORAGE_MAX_KEY_LEN - 1U);
    hdr.val_len = (uint32_t)len;
    hdr.payload_crc32 = bk_storage_crc32(encoded, len);

    bk_storage_rec_WaveTail_t tail;
    tail.header_crc32 = bk_storage_crc32(&hdr, sizeof(hdr));
    tail.commit_tag = BK_STORAGE_COMMIT_TAG;

    size_t off = handle->write_offset;
    memcpy(handle->flash_buf + off, &hdr, sizeof(hdr));
    if (len > 0) {
        memcpy(handle->flash_buf + off + sizeof(hdr), encoded, len);
    }
    memcpy(handle->flash_buf + off + sizeof(hdr) + len, &tail, sizeof(tail));
    handle->write_offset += rec_total;
    secure_zero(encoded, sizeof(encoded));

    bk_storage_entry_t *entry = find_entry(handle, ns, key);
    if (entry == NULL) {
        entry = (bk_storage_entry_t *)bk_port_calloc(
            1, sizeof(*entry), BK_MEM_CAP_DEFAULT, "bk_storage_ent");
        if (entry == NULL) {
            bk_port_mutex_unlock(handle->lock);
            return BK_ERR_NO_MEM;
        }
        strncpy(entry->ns, ns, BK_STORAGE_MAX_NS_LEN - 1U);
        strncpy(entry->key, key, BK_STORAGE_MAX_KEY_LEN - 1U);
        entry->next = handle->entries;
        handle->entries = entry;
    } else if (entry->flags & BK_STORAGE_FLAG_SENSITIVE) {
        secure_zero(handle->flash_buf + entry->flash_offset, entry->flash_total_len);
    }

    secure_zero(entry->val, sizeof(entry->val));
    if (len > 0) {
        memcpy(entry->val, value, len);
    }
    entry->val_len = len;
    entry->flags = flags;
    entry->flash_offset = off;
    entry->flash_total_len = rec_total;

    bk_port_mutex_unlock(handle->lock);
    return BK_OK;
}

bk_err_t bk_storage_get_blob(bk_storage_handle_t handle,
                             const char *ns,
                             const char *key,
                             void *out_value,
                             size_t *inout_len,
                             uint32_t *out_flags)
{
    if (handle == NULL || !valid_identifier(ns, BK_STORAGE_MAX_NS_LEN) ||
        !valid_identifier(key, BK_STORAGE_MAX_KEY_LEN) || inout_len == NULL) {
        return BK_ERR_INVALID_ARG;
    }

    bk_port_mutex_lock(handle->lock, UINT32_MAX);
    bk_storage_entry_t *entry = find_entry(handle, ns, key);
    if (entry == NULL) {
        bk_port_mutex_unlock(handle->lock);
        return BK_ERR_NOT_FOUND;
    }
    if (*inout_len < entry->val_len) {
        *inout_len = entry->val_len;
        bk_port_mutex_unlock(handle->lock);
        return BK_ERR_PAYLOAD_TOO_LARGE;
    }
    if (entry->val_len > 0 && out_value != NULL) {
        memcpy(out_value, entry->val, entry->val_len);
    }
    *inout_len = entry->val_len;
    if (out_flags != NULL) {
        *out_flags = entry->flags;
    }
    bk_port_mutex_unlock(handle->lock);
    return BK_OK;
}

bk_err_t bk_storage_delete_key(bk_storage_handle_t handle,
                               const char *ns,
                               const char *key)
{
    if (handle == NULL || !valid_identifier(ns, BK_STORAGE_MAX_NS_LEN) ||
        !valid_identifier(key, BK_STORAGE_MAX_KEY_LEN)) {
        return BK_ERR_INVALID_ARG;
    }

    bk_port_mutex_lock(handle->lock, UINT32_MAX);
    bk_storage_entry_t *entry = find_entry(handle, ns, key);
    if (entry == NULL) {
        bk_port_mutex_unlock(handle->lock);
        return BK_ERR_NOT_FOUND;
    }
    remove_entry(handle, ns, key, true);
    bk_port_mutex_unlock(handle->lock);
    return BK_OK;
}

bk_err_t bk_storage_erase_ns(bk_storage_handle_t handle, const char *ns)
{
    if (handle == NULL || !valid_identifier(ns, BK_STORAGE_MAX_NS_LEN)) {
        return BK_ERR_INVALID_ARG;
    }
    bk_port_mutex_lock(handle->lock, UINT32_MAX);
    remove_entry(handle, ns, NULL, true);
    bk_port_mutex_unlock(handle->lock);
    return BK_OK;
}

bk_err_t bk_storage_factory_reset(bk_storage_handle_t handle)
{
    if (handle == NULL) {
        return BK_ERR_INVALID_ARG;
    }
    bk_port_mutex_lock(handle->lock, UINT32_MAX);
    while (handle->entries != NULL) {
        bk_storage_entry_t *next = handle->entries->next;
        secure_zero(handle->entries, sizeof(*handle->entries));
        bk_port_free(handle->entries);
        handle->entries = next;
    }
    secure_zero(handle->flash_buf, handle->flash_size);
    handle->write_offset = 0U;
    bk_port_mutex_unlock(handle->lock);
    return BK_OK;
}

size_t bk_storage_get_recovered_tail_records(bk_storage_handle_t handle)
{
    if (handle == NULL) {
        return 0U;
    }
    return handle->recovered_tail_records;
}
