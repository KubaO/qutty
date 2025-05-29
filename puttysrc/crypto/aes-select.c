#include <assert.h>
#include <stdlib.h>

#include "putty.h"
#include "ssh.h"
#include "aes.h"

VTABLES(128)
VTABLES(192)
VTABLES(256)

static const ssh_cipheralg ssh_rijndael_lysator = {
    /* Same as aes256_cbc, but with a different protocol ID */
    ._new = aes_select,
    .ssh2_id = "rijndael-cbc@lysator.liu.se",
    .blksize = 16,
    .real_keybits = 256,
    .padded_keybytes = 256/8,
    .flags = 0,
    .text_name = "AES-256 CBC (dummy selector vtable)",
    .extra = &extra_aes256_cbc,
};

static const ssh_cipheralg *const aes_list[] = {
    &ssh_aes256_sdctr,
    &ssh_aes256_cbc,
    &ssh_rijndael_lysator,
    &ssh_aes192_sdctr,
    &ssh_aes192_cbc,
    &ssh_aes128_sdctr,
    &ssh_aes128_cbc,
};

const ssh2_ciphers ssh2_aes = { lenof(aes_list), aes_list };

/*
 * The actual query function that asks if hardware acceleration is
 * available.
 */
bool aes_hw_available(void);

/*
 * The top-level selection function, caching the results of
 * aes_hw_available() so it only has to run once.
 */
bool aes_hw_available_cached(void)
{
    static bool initialised = false;
    static bool hw_available;
    if (!initialised) {
        hw_available = aes_hw_available();
        initialised = true;
    }
    return hw_available;
}

ssh_cipher *aes_select(const ssh_cipheralg *alg)
{
    const struct aes_extra *extra = (const struct aes_extra *)alg->extra;
    const ssh_cipheralg *real_alg =
        aes_hw_available_cached() ? extra->hw : extra->sw;

    return ssh_cipher_new(real_alg);
}
