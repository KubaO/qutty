/* ----------------------------------------------------------------------
 * Definitions likely to be helpful to multiple implementations.
 */

#include <stdint.h>

/*
 * Start by deciding whether we can support hardware SHA at all.
 */
#define HW_SHA1_NONE 0
#define HW_SHA1_NI 1
#define HW_SHA1_NEON 2

#ifdef _FORCE_SHA_NI
#   define HW_SHA1 HW_SHA1_NI
#elif defined(__clang__)
#   if __has_attribute(target) && __has_include(<wmmintrin.h>) &&       \
    (defined(__x86_64__) || defined(__i386))
#       define HW_SHA1 HW_SHA1_NI
#   endif
#elif defined(__GNUC__)
#    if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9)) && \
        (defined(__x86_64__) || defined(__i386))
#       define HW_SHA1 HW_SHA1_NI
#    endif
#elif defined (_MSC_VER)
#   if (defined(_M_X64) || defined(_M_IX86)) && _MSC_FULL_VER >= 150030729
#      define HW_SHA1 HW_SHA1_NI
#   endif
#endif

#ifdef _FORCE_SHA_NEON
#   define HW_SHA1 HW_SHA1_NEON
#elif defined __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    /* Arm can potentially support both endiannesses, but this code
     * hasn't been tested on anything but little. If anyone wants to
     * run big-endian, they'll need to fix it first. */
#elif defined __ARM_FEATURE_CRYPTO
    /* If the Arm crypto extension is available already, we can
     * support NEON SHA without having to enable anything by hand */
#   define HW_SHA1 HW_SHA1_NEON
#elif defined(__clang__)
#   if __has_attribute(target) && __has_include(<arm_neon.h>) &&       \
    (defined(__aarch64__))
        /* clang can enable the crypto extension in AArch64 using
         * __attribute__((target)) */
#       define HW_SHA1 HW_SHA1_NEON
#       define USE_CLANG_ATTR_TARGET_AARCH64
#   endif
#elif defined _MSC_VER
    /* Visual Studio supports the crypto extension when targeting
     * AArch64, but as of VS2017, the AArch32 header doesn't quite
     * manage it (declaring the shae/shad intrinsics without a round
     * key operand). */
#   if defined _M_ARM64
#       define HW_SHA1 HW_SHA1_NEON
#       if defined _M_ARM64
#           define USE_ARM64_NEON_H /* unusual header name in this case */
#       endif
#   endif
#endif

#if defined _FORCE_SOFTWARE_SHA || !defined HW_SHA1
#   undef HW_SHA1
#   define HW_SHA1 HW_SHA1_NONE
#endif

/*
 * The actual query function that asks if hardware acceleration is
 * available.
 */
bool sha1_hw_available(void);

/*
 * The top-level selection function, caching the results of
 * sha1_hw_available() so it only has to run once.
 */
static bool sha1_hw_available_cached(void)
{
    static bool initialised = false;
    static bool hw_available;
    if (!initialised) {
        hw_available = sha1_hw_available();
        initialised = true;
    }
    return hw_available;
}

extern const uint32_t sha1_initial_state[5];

#define SHA1_ROUNDS_PER_STAGE 20
#define SHA1_STAGE0_CONSTANT 0x5a827999
#define SHA1_STAGE1_CONSTANT 0x6ed9eba1
#define SHA1_STAGE2_CONSTANT 0x8f1bbcdc
#define SHA1_STAGE3_CONSTANT 0xca62c1d6
#define SHA1_ROUNDS (4 * SHA1_ROUNDS_PER_STAGE)

typedef struct sha1_block sha1_block;
struct sha1_block {
    uint8_t block[64];
    size_t used;
    uint64_t len;
};

static inline void sha1_block_setup(sha1_block *blk)
{
    blk->used = 0;
    blk->len = 0;
}

static inline bool sha1_block_write(
    sha1_block *blk, const void **vdata, size_t *len)
{
    size_t blkleft = sizeof(blk->block) - blk->used;
    size_t chunk = *len < blkleft ? *len : blkleft;

    const uint8_t *p = *vdata;
    memcpy(blk->block + blk->used, p, chunk);
    *vdata = p + chunk;
    *len -= chunk;
    blk->used += chunk;
    blk->len += chunk;

    if (blk->used == sizeof(blk->block)) {
        blk->used = 0;
        return true;
    }

    return false;
}

static inline void sha1_block_pad(sha1_block *blk, BinarySink *bs)
{
    uint64_t final_len = blk->len << 3;
    size_t pad = 1 + (63 & (55 - blk->used));

    put_byte(bs, 0x80);
    for (size_t i = 1; i < pad; i++)
        put_byte(bs, 0);
    put_uint64(bs, final_len);

    assert(blk->used == 0 && "Should have exactly hit a block boundary");
}
