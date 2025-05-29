/*
 * sshaes.c - implementation of AES
 */

#include <assert.h>
#include <stdlib.h>

#include "ssh.h"
#include "mpint_i.h" /* we reuse the BignumInt system */
#include "aes.h"

/* ----------------------------------------------------------------------
 * Stub functions if we have no hardware-accelerated AES. In this
 * case, aes_hw_new returns NULL (though it should also never be
 * selected by aes_select, so the only thing that should even be
 * _able_ to call it is testcrypt). As a result, the remaining vtable
 * functions should never be called at all.
 */

#if HW_AES == HW_AES_NONE

bool aes_hw_available(void)
{
    return false;
}

static ssh_cipher *aes_hw_new(const ssh_cipheralg *alg)
{
    return NULL;
}

#define STUB_BODY { unreachable("Should never be called"); }

static void aes_hw_free(ssh_cipher *ciph) STUB_BODY
static void aes_hw_setkey(ssh_cipher *ciph, const void *key) STUB_BODY
static void aes_hw_setiv_cbc(ssh_cipher *ciph, const void *iv) STUB_BODY
static void aes_hw_setiv_sdctr(ssh_cipher *ciph, const void *iv) STUB_BODY
#define STUB_ENC_DEC(len)                                       \
    static void aes##len##_cbc_hw_encrypt(                      \
        ssh_cipher *ciph, void *vblk, int blklen) STUB_BODY     \
    static void aes##len##_cbc_hw_decrypt(                      \
        ssh_cipher *ciph, void *vblk, int blklen) STUB_BODY     \
    static void aes##len##_sdctr_hw(                            \
        ssh_cipher *ciph, void *vblk, int blklen) STUB_BODY

STUB_ENC_DEC(128)
STUB_ENC_DEC(192)
STUB_ENC_DEC(256)

#endif /* HW_AES */
