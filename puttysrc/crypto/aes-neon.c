/* ----------------------------------------------------------------------
 * Hardware-accelerated implementation of AES using Arm NEON.
 */

#include "ssh.h"
#include "aes.h"

#if HW_AES == HW_AES_NEON

/*
 * Manually set the target architecture, if we decided above that we
 * need to.
 */
#ifdef USE_CLANG_ATTR_TARGET_AARCH64
/*
 * A spot of cheating: redefine some ACLE feature macros before
 * including arm_neon.h. Otherwise we won't get the AES intrinsics
 * defined by that header, because it will be looking at the settings
 * for the whole translation unit rather than the ones we're going to
 * put on some particular functions using __attribute__((target)).
 */
#define __ARM_NEON 1
#define __ARM_FEATURE_CRYPTO 1
#define __ARM_FEATURE_AES 1
#define FUNC_ISA __attribute__ ((target("neon,crypto")))
#endif /* USE_CLANG_ATTR_TARGET_AARCH64 */

#ifndef FUNC_ISA
#define FUNC_ISA
#endif

#ifdef USE_ARM64_NEON_H
#include <arm64_neon.h>
#else
#include <arm_neon.h>
#endif

static bool aes_hw_available(void)
{
    /*
     * For Arm, we delegate to a per-platform AES detection function,
     * because it has to be implemented by asking the operating system
     * rather than directly querying the CPU.
     *
     * That's because Arm systems commonly have multiple cores that
     * are not all alike, so any method of querying whether NEON
     * crypto instructions work on the _current_ CPU - even one as
     * crude as just trying one and catching the SIGILL - wouldn't
     * give an answer that you could still rely on the first time the
     * OS migrated your process to another CPU.
     */
    return platform_aes_hw_available();
}

/*
 * Core NEON encrypt/decrypt functions, one per length and direction.
 */

#define NEON_CIPHER(len, repmacro)                              \
    static FUNC_ISA inline uint8x16_t aes_neon_##len##_e(       \
        uint8x16_t v, const uint8x16_t *keysched)               \
    {                                                           \
        repmacro(v = vaesmcq_u8(vaeseq_u8(v, *keysched++)););   \
        v = vaeseq_u8(v, *keysched++);                          \
        return veorq_u8(v, *keysched);                          \
    }                                                           \
    static FUNC_ISA inline uint8x16_t aes_neon_##len##_d(       \
        uint8x16_t v, const uint8x16_t *keysched)               \
    {                                                           \
        repmacro(v = vaesimcq_u8(vaesdq_u8(v, *keysched++)););  \
        v = vaesdq_u8(v, *keysched++);                          \
        return veorq_u8(v, *keysched);                          \
    }

NEON_CIPHER(128, REP9)
NEON_CIPHER(192, REP11)
NEON_CIPHER(256, REP13)

/*
 * The main key expansion.
 */
static FUNC_ISA void aes_neon_key_expand(
    const unsigned char *key, size_t key_words,
    uint8x16_t *keysched_e, uint8x16_t *keysched_d)
{
    size_t rounds = key_words + 6;
    size_t sched_words = (rounds + 1) * 4;

    /*
     * Store the key schedule as 32-bit integers during expansion, so
     * that it's easy to refer back to individual previous words. We
     * collect them into the final uint8x16_t form at the end.
     */
    uint32_t sched[MAXROUNDKEYS * 4];

    unsigned rconpos = 0;

    for (size_t i = 0; i < sched_words; i++) {
        if (i < key_words) {
            sched[i] = GET_32BIT_LSB_FIRST(key + 4 * i);
        } else {
            uint32_t temp = sched[i - 1];

            bool rotate_and_round_constant = (i % key_words == 0);
            bool sub = rotate_and_round_constant ||
                (key_words == 8 && i % 8 == 4);

            if (rotate_and_round_constant)
                temp = (temp << 24) | (temp >> 8);

            if (sub) {
                uint32x4_t v32 = vdupq_n_u32(temp);
                uint8x16_t v8 = vreinterpretq_u8_u32(v32);
                v8 = vaeseq_u8(v8, vdupq_n_u8(0));
                v32 = vreinterpretq_u32_u8(v8);
                temp = vget_lane_u32(vget_low_u32(v32), 0);
            }

            if (rotate_and_round_constant) {
                assert(rconpos < lenof(key_setup_round_constants));
                temp ^= key_setup_round_constants[rconpos++];
            }

            sched[i] = sched[i - key_words] ^ temp;
        }
    }

    /*
     * Combine the key schedule words into uint8x16_t vectors and
     * store them in the output context.
     */
    for (size_t round = 0; round <= rounds; round++)
        keysched_e[round] = vreinterpretq_u8_u32(vld1q_u32(sched + 4*round));

    smemclr(sched, sizeof(sched));

    /*
     * Now prepare the modified keys for the inverse cipher.
     */
    for (size_t eround = 0; eround <= rounds; eround++) {
        size_t dround = rounds - eround;
        uint8x16_t rkey = keysched_e[eround];
        if (eround && dround)      /* neither first nor last */
            rkey = vaesimcq_u8(rkey);
        keysched_d[dround] = rkey;
    }
}

