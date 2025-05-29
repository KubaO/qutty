/* ----------------------------------------------------------------------
 * Stub functions if we have no hardware-accelerated SHA-1. In this
 * case, sha1_hw_new returns NULL (though it should also never be
 * selected by sha1_select, so the only thing that should even be
 * _able_ to call it is testcrypt). As a result, the remaining vtable
 * functions should never be called at all.
 */

#include "ssh.h"
#include "sha1.h"

#if HW_SHA1 == HW_SHA1_NONE

static bool sha1_hw_available(void)
{
    return false;
}

static ssh_hash *sha1_stub_new(const ssh_hashalg *alg)
{
    return NULL;
}

#define STUB_BODY { unreachable("Should never be called"); }

static void sha1_stub_reset(ssh_hash *hash) STUB_BODY
static void sha1_stub_copyfrom(ssh_hash *hash, ssh_hash *orig) STUB_BODY
static void sha1_stub_free(ssh_hash *hash) STUB_BODY
static void sha1_stub_digest(ssh_hash *hash, uint8_t *digest) STUB_BODY

const ssh_hashalg ssh_sha1_hw = {
    ._new = sha1_stub_new,
    .reset = sha1_stub_reset,
    .copyfrom = sha1_stub_copyfrom,
    .digest = sha1_stub_digest,
    .free = sha1_stub_free,
    .hlen = 20,
    .blocklen = 64,
    HASHALG_NAMES_ANNOTATED("SHA-1", "!NONEXISTENT ACCELERATED VERSION!"),
};

#endif /* HW_SHA1 */
