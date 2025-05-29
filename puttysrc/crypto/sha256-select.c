/*
 * Top-level vtables to select a SHA-256 implementation.
 */

#include <assert.h>
#include <stdlib.h>

#include "putty.h"
#include "ssh.h"
#include "sha256.h"

/*
 * The top-level selection function, caching the results of
 * sha256_hw_available() so it only has to run once.
 */
bool sha256_hw_available_cached(void)
{
    static bool initialised = false;
    static bool hw_available;
    if (!initialised) {
        hw_available = sha256_hw_available();
        initialised = true;
    }
    return hw_available;
}

static ssh_hash *sha256_select(const ssh_hashalg *alg)
{
    const ssh_hashalg *real_alg =
        sha256_hw_available_cached() ? &ssh_sha256_hw : &ssh_sha256_sw;

    return ssh_hash_new(real_alg);
}

const ssh_hashalg ssh_sha256 = {
    ._new = sha256_select,
    .hlen = 32,
    .blocklen = 64,
    HASHALG_NAMES_ANNOTATED("SHA-256", "dummy selector vtable"),
};
