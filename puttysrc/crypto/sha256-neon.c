/* ----------------------------------------------------------------------
 * Hardware-accelerated implementation of SHA-256 using Arm NEON.
 */

#include "ssh.h"
#include "sha256.h"

#if HW_SHA256 == HW_SHA256_NEON

/*
 * Manually set the target architecture, if we decided above that we
 * need to.
 */
#ifdef USE_CLANG_ATTR_TARGET_AARCH64
/*
 * A spot of cheating: redefine some ACLE feature macros before
 * including arm_neon.h. Otherwise we won't get the SHA intrinsics
 * defined by that header, because it will be looking at the settings
 * for the whole translation unit rather than the ones we're going to
 * put on some particular functions using __attribute__((target)).
 */
#define __ARM_NEON 1
#define __ARM_FEATURE_CRYPTO 1
#define __ARM_FEATURE_SHA2 1
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

bool sha256_hw_available(void)
{
    /*
     * For Arm, we delegate to a per-platform detection function (see
     * explanation in sshaes.c).
     */
    return platform_sha256_hw_available();
}

typedef struct sha256_neon_core sha256_neon_core;
struct sha256_neon_core {
    uint32x4_t abcd, efgh;
};

FUNC_ISA
static inline uint32x4_t sha256_neon_load_input(const uint8_t *p)
{
    return vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(p)));
}

FUNC_ISA
static inline uint32x4_t sha256_neon_schedule_update(
    uint32x4_t m4, uint32x4_t m3, uint32x4_t m2, uint32x4_t m1)
{
    return vsha256su1q_u32(vsha256su0q_u32(m4, m3), m2, m1);
}

FUNC_ISA
static inline sha256_neon_core sha256_neon_round4(
    sha256_neon_core old, uint32x4_t sched, unsigned round)
{
    sha256_neon_core new;

    uint32x4_t round_input = vaddq_u32(
        sched, vld1q_u32(sha256_round_constants + round));
    new.abcd = vsha256hq_u32 (old.abcd, old.efgh, round_input);
    new.efgh = vsha256h2q_u32(old.efgh, old.abcd, round_input);
    return new;
}

FUNC_ISA
static inline void sha256_neon_block(sha256_neon_core *core, const uint8_t *p)
{
    uint32x4_t s0, s1, s2, s3;
    sha256_neon_core cr = *core;

    s0 = sha256_neon_load_input(p);
    cr = sha256_neon_round4(cr, s0, 0);
    s1 = sha256_neon_load_input(p+16);
    cr = sha256_neon_round4(cr, s1, 4);
    s2 = sha256_neon_load_input(p+32);
    cr = sha256_neon_round4(cr, s2, 8);
    s3 = sha256_neon_load_input(p+48);
    cr = sha256_neon_round4(cr, s3, 12);
    s0 = sha256_neon_schedule_update(s0, s1, s2, s3);
    cr = sha256_neon_round4(cr, s0, 16);
    s1 = sha256_neon_schedule_update(s1, s2, s3, s0);
    cr = sha256_neon_round4(cr, s1, 20);
    s2 = sha256_neon_schedule_update(s2, s3, s0, s1);
    cr = sha256_neon_round4(cr, s2, 24);
    s3 = sha256_neon_schedule_update(s3, s0, s1, s2);
    cr = sha256_neon_round4(cr, s3, 28);
    s0 = sha256_neon_schedule_update(s0, s1, s2, s3);
    cr = sha256_neon_round4(cr, s0, 32);
    s1 = sha256_neon_schedule_update(s1, s2, s3, s0);
    cr = sha256_neon_round4(cr, s1, 36);
    s2 = sha256_neon_schedule_update(s2, s3, s0, s1);
    cr = sha256_neon_round4(cr, s2, 40);
    s3 = sha256_neon_schedule_update(s3, s0, s1, s2);
    cr = sha256_neon_round4(cr, s3, 44);
    s0 = sha256_neon_schedule_update(s0, s1, s2, s3);
    cr = sha256_neon_round4(cr, s0, 48);
    s1 = sha256_neon_schedule_update(s1, s2, s3, s0);
    cr = sha256_neon_round4(cr, s1, 52);
    s2 = sha256_neon_schedule_update(s2, s3, s0, s1);
    cr = sha256_neon_round4(cr, s2, 56);
    s3 = sha256_neon_schedule_update(s3, s0, s1, s2);
    cr = sha256_neon_round4(cr, s3, 60);

    core->abcd = vaddq_u32(core->abcd, cr.abcd);
    core->efgh = vaddq_u32(core->efgh, cr.efgh);
}

typedef struct sha256_neon {
    sha256_neon_core core;
    sha256_block blk;
    BinarySink_IMPLEMENTATION;
    ssh_hash hash;
} sha256_neon;

static void sha256_neon_write(BinarySink *bs, const void *vp, size_t len);

static ssh_hash *sha256_neon_new(const ssh_hashalg *alg)
{
    if (!sha256_hw_available_cached())
        return NULL;

    sha256_neon *s = snew(sha256_neon);

    s->hash.vt = alg;
    BinarySink_INIT(s, sha256_neon_write);
    BinarySink_DELEGATE_INIT(&s->hash, s);
    return &s->hash;
}

static void sha256_neon_reset(ssh_hash *hash)
{
    sha256_neon *s = container_of(hash, sha256_neon, hash);

    s->core.abcd = vld1q_u32(sha256_initial_state);
    s->core.efgh = vld1q_u32(sha256_initial_state + 4);

    sha256_block_setup(&s->blk);
}

static void sha256_neon_copyfrom(ssh_hash *hcopy, ssh_hash *horig)
{
    sha256_neon *copy = container_of(hcopy, sha256_neon, hash);
    sha256_neon *orig = container_of(horig, sha256_neon, hash);

    *copy = *orig; /* structure copy */

    BinarySink_COPIED(copy);
    BinarySink_DELEGATE_INIT(&copy->hash, copy);
}

static void sha256_neon_free(ssh_hash *hash)
{
    sha256_neon *s = container_of(hash, sha256_neon, hash);
    smemclr(s, sizeof(*s));
    sfree(s);
}

static void sha256_neon_write(BinarySink *bs, const void *vp, size_t len)
{
    sha256_neon *s = BinarySink_DOWNCAST(bs, sha256_neon);

    while (len > 0)
        if (sha256_block_write(&s->blk, &vp, &len))
            sha256_neon_block(&s->core, s->blk.block);
}

static void sha256_neon_digest(ssh_hash *hash, uint8_t *digest)
{
    sha256_neon *s = container_of(hash, sha256_neon, hash);

    sha256_block_pad(&s->blk, BinarySink_UPCAST(s));
    vst1q_u8(digest,      vrev32q_u8(vreinterpretq_u8_u32(s->core.abcd)));
    vst1q_u8(digest + 16, vrev32q_u8(vreinterpretq_u8_u32(s->core.efgh)));
}

const ssh_hashalg ssh_sha256_hw = {
    ._new = sha256_neon_new,
    .reset = sha256_neon_reset,
    .copyfrom = sha256_neon_copyfrom,
    .digest = sha256_neon_digest,
    .free = sha256_neon_free,
    .hlen = 32,
    .blocklen = 64,
    HASHALG_NAMES_ANNOTATED("SHA-256", "NEON accelerated"),
};

#endif