/*
 * Auxiliary routine to reverse the byte order of a vector, so that
 * the SDCTR IV can be made big-endian for feeding to the cipher.
 *
 * In fact we don't need to reverse the vector _all_ the way; we leave
 * the two lanes in MSW,LSW order, because that makes no difference to
 * the efficiency of the increment. That way we only have to reverse
 * bytes within each lane in this function.
 */
static FUNC_ISA inline uint8x16_t aes_neon_sdctr_reverse(uint8x16_t v)
{
    return vrev64q_u8(v);
}

/*
 * Auxiliary routine to increment the 128-bit counter used in SDCTR
 * mode. There's no instruction to treat a 128-bit vector as a single
 * long integer, so instead we have to increment the bottom half
 * unconditionally, and the top half if the bottom half started off as
 * all 1s (in which case there was about to be a carry).
 */
static FUNC_ISA inline uint8x16_t aes_neon_sdctr_increment(uint8x16_t in)
{
#ifdef __aarch64__
    /* There will be a carry if the low 64 bits are all 1s. */
    uint64x1_t all1 = vcreate_u64(0xFFFFFFFFFFFFFFFF);
    uint64x1_t carry = vceq_u64(vget_high_u64(vreinterpretq_u64_u8(in)), all1);

    /* Make a word whose bottom half is unconditionally all 1s, and
     * the top half is 'carry', i.e. all 0s most of the time but all
     * 1s if we need to increment the top half. Then that word is what
     * we need to _subtract_ from the input counter. */
    uint64x2_t subtrahend = vcombine_u64(carry, all1);
#else
    /* AArch32 doesn't have comparisons that operate on a 64-bit lane,
     * so we start by comparing each 32-bit half of the low 64 bits
     * _separately_ to all-1s. */
    uint32x2_t all1 = vdup_n_u32(0xFFFFFFFF);
    uint32x2_t carry = vceq_u32(
        vget_high_u32(vreinterpretq_u32_u8(in)), all1);

    /* Swap the 32-bit words of the compare output, and AND with the
     * unswapped version. Now carry is all 1s iff the bottom half of
     * the input counter was all 1s, and all 0s otherwise. */
    carry = vand_u32(carry, vrev64_u32(carry));

    /* Now make the vector to subtract in the same way as above. */
    uint64x2_t subtrahend = vreinterpretq_u64_u32(vcombine_u32(carry, all1));
#endif

    return vreinterpretq_u8_u64(
        vsubq_u64(vreinterpretq_u64_u8(in), subtrahend));
}

/*
 * The SSH interface and the cipher modes.
 */

typedef struct aes_neon_context aes_neon_context;
struct aes_neon_context {
    uint8x16_t keysched_e[MAXROUNDKEYS], keysched_d[MAXROUNDKEYS], iv;

    ssh_cipher ciph;
};

static ssh_cipher *aes_hw_new(const ssh_cipheralg *alg)
{
    if (!aes_hw_available_cached())
        return NULL;

    aes_neon_context *ctx = snew(aes_neon_context);
    ctx->ciph.vt = alg;
    return &ctx->ciph;
}

static void aes_hw_free(ssh_cipher *ciph)
{
    aes_neon_context *ctx = container_of(ciph, aes_neon_context, ciph);
    smemclr(ctx, sizeof(*ctx));
    sfree(ctx);
}

