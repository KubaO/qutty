#include "putty.h"
#include "ssh.h"
#include "sha512.h"

/*
 * The top-level selection function, caching the results of
 * sha512_hw_available() so it only has to run once.
 */
bool sha512_hw_available_cached(void)
{
    static bool initialised = false;
    static bool hw_available;
    if (!initialised) {
        hw_available = sha512_hw_available();
        initialised = true;
    }
    return hw_available;
}

struct sha512_select_options {
    const ssh_hashalg *hw, *sw;
};

static ssh_hash *sha512_select(const ssh_hashalg *alg)
{
    const struct sha512_select_options *options =
        (const struct sha512_select_options *)alg->extra;

    const ssh_hashalg *real_alg =
        sha512_hw_available_cached() ? options->hw : options->sw;

    return ssh_hash_new(real_alg);
}

const struct sha512_select_options ssh_sha512_select_options = {
    &ssh_sha512_hw, &ssh_sha512_sw,
};
const struct sha512_select_options ssh_sha384_select_options = {
    &ssh_sha384_hw, &ssh_sha384_sw,
};

const ssh_hashalg ssh_sha512 = {
    ._new = sha512_select,
    .hlen = 64,
    .blocklen = 128,
    HASHALG_NAMES_ANNOTATED("SHA-512", "dummy selector vtable"),
    .extra = &ssh_sha512_select_options,
};

const ssh_hashalg ssh_sha384 = {
    ._new = sha512_select,
    .hlen = 48,
    .blocklen = 128,
    HASHALG_NAMES_ANNOTATED("SHA-384", "dummy selector vtable"),
    .extra = &ssh_sha384_select_options,
};
