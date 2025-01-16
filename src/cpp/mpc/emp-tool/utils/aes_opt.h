#ifndef EMP_AES_OPT_KS_H
#define EMP_AES_OPT_KS_H

#include "emp-tool/utils/utils.h"
#include "aes.h"  // Updated to include our standard AES functions without SIMD

namespace emp {

/*
 * AES key scheduling for multiple keys
 */
template<int NumKeys>
static inline void AES_opt_key_schedule(block* user_keys, AES_KEY *keys) {
    for(int i = 0; i < NumKeys; ++i) {
        unsigned char key_bytes[16];
        memcpy(key_bytes, &user_keys[i].low, 8);
        memcpy(key_bytes + 8, &user_keys[i].high, 8);
        // Initialize the mbed TLS cipher context
        mbedtls_cipher_init(&keys[i]);
        const mbedtls_cipher_info_t *cipher_info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);
        mbedtls_cipher_setup(&keys[i], cipher_info);
        mbedtls_cipher_setkey(&keys[i], key_bytes, 128, MBEDTLS_ENCRYPT);
        mbedtls_cipher_set_padding_mode(&keys[i], MBEDTLS_PADDING_NONE);
    }
}

/*
 * With numKeys keys, use each key to encrypt numEncs blocks.
 */
template<int numKeys, int numEncs>
static inline void ParaEnc(block *blks, AES_KEY *keys) {
    for(int i = 0; i < numKeys; ++i) {
        unsigned char *data = reinterpret_cast<unsigned char*>(blks + i * numEncs);
        size_t outlen1 = 0, outlen2 = 0;
        size_t total_len = numEncs * 16;  // Each block is 16 bytes
        unsigned char *output = new unsigned char[total_len + 16];  // Allocate output buffer

        // Reinitialize the context
        mbedtls_cipher_reset(&keys[i]);

        // Encrypt data
        int ret = mbedtls_cipher_update(&keys[i], data, total_len, output, &outlen1);
        if (ret != 0) {
            error("Error in ParaEnc");
        }

        // Finalize encryption
        ret = mbedtls_cipher_finish(&keys[i], output + outlen1, &outlen2);
        if (ret != 0) {
            error("Error in ParaEnc");
        }

        // Verify that the total output length matches the input length
        assert(outlen1 + outlen2 == total_len);

        // Copy output back to data
        memcpy(data, output, total_len);
        delete[] output;  // Free allocated memory
    }
}

// Function to free the AES key contexts
template<int NumKeys>
static inline void AES_opt_key_free(AES_KEY *keys) {
    for(int i = 0; i < NumKeys; ++i) {
        mbedtls_cipher_free(&keys[i]);
    }
}

} // namespace emp

#endif // EMP_AES_OPT_KS_H
