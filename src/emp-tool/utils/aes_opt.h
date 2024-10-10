#ifndef EMP_AES_OPT_KS_H
#define EMP_AES_OPT_KS_H

#include "aes.h"  // Updated to include our standard AES functions without SIMD

namespace emp {

/*
 * AES key scheduling for multiple keys
 * [REF] Implementation of "Fast Garbling of Circuits Under Standard Assumptions"
 * https://eprint.iacr.org/2015/751.pdf
 */
template<int NumKeys>
static inline void AES_opt_key_schedule(block* user_keys, AES_KEY *keys) {
    for(int i = 0; i < NumKeys; ++i) {
        unsigned char key_bytes[16];
        memcpy(key_bytes, &user_keys[i].low, 8);
        memcpy(key_bytes + 8, &user_keys[i].high, 8);
        AES_set_encrypt_key(key_bytes, 128, &keys[i]);
    }
}

/*
 * With numKeys keys, use each key to encrypt numEncs blocks.
 */
template<int numKeys, int numEncs>
static inline void ParaEnc(block *blks, AES_KEY *keys) {
    for(int i = 0; i < numKeys; ++i) {
        for(int j = 0; j < numEncs; ++j) {
            unsigned char in[16], out[16];
            // Copy block data into input buffer
            memcpy(in, &blks[i * numEncs + j].low, 8);
            memcpy(in + 8, &blks[i * numEncs + j].high, 8);

            // Encrypt using OpenSSL's AES_encrypt function
            AES_encrypt(in, out, &keys[i]);

            // Copy encrypted data back into block
            memcpy(&blks[i * numEncs + j].low, out, 8);
            memcpy(&blks[i * numEncs + j].high, out + 8, 8);
        }
    }
}

} // namespace emp

#endif // EMP_AES_OPT_KS_H
