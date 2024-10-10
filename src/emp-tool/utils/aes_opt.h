#ifndef EMP_AES_OPT_KS_H
#define EMP_AES_OPT_KS_H

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
        // Allocate and initialize the EVP_CIPHER_CTX
        keys[i] = EVP_CIPHER_CTX_new();
        // Initialize the context for encryption with the given key
        EVP_EncryptInit_ex(keys[i], EVP_aes_128_ecb(), NULL, key_bytes, NULL);
        // Disable padding
        EVP_CIPHER_CTX_set_padding(keys[i], 0);
    }
}

/*
 * With numKeys keys, use each key to encrypt numEncs blocks.
 */
template<int numKeys, int numEncs>
static inline void ParaEnc(block *blks, AES_KEY *keys) {
    for(int i = 0; i < numKeys; ++i) {
        unsigned char *data = reinterpret_cast<unsigned char*>(blks + i * numEncs);
        int outlen1, outlen2;
        int total_len = numEncs * 16;  // Each block is 16 bytes

        // Reinitialize the context
        EVP_EncryptInit_ex(keys[i], NULL, NULL, NULL, NULL);

        // Encrypt data in-place
        EVP_EncryptUpdate(keys[i], data, &outlen1, data, total_len);

        // Finalize encryption
        EVP_EncryptFinal_ex(keys[i], data + outlen1, &outlen2);

        // Verify that the total output length matches the input length
        assert(outlen1 + outlen2 == total_len);
    }
}

// Function to free the AES key contexts
template<int NumKeys>
static inline void AES_opt_key_free(AES_KEY *keys) {
    for(int i = 0; i < NumKeys; ++i) {
        EVP_CIPHER_CTX_free(keys[i]);
    }
}

} // namespace emp

#endif // EMP_AES_OPT_KS_H
