#ifndef EMP_HASH_H
#define EMP_HASH_H

#include "block.h"
#include <openssl/evp.h>
#include <stdio.h>

namespace emp {
class Hash {
public:
    EVP_MD_CTX *mdctx;
    char buffer[HASH_BUFFER_SIZE];
    int size = 0;
    static const int DIGEST_SIZE = 32;

    Hash() {
        mdctx = EVP_MD_CTX_create();
        EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    }

    ~Hash() {
        EVP_MD_CTX_destroy(mdctx);
    }

    void put(const void * data, int nbyte) {
        if (nbyte >= HASH_BUFFER_SIZE)
            EVP_DigestUpdate(mdctx, data, nbyte);
        else if(size + nbyte < HASH_BUFFER_SIZE) {
            memcpy(buffer+size, data, nbyte);
            size+=nbyte;
        } else {
            EVP_DigestUpdate(mdctx, buffer, size);
            memcpy(buffer, data, nbyte);
            size = nbyte;
        }
    }

    void put_block(const block* blk, int nblock=1){
        put(blk, sizeof(block)*nblock);
    }

    void digest(void * a) {
        if(size > 0) {
            EVP_DigestUpdate(mdctx, buffer, size);
            size=0;
        }
        unsigned int len = 0;
        EVP_DigestFinal_ex(mdctx, (unsigned char *)a, &len);
        reset();
    }

    void reset() {
        EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
        size=0;
    }

    static void hash_once(void * dgst, const void * data, int nbyte) {
        Hash hash;
        hash.put(data, nbyte);
        hash.digest(dgst);
    }

    static block hash_for_block(const void * data, int nbyte) {
        char digest[DIGEST_SIZE];
        hash_once(digest, data, nbyte);
        uint64_t low, high;
        memcpy(&low, digest, sizeof(uint64_t));
        memcpy(&high, digest + sizeof(uint64_t), sizeof(uint64_t));
        return block(high, low);
    }

    static block KDF(Point &in, uint64_t id = 1) {
        size_t len = in.size();
        in.group->resize_scratch(len+8);
        unsigned char * tmp = in.group->scratch;
        in.to_bin(tmp, len);
        memcpy(tmp+len, &id, 8);
        block ret = hash_for_block(tmp, len+8);
        return ret;
    }
};
} // namespace emp
#endif // EMP_HASH_H
