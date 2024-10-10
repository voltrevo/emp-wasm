#ifndef EMP_AES_H
#define EMP_AES_H

#include "block.h"
#include <openssl/evp.h>

namespace emp {

using AES_KEY = EVP_CIPHER_CTX*;  // Use pointer to OpenSSL's EVP_CIPHER_CTX structure

inline void AES_set_encrypt_key(const block userkey, AES_KEY *key) {
    // Allocate and initialize the EVP_CIPHER_CTX
    *key = EVP_CIPHER_CTX_new();
    unsigned char key_bytes[16];
    memcpy(key_bytes, &userkey.low, 8);
    memcpy(key_bytes + 8, &userkey.high, 8);
    // Initialize the context for encryption with the given key
    EVP_EncryptInit_ex(*key, EVP_aes_128_ecb(), NULL, key_bytes, NULL);
    // Disable padding
    EVP_CIPHER_CTX_set_padding(*key, 0);
}

inline void AES_ecb_encrypt_blks(block *blks, unsigned int nblks, const AES_KEY key) {
    unsigned char *data = reinterpret_cast<unsigned char*>(blks);
    int outlen1, outlen2;
    int total_len = nblks * 16;  // Each block is 16 bytes

    // Reinitialize the context
    EVP_EncryptInit_ex(key, NULL, NULL, NULL, NULL);

    // Encrypt data in-place
    EVP_EncryptUpdate(key, data, &outlen1, data, total_len);

    // Finalize encryption
    EVP_EncryptFinal_ex(key, data + outlen1, &outlen2);

    // Verify that the total output length matches the input length
    assert(outlen1 + outlen2 == total_len);
}

// Templated function for encrypting a fixed number of blocks
template<int N>
inline void AES_ecb_encrypt_blks(block *blks, const AES_KEY key) {
    AES_ecb_encrypt_blks(blks, N, key);
}

// Function to free the AES key context
inline void AES_KEY_free(AES_KEY key) {
    EVP_CIPHER_CTX_free(key);
}

} // namespace emp

#endif // EMP_AES_H
