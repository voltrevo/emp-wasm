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
            // round down to a whole number of blocks
            size = sizeof(block) * (size / sizeof(block));
            int chopped = nbytes - size;

            // temporarily fill the bulk of the buffer with random data
            random_data(aligned_data, nbytes - chopped);

            // move the random data to the start of the buffer
            // (using memmove, not memcpy, because of memory overlap)
            memmove(data, aligned_data, nbytes - chopped);

            int remaining_bytes = chopped;
            char* end = (char*)data + nbytes;

            // there can be 0-2 blocks leftover
            // eg: <- 8 bytes -> <- N blocks -> <- 15 bytes ->
            //     in the above case, the N blocks are filled by random_data,
            //     leaving 23 bytes leftover, which requires 2 blocks to fill,
            //     of which we must ignore 9 bytes to avoid writing past the
            //     end of the buffer (`void *data`).
            while (remaining_bytes > 0) {
                block tmp;
                random_block(&tmp, 1);
                int bytes_to_copy = std::min(remaining_bytes, (int)sizeof(block));
                memcpy(end - remaining_bytes, &tmp, bytes_to_copy);
                remaining_bytes -= bytes_to_copy;
            }
        } else {
            block tmp[2];
            random_block(tmp, 2);
            memcpy(data, tmp, nbytes);
        }
    }

    void random_block(block * data, int nblocks=1) {
        for(int i = 0; i < nblocks; ++i) {
            block blk = makeBlock(0LL, counter++);
            AES_ecb_encrypt_blks(&blk, 1, &aes);
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
