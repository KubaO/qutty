/*
 * SHA-512 algorithm as described at
 *
 *   http://csrc.nist.gov/cryptval/shs.html
 *
 * Modifications made for SHA-384 also
 */

/*
 * Start by deciding whether we can support hardware SHA at all.
 */
#define HW_SHA512_NONE 0
#define HW_SHA512_NEON 1

#ifdef _FORCE_SHA512_NEON
#   define HW_SHA512 HW_SHA512_NEON
#elif defined __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    /* Arm can potentially support both endiannesses, but this code
     * hasn't been tested on anything but little. If anyone wants to
     * run big-endian, they'll need to fix it first. */
#elif defined __ARM_FEATURE_SHA512
    /* If the Arm SHA-512 extension is available already, we can
     * support NEON SHA without having to enable anything by hand */
#   define HW_SHA512 HW_SHA512_NEON
#elif defined(__clang__)
#   if __has_attribute(target) && __has_include(<arm_neon.h>) &&       \
    (defined(__aarch64__))
        /* clang can enable the crypto extension in AArch64 using
         * __attribute__((target)) */
#       define HW_SHA512 HW_SHA512_NEON
#       define USE_CLANG_ATTR_TARGET_AARCH64
#   endif
#endif

#if defined _FORCE_SOFTWARE_SHA || !defined HW_SHA512
#   undef HW_SHA512
#   define HW_SHA512 HW_SHA512_NONE
#endif

/*
 * The actual query function that asks if hardware acceleration is
 * available.
 */
bool sha512_hw_available(void);

extern const uint64_t sha512_initial_state[8];
extern const uint64_t sha384_initial_state[8];
extern const uint64_t sha512_round_constants[80];

#define SHA512_ROUNDS 80

typedef struct sha512_block sha512_block;
struct sha512_block {
    uint8_t block[128];
    size_t used;
    uint64_t lenhi, lenlo;
};

static inline void sha512_block_setup(sha512_block *blk)
{
    blk->used = 0;
    blk->lenhi = blk->lenlo = 0;
}

static inline bool sha512_block_write(
    sha512_block *blk, const void **vdata, size_t *len)
{
    size_t blkleft = sizeof(blk->block) - blk->used;
    size_t chunk = *len < blkleft ? *len : blkleft;

    const uint8_t *p = *vdata;
    memcpy(blk->block + blk->used, p, chunk);
    *vdata = p + chunk;
    *len -= chunk;
    blk->used += chunk;

    size_t chunkbits = chunk << 3;

    blk->lenlo += chunkbits;
    blk->lenhi += (blk->lenlo < chunkbits);

    if (blk->used == sizeof(blk->block)) {
        blk->used = 0;
        return true;
    }

    return false;
}

static inline void sha512_block_pad(sha512_block *blk, BinarySink *bs)
{
    uint64_t final_lenhi = blk->lenhi;
    uint64_t final_lenlo = blk->lenlo;
    size_t pad = 127 & (111 - blk->used);

    put_byte(bs, 0x80);
    put_padding(bs, pad, 0);
    put_uint64(bs, final_lenhi);
    put_uint64(bs, final_lenlo);

    assert(blk->used == 0 && "Should have exactly hit a block boundary");
}
