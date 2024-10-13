#ifndef EMP_UTIL_BLOCK_H
#define EMP_UTIL_BLOCK_H

#include <assert.h>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <cstdint>

namespace emp {

struct block {
    uint64_t low;
    uint64_t high;

    block() = default;
    block(uint64_t high_, uint64_t low_) : low(low_), high(high_) {}

    block& operator^=(const block& rhs) {
        low ^= rhs.low;
        high ^= rhs.high;
        return *this;
    }

    block operator^(const block& rhs) const {
        return block(high ^ rhs.high, low ^ rhs.low);
    }

    block operator&(const block& rhs) const {
        return block(high & rhs.high, low & rhs.low);
    }

    block operator|(const block& rhs) const {
        return block(high | rhs.high, low | rhs.low);
    }
};

inline bool getLSB(const block & x) {
    return (x.low & 1) == 1;
}

inline block makeBlock(uint64_t high, uint64_t low) {
    return block(high, low);
}

/* Linear orthomorphism function */
inline block sigma(const block& a) {
    // Extract 32-bit words from a
    uint32_t a0 = static_cast<uint32_t>(a.low & 0xFFFFFFFF);
    uint32_t a1 = static_cast<uint32_t>((a.low >> 32) & 0xFFFFFFFF);
    uint32_t a2 = static_cast<uint32_t>(a.high & 0xFFFFFFFF);
    uint32_t a3 = static_cast<uint32_t>((a.high >> 32) & 0xFFFFFFFF);

    // Perform the shuffle (equivalent to _mm_shuffle_epi32 with shuffle constant 78)
    uint32_t s0 = a2;  // Output lane 0 = input lane 2
    uint32_t s1 = a3;  // Output lane 1 = input lane 3
    uint32_t s2 = a0;  // Output lane 2 = input lane 0
    uint32_t s3 = a1;  // Output lane 3 = input lane 1

    // Reconstruct shuffled block
    uint64_t shuffled_low = ((uint64_t)s0) | ((uint64_t)s1 << 32);
    uint64_t shuffled_high = ((uint64_t)s2) | ((uint64_t)s3 << 32);

    // Compute a & makeBlock(0xFFFFFFFFFFFFFFFF, 0x00)
    block masked = makeBlock(a.high, 0);

    // Compute result = shuffled ^ masked
    block result;
    result.low = shuffled_low ^ masked.low;
    result.high = shuffled_high ^ masked.high;

    return result;
}

const block zero_block = makeBlock(0, 0);
const block all_one_block = makeBlock(0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL);
const block select_mask[2] = {zero_block, all_one_block};

inline block set_bit(const block & a, int i) {
    if(i < 64)
        return makeBlock(a.high, a.low | (1ULL << i));
    else
        return makeBlock(a.high | (1ULL << (i - 64)), a.low);
}

inline std::ostream& operator<<(std::ostream& out, const block& blk) {
    out << std::hex;

    out << std::setw(16) << std::setfill('0') << blk.high <<" "
        << std::setw(16) << std::setfill('0') << blk.low;

    out << std::dec << std::setw(0);
    return out;
}

inline void xorBlocks_arr(block* res, const block* x, const block* y, int nblocks) {
    for (int i = 0; i < nblocks; ++i) {
        res[i].low = x[i].low ^ y[i].low;
        res[i].high = x[i].high ^ y[i].high;
    }
}

inline void xorBlocks_arr(block* res, const block* x, block y, int nblocks) {
    for (int i = 0; i < nblocks; ++i) {
        res[i].low = x[i].low ^ y.low;
        res[i].high = x[i].high ^ y.high;
    }
}

inline bool cmpBlock(const block * x, const block * y, int nblocks) {
    for (int i = 0; i < nblocks; ++i) {
        if (x[i].low != y[i].low || x[i].high != y[i].high)
            return false;
    }
    return true;
}

// Transpose function without SIMD, renamed back to sse_trans for compatibility
inline void sse_trans(uint8_t *out, const uint8_t *inp, uint64_t nrows, uint64_t ncols) {
    assert(nrows % 8 == 0 && ncols % 8 == 0);

    uint64_t bytes_per_row = ncols / 8;
    uint64_t bytes_per_col = nrows / 8;

    for (uint64_t rr = 0; rr < nrows; rr += 8) {
        for (uint64_t cc = 0; cc < ncols; cc += 8) {
            uint8_t block[8];
            for (int i = 0; i < 8; ++i) {
                block[i] = inp[(rr + i)*bytes_per_row + (cc / 8)];
            }
            uint8_t transposed[8];
            for (int i = 0; i < 8; ++i) {
                transposed[i] = 0;
                for (int j = 0; j < 8; ++j) {
                    transposed[i] |= ((block[j] >> i) & 1) << j;
                }
            }
            for (int i = 0; i < 8; ++i) {
                out[(cc + i)*bytes_per_col + (rr / 8)] = transposed[i];
            }
        }
    }
}

} // namespace emp

#endif // EMP_UTIL_BLOCK_H
