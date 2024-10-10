#ifndef EMP_AES_H
#define EMP_AES_H

#include "emp-tool/utils/utils.h"
#include "block.h"
#include <mbedtls/cipher.h>  // Include mbed TLS cipher headers

namespace emp {

using AES_KEY = mbedtls_cipher_context_t;  // Use mbed TLS cipher context

inline void AES_set_encrypt_key(const block userkey, AES_KEY *key) {
    mbedtls_cipher_init(key);
    const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);
    mbedtls_cipher_setup(key, cipher_info);
    unsigned char key_bytes[16];
    memcpy(key_bytes, &userkey.low, 8);
    memcpy(key_bytes + 8, &userkey.high, 8);
    mbedtls_cipher_setkey(key, key_bytes, 128, MBEDTLS_ENCRYPT);
    mbedtls_cipher_set_padding_mode(key, MBEDTLS_PADDING_NONE);
}

inline void AES_ecb_encrypt_blks(block *blks, unsigned int nblks, AES_KEY *key) {
    unsigned char *data = reinterpret_cast<unsigned char*>(blks);
    size_t outlen1 = 0, outlen2 = 0;
    size_t total_len = nblks * 16;  // Each block is 16 bytes
    unsigned char *output = new unsigned char[total_len + 16];  // Allocate output buffer

    // Reinitialize the context
    mbedtls_cipher_reset(key);

    // Encrypt data
    int ret = mbedtls_cipher_update(key, data, total_len, output, &outlen1);
    if (ret != 0) {
        error("Error in AES_ecb_encrypt_blks");
    }

    // Finalize encryption
    ret = mbedtls_cipher_finish(key, output + outlen1, &outlen2);
    if (ret != 0) {
        error("Error in AES_ecb_encrypt_blks");
    }

    // Verify that the total output length matches the input length
    assert(outlen1 + outlen2 == total_len);

    // Copy output back to data
    memcpy(data, output, total_len);
    delete[] output;  // Free allocated memory
}

// Templated function for encrypting a fixed number of blocks
template<int N>
inline void AES_ecb_encrypt_blks(block *blks, AES_KEY *key) {
    AES_ecb_encrypt_blks(blks, N, key);
}

// Function to free the AES key context
inline void AES_KEY_free(AES_KEY *key) {
    mbedtls_cipher_free(key);
}

} // namespace emp

#endif // EMP_AES_H
