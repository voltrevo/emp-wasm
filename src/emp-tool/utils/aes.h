#ifndef EMP_AES_H
#define EMP_AES_H

#include "block.h"
#include <openssl/aes.h>

namespace emp {

using AES_KEY = AES_KEY;  // Use OpenSSL's AES_KEY structure

inline void AES_set_encrypt_key(const block userkey, AES_KEY *key) {
    unsigned char key_bytes[16];
    memcpy(key_bytes, &userkey.low, 8);
    memcpy(key_bytes + 8, &userkey.high, 8);
    AES_set_encrypt_key(key_bytes, 128, key);
}

inline void AES_ecb_encrypt_blks(block *blks, unsigned int nblks, const AES_KEY *key) {
    unsigned char in[16], out[16];
    for (unsigned int i = 0; i < nblks; ++i) {
        // Copy block data into input buffer
        memcpy(in, &blks[i].low, 8);
        memcpy(in + 8, &blks[i].high, 8);

        // Encrypt using OpenSSL's AES_encrypt function
        AES_encrypt(in, out, key);

        // Copy encrypted data back into block
        memcpy(&blks[i].low, out, 8);
        memcpy(&blks[i].high, out + 8, 8);
    }
}

// Templated function for encrypting a fixed number of blocks
template<int N>
inline void AES_ecb_encrypt_blks(block *blks, const AES_KEY *key) {
    AES_ecb_encrypt_blks(blks, N, key);
}

} // namespace emp

#endif // EMP_AES_H
