/*
 * Definitions likely to be helpful to multiple AES implementations.
 */

/*
 * Start by deciding whether we can support hardware AES at all.
 */
#define HW_AES_NONE 0
#define HW_AES_NI 1
#define HW_AES_NEON 2

#ifdef _FORCE_AES_NI
#   define HW_AES HW_AES_NI
#elif defined(__clang__)
#   if __has_attribute(target) && __has_include(<wmmintrin.h>) &&       \
    (defined(__x86_64__) || defined(__i386))
#       define HW_AES HW_AES_NI
#   endif
#elif defined(__GNUC__)
#    if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)) && \
    (defined(__x86_64__) || defined(__i386))
#       define HW_AES HW_AES_NI
#    endif
#elif defined (_MSC_VER)
#   if (defined(_M_X64) || defined(_M_IX86)) && _MSC_FULL_VER >= 150030729
#      define HW_AES HW_AES_NI
#   endif
#endif

#ifdef _FORCE_AES_NEON
#   define HW_AES HW_AES_NEON
#elif defined __BYTE_ORDER__ && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    /* Arm can potentially support both endiannesses, but this code
     * hasn't been tested on anything but little. If anyone wants to
     * run big-endian, they'll need to fix it first. */
#elif defined __ARM_FEATURE_CRYPTO
    /* If the Arm crypto extension is available already, we can
     * support NEON AES without having to enable anything by hand */
#   define HW_AES HW_AES_NEON
#elif defined(__clang__)
#   if __has_attribute(target) && __has_include(<arm_neon.h>) &&       \
    (defined(__aarch64__))
        /* clang can enable the crypto extension in AArch64 using
         * __attribute__((target)) */
#       define HW_AES HW_AES_NEON
#       define USE_CLANG_ATTR_TARGET_AARCH64
#   endif
#elif defined _MSC_VER
#   if defined _M_ARM64
#       define HW_AES HW_AES_NEON
        /* 64-bit Visual Studio uses the header <arm64_neon.h> in place
         * of the standard <arm_neon.h> */
#       define USE_ARM64_NEON_H
#   elif defined _M_ARM
#       define HW_AES HW_AES_NEON
        /* 32-bit Visual Studio uses the right header name, but requires
         * this #define to enable a set of intrinsic definitions that
         * do not omit one of the parameters for vaes[ed]q_u8 */
#       define _ARM_USE_NEW_NEON_INTRINSICS
#   endif
#endif

#if defined _FORCE_SOFTWARE_AES || !defined HW_AES
#   undef HW_AES
#   define HW_AES HW_AES_NONE
#endif

#if HW_AES == HW_AES_NI
#define HW_NAME_SUFFIX " (AES-NI accelerated)"
#elif HW_AES == HW_AES_NEON
#define HW_NAME_SUFFIX " (NEON accelerated)"
#else
#define HW_NAME_SUFFIX " (!NONEXISTENT ACCELERATED VERSION!)"
#endif

/*
 * Vtable collection for AES. For each SSH-level cipher id (i.e.
 * combination of key length and cipher mode), we provide three
 * vtables: one for the pure software implementation, one using
 * hardware acceleration (if available), and a top-level one which is
 * never actually instantiated, and only contains a new() method whose
 * job is to decide which of the other two to return an actual
 * instance of.
 */

ssh_cipher *aes_select(const ssh_cipheralg *alg);
ssh_cipher *aes_sw_new(const ssh_cipheralg *alg);
void aes_sw_free(ssh_cipher *);
void aes_sw_setiv_cbc(ssh_cipher *, const void *iv);
void aes_sw_setiv_sdctr(ssh_cipher *, const void *iv);
void aes_sw_setkey(ssh_cipher *, const void *key);
ssh_cipher *aes_hw_new(const ssh_cipheralg *alg);
void aes_hw_free(ssh_cipher *);
void aes_hw_setiv_cbc(ssh_cipher *, const void *iv);
void aes_hw_setiv_sdctr(ssh_cipher *, const void *iv);
void aes_hw_setkey(ssh_cipher *, const void *key);

struct aes_extra {
    const ssh_cipheralg *sw, *hw;
};

#define VTABLES_INNER(cid, pid, bits, name, encsuffix,                  \
                      decsuffix, setivsuffix, flagsval)                 \
    void cid##_sw##encsuffix(ssh_cipher *, void *blk, int len);         \
    void cid##_sw##decsuffix(ssh_cipher *, void *blk, int len);         \
    const ssh_cipheralg ssh_##cid##_sw = {                              \
        ._new = aes_sw_new,                                             \
        .free = aes_sw_free,                                            \
        .setiv = aes_sw_##setivsuffix,                                  \
        .setkey = aes_sw_setkey,                                        \
        .encrypt = cid##_sw##encsuffix,                                 \
        .decrypt = cid##_sw##decsuffix,                                 \
        .ssh2_id = pid,                                                 \
        .blksize = 16,                                                  \
        .real_keybits = bits,                                           \
        .padded_keybytes = bits/8,                                      \
        .flags = flagsval,                                              \
        .text_name = name " (unaccelerated)",                           \
    };                                                                  \
                                                                        \
    void cid##_hw##encsuffix(ssh_cipher *, void *blk, int len);         \
    void cid##_hw##decsuffix(ssh_cipher *, void *blk, int len);         \
    const ssh_cipheralg ssh_##cid##_hw = {                              \
        ._new = aes_hw_new,                                             \
        .free = aes_hw_free,                                            \
        .setiv = aes_hw_##setivsuffix,                                  \
        .setkey = aes_hw_setkey,                                        \
        .encrypt = cid##_hw##encsuffix,                                 \
        .decrypt = cid##_hw##decsuffix,                                 \
        .ssh2_id = pid,                                                 \
        .blksize = 16,                                                  \
        .real_keybits = bits,                                           \
        .padded_keybytes = bits/8,                                      \
        .flags = flagsval,                                              \
        .text_name = name HW_NAME_SUFFIX,                               \
    };                                                                  \
                                                                        \
    const struct aes_extra extra_##cid = {                              \
        &ssh_##cid##_sw, &ssh_##cid##_hw };                             \
                                                                        \
    const ssh_cipheralg ssh_##cid = {                                   \
        ._new = aes_select,                                             \
        .ssh2_id = pid,                                                 \
        .blksize = 16,                                                  \
        .real_keybits = bits,                                           \
        .padded_keybytes = bits/8,                                      \
        .flags = flagsval,                                              \
        .text_name = name " (dummy selector vtable)",                   \
        .extra = &extra_##cid                                           \
    };                                                                  \

#define VTABLES(keylen)                                                 \
    VTABLES_INNER(aes ## keylen ## _cbc, "aes" #keylen "-cbc",          \
                  keylen, "AES-" #keylen " CBC", _encrypt, _decrypt,    \
                  setiv_cbc, SSH_CIPHER_IS_CBC)                         \
    VTABLES_INNER(aes ## keylen ## _sdctr, "aes" #keylen "-ctr",        \
                  keylen, "AES-" #keylen " SDCTR",,, setiv_sdctr, 0)
   
#define REP2(x) x x
#define REP4(x) REP2(REP2(x))
#define REP8(x) REP2(REP4(x))
#define REP9(x) REP8(x) x
#define REP11(x) REP8(x) REP2(x) x
#define REP13(x) REP8(x) REP4(x) x

/*
 * The round constants used in key schedule expansion.
 */
extern const uint8_t key_setup_round_constants[10];

/*
 * The largest number of round keys ever needed.
 */
#define MAXROUNDKEYS 15
