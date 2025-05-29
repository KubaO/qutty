#include <assert.h>
#include <stdlib.h>

#include "putty.h"
#include "ssh.h"
#include "sha1.h"

static ssh_hash *sha1_select(const ssh_hashalg *alg)
{
    const ssh_hashalg *real_alg =
        sha1_hw_available_cached() ? &ssh_sha1_hw : &ssh_sha1_sw;

    return ssh_hash_new(real_alg);
}

const ssh_hashalg ssh_sha1 = {
    ._new = sha1_select,
    .hlen = 20,
    .blocklen = 64,
    HASHALG_NAMES_ANNOTATED("SHA-1", "dummy selector vtable"),
};
