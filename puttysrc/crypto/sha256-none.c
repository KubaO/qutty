/* ----------------------------------------------------------------------
 * Stub functions if we have no hardware-accelerated SHA-256. In this
 * case, sha256_hw_new returns NULL (though it should also never be
 * selected by sha256_select, so the only thing that should even be
 * _able_ to call it is testcrypt). As a result, the remaining vtable
 * functions should never be called at all.
 */

#include "ssh.h"
#include "sha256.h"
 
#if HW_SHA256 == HW_SHA256_NONE

bool sha256_hw_available(void)
{
    return false;
}

static ssh_hash *sha256_stub_new(const ssh_hashalg *alg)
{
    return NULL;
}

#define STUB_BODY { unreachable("Should never be called"); }

static void sha256_stub_reset(ssh_hash *hash) STUB_BODY
static void sha256_stub_copyfrom(ssh_hash *hash, ssh_hash *orig) STUB_BODY
static void sha256_stub_free(ssh_hash *hash) STUB_BODY
static void sha256_stub_digest(ssh_hash *hash, uint8_t *digest) STUB_BODY

const ssh_hashalg ssh_sha256_hw = {
    ._new = sha256_stub_new,
    .reset = sha256_stub_reset,
    .copyfrom = sha256_stub_copyfrom,
    .digest = sha256_stub_digest,
    .free = sha256_stub_free,
    .hlen = 32,
    .blocklen = 64,
    HASHALG_NAMES_ANNOTATED("SHA-256", "!NONEXISTENT ACCELERATED VERSION!"),
};

#endif /* HW_SHA256 */