static void aes_hw_setkey(ssh_cipher *ciph, const void *vkey)
{
    aes_neon_context *ctx = container_of(ciph, aes_neon_context, ciph);
    const unsigned char *key = (const unsigned char *)vkey;

    aes_neon_key_expand(key, ctx->ciph.vt->real_keybits / 32,
                      ctx->keysched_e, ctx->keysched_d);
}

static FUNC_ISA void aes_hw_setiv_cbc(ssh_cipher *ciph, const void *iv)
{
    aes_neon_context *ctx = container_of(ciph, aes_neon_context, ciph);
    ctx->iv = vld1q_u8(iv);
}

static FUNC_ISA void aes_hw_setiv_sdctr(ssh_cipher *ciph, const void *iv)
{
    aes_neon_context *ctx = container_of(ciph, aes_neon_context, ciph);
    uint8x16_t counter = vld1q_u8(iv);
    ctx->iv = aes_neon_sdctr_reverse(counter);
}

typedef uint8x16_t (*aes_neon_fn)(uint8x16_t v, const uint8x16_t *keysched);

static FUNC_ISA inline void aes_cbc_neon_encrypt(
    ssh_cipher *ciph, void *vblk, int blklen, aes_neon_fn encrypt)
{
    aes_neon_context *ctx = container_of(ciph, aes_neon_context, ciph);

    for (uint8_t *blk = (uint8_t *)vblk, *finish = blk + blklen;
         blk < finish; blk += 16) {
        uint8x16_t plaintext = vld1q_u8(blk);
        uint8x16_t cipher_input = veorq_u8(plaintext, ctx->iv);
        uint8x16_t ciphertext = encrypt(cipher_input, ctx->keysched_e);
        vst1q_u8(blk, ciphertext);
        ctx->iv = ciphertext;
    }
}

static FUNC_ISA inline void aes_cbc_neon_decrypt(
    ssh_cipher *ciph, void *vblk, int blklen, aes_neon_fn decrypt)
{
    aes_neon_context *ctx = container_of(ciph, aes_neon_context, ciph);

    for (uint8_t *blk = (uint8_t *)vblk, *finish = blk + blklen;
         blk < finish; blk += 16) {
        uint8x16_t ciphertext = vld1q_u8(blk);
        uint8x16_t decrypted = decrypt(ciphertext, ctx->keysched_d);
        uint8x16_t plaintext = veorq_u8(decrypted, ctx->iv);
        vst1q_u8(blk, plaintext);
        ctx->iv = ciphertext;
    }
}

static FUNC_ISA inline void aes_sdctr_neon(
    ssh_cipher *ciph, void *vblk, int blklen, aes_neon_fn encrypt)
{
    aes_neon_context *ctx = container_of(ciph, aes_neon_context, ciph);

    for (uint8_t *blk = (uint8_t *)vblk, *finish = blk + blklen;
         blk < finish; blk += 16) {
        uint8x16_t counter = aes_neon_sdctr_reverse(ctx->iv);
        uint8x16_t keystream = encrypt(counter, ctx->keysched_e);
        uint8x16_t input = vld1q_u8(blk);
        uint8x16_t output = veorq_u8(input, keystream);
        vst1q_u8(blk, output);
        ctx->iv = aes_neon_sdctr_increment(ctx->iv);
    }
}

#define NEON_ENC_DEC(len)                                               \
    static FUNC_ISA void aes##len##_cbc_hw_encrypt(                     \
        ssh_cipher *ciph, void *vblk, int blklen)                       \
    { aes_cbc_neon_encrypt(ciph, vblk, blklen, aes_neon_##len##_e); }   \
    static FUNC_ISA void aes##len##_cbc_hw_decrypt(                     \
        ssh_cipher *ciph, void *vblk, int blklen)                       \
    { aes_cbc_neon_decrypt(ciph, vblk, blklen, aes_neon_##len##_d); }   \
    static FUNC_ISA void aes##len##_sdctr_hw(                           \
        ssh_cipher *ciph, void *vblk, int blklen)                       \
    { aes_sdctr_neon(ciph, vblk, blklen, aes_neon_##len##_e); }         \

NEON_ENC_DEC(128)
NEON_ENC_DEC(192)
NEON_ENC_DEC(256)

#endif
