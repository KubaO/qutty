/*
 * sshauxcrypt.c: wrapper functions on crypto primitives for use in
 * other contexts than the main SSH packet protocol, such as
 * encrypting private key files and performing XDM-AUTHORIZATION-1.
 *
 * These all work through the standard cipher/hash/MAC APIs, so they
 * don't need to live in the same actual source files as the ciphers
 * they wrap, and I think it keeps things tidier to have them out of
 * the way here instead.
 */

#include "ssh.h"

static ssh_cipher *aes256_pubkey_cipher(const void *key, const void *iv)
{
    /*
     * PuTTY's own .PPK format for SSH-2 private key files is
     * encrypted with 256-bit AES in CBC mode.
     */
    ssh_cipher *cipher = ssh_cipher_new(&ssh_aes256_cbc);
    ssh_cipher_setkey(cipher, key);
    ssh_cipher_setiv(cipher, iv);
    return cipher;
}

void aes256_encrypt_pubkey(const void *key, const void *iv, void *blk, int len)
{
    ssh_cipher *c = aes256_pubkey_cipher(key, iv);
    ssh_cipher_encrypt(c, blk, len);
    ssh_cipher_free(c);
}

void aes256_decrypt_pubkey(const void *key, const void *iv, void *blk, int len)
{
    ssh_cipher *c = aes256_pubkey_cipher(key, iv);
    ssh_cipher_decrypt(c, blk, len);
    ssh_cipher_free(c);
}
