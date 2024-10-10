#ifndef EMP_PRG_H
#define EMP_PRG_H
#include "block.h"
#include "aes.h"
#include <random>

namespace emp {

class PRG {
public:
    uint64_t counter = 0;
    AES_KEY aes;
    block key;

    PRG(const void * seed = nullptr, int id = 0) {
        if (seed != nullptr) {
            reseed((const block *)seed, id);
        } else {
            block v;
            std::random_device rand_div;
            uint32_t * data = (uint32_t *)(&v);
            for (size_t i = 0; i < sizeof(block) / sizeof(uint32_t); ++i)
                data[i] = rand_div();
            reseed(&v, id);
        }
    }

    void reseed(const block* seed, uint64_t id = 0) {
        block v = *seed;
        v.low ^= id;
        key = v;
        AES_set_encrypt_key(v, &aes);
        counter = 0;
    }

    void random_data(void *data, int nbytes) {
        random_block((block *)data, nbytes/16);
        if (nbytes % 16 != 0) {
            block extra;
            random_block(&extra, 1);
            memcpy((nbytes/16*16)+(char *) data, &extra, nbytes%16);
        }
    }

    void random_bool(bool * data, int length) {
        uint8_t * uint_data = (uint8_t*)data;
        random_data_unaligned(uint_data, length);
        for(int i = 0; i < length; ++i)
            data[i] = uint_data[i] & 1;
    }

    void random_data_unaligned(void *data, int nbytes) {
        size_t size = nbytes;
        void *aligned_data = data;
        if(std::align(sizeof(block), sizeof(block), aligned_data, size)) {
            int chopped = nbytes - size;
            random_data(aligned_data, nbytes - chopped);
            block tmp[1];
            random_block(tmp, 1);
            memcpy(data, tmp, chopped);
        } else {
            block tmp[2];
            random_block(tmp, 2);
            memcpy(data, tmp, nbytes);
        }
    }

    void random_block(block * data, int nblocks=1) {
        for(int i = 0; i < nblocks; ++i) {
            block blk = makeBlock(0LL, counter++);
            AES_ecb_encrypt_blks(&blk, 1, aes);
            data[i] = blk;
        }
    }

    typedef uint64_t result_type;
    result_type buffer[32];
    size_t ptr = 32;
    static constexpr result_type min() {
        return 0;
    }
    static constexpr result_type max() {
        return 0xFFFFFFFFFFFFFFFFULL;
    }
    result_type operator()() {
        if(ptr == 32) {
            random_block((block*)buffer, 16);
            ptr = 0;
        }
        return buffer[ptr++];
    }
};

} // namespace emp
#endif // EMP_PRG_H
