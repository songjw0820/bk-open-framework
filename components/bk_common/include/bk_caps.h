/* SPDX-License-Identifier: Apache-2.0 */
#ifndef BK_OPEN_FRAMEWORK_BK_CAPS_H
#define BK_OPEN_FRAMEWORK_BK_CAPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Static SDK alignment facts verified from bekencorp/bk_avdk_smp (release/v4.0.1.11):
 * - ap/middleware/arch/cortex-m/l2_cache.c: L2C_CACHE_LINE_SIZE = 32U
 * - ap/components/bk_frame_buffer/include/bk_frame_queue.h: BK_FRAME_QUEUE_ALIGN_SIZE = 64U
 */
#define BK_PL310_L2_CACHE_LINE_BYTES  32U
#define BK_FRAME_QUEUE_ALIGN_BYTES    64U

typedef enum {
    BK_CAP_STATE_UNVERIFIED        = 0,
    BK_CAP_STATE_STATIC_VERIFIED   = 1,
    BK_CAP_STATE_HARDWARE_VERIFIED = 2,
} bk_cap_verify_state_t;

typedef struct {
    const char *name;
    bk_cap_verify_state_t verify_state;
    uint32_t flags;
} bk_cap_descriptor_t;

static inline bool bk_cap_is_usable(const bk_cap_descriptor_t *cap, bool require_hardware_verified)
{
    if (cap == NULL) {
        return false;
    }
    if (require_hardware_verified) {
        return cap->verify_state == BK_CAP_STATE_HARDWARE_VERIFIED;
    }
    return cap->verify_state == BK_CAP_STATE_STATIC_VERIFIED ||
           cap->verify_state == BK_CAP_STATE_HARDWARE_VERIFIED;
}

#ifdef __cplusplus
}
#endif

#endif /* BK_OPEN_FRAMEWORK_BK_CAPS_H */
