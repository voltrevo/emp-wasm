#ifndef SSE2NEON_H
#define SSE2NEON_H

// This header file provides a simple API translation layer
// between SSE intrinsics to their corresponding Arm/Aarch64 NEON versions
//
// Contributors to this work are:
//   John W. Ratcliff <jratcliffscarab@gmail.com>
//   Brandon Rowlett <browlett@nvidia.com>
//   Ken Fast <kfast@gdeb.com>
//   Eric van Beurden <evanbeurden@nvidia.com>
//   Alexander Potylitsin <apotylitsin@nvidia.com>
//   Hasindu Gamaarachchi <hasindu2008@gmail.com>
//   Jim Huang <jserv@ccns.ncku.edu.tw>
//   Mark Cheng <marktwtn@gmail.com>
//   Malcolm James MacLeod <malcolm@gulden.com>
//   Devin Hussey (easyaspi314) <husseydevin@gmail.com>
//   Sebastian Pop <spop@amazon.com>
//   Developer Ecosystem Engineering <DeveloperEcosystemEngineering@apple.com>
//   Danila Kutenin <danilak@google.com>
//   François Turban (JishinMaster) <francois.turban@gmail.com>
//   Pei-Hsuan Hung <afcidk@gmail.com>
//   Yang-Hao Yuan <yuanyanghau@gmail.com>
//   Syoyo Fujita <syoyo@lighttransport.com>
//   Brecht Van Lommel <brecht@blender.org>
//   Jonathan Hue <jhue@adobe.com>
//   Cuda Chen <clh960524@gmail.com>
//   Aymen Qader <aymen.qader@arm.com>

/*
 * sse2neon is freely redistributable under the MIT License.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Tunable configurations */

/* Enable precise implementation of math operations
 * This would slow down the computation a bit, but gives consistent result with
 * x86 SSE. (e.g. would solve a hole or NaN pixel in the rendering result)
 */
/* _mm_min|max_ps|ss|pd|sd */
#ifndef SSE2NEON_PRECISE_MINMAX
#define SSE2NEON_PRECISE_MINMAX (0)
#endif
/* _mm_rcp_ps and _mm_div_ps */
#ifndef SSE2NEON_PRECISE_DIV
#define SSE2NEON_PRECISE_DIV (0)
#endif
/* _mm_sqrt_ps and _mm_rsqrt_ps */
#ifndef SSE2NEON_PRECISE_SQRT
#define SSE2NEON_PRECISE_SQRT (0)
#endif
/* _mm_dp_pd */
#ifndef SSE2NEON_PRECISE_DP
#define SSE2NEON_PRECISE_DP (0)
#endif

/* compiler specific definitions */
#if defined(__GNUC__) || defined(__clang__)
#pragma push_macro("FORCE_INLINE")
#pragma push_macro("ALIGN_STRUCT")
#define FORCE_INLINE static inline __attribute__((always_inline))
#define ALIGN_STRUCT(x) __attribute__((aligned(x)))
#define _sse2neon_likely(x) __builtin_expect(!!(x), 1)
#define _sse2neon_unlikely(x) __builtin_expect(!!(x), 0)
#else /* non-GNU / non-clang compilers */
#warning "Macro name collisions may happen with unsupported compiler."
#ifndef FORCE_INLINE
#define FORCE_INLINE static inline
#endif
#ifndef ALIGN_STRUCT
#define ALIGN_STRUCT(x) __declspec(align(x))
#endif
#define _sse2neon_likely(x) (x)
#define _sse2neon_unlikely(x) (x)
#endif

/* C language does not allow initializing a variable with a function call. */
#ifdef __cplusplus
#define _sse2neon_const static const
#else
#define _sse2neon_const const
#endif

#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32)
/* Definitions for _mm_{malloc,free} are provided by <malloc.h>
 * from both MinGW-w64 and MSVC.
 */
#define SSE2NEON_ALLOC_DEFINED
#endif

/* If using MSVC */
#ifdef _MSC_VER
#include <intrin.h>
#if (defined(_M_AMD64) || defined(__x86_64__)) || \
    (defined(_M_ARM) || defined(__arm__))
#define SSE2NEON_HAS_BITSCAN64
#endif
#endif

/* Compiler barrier */
#define SSE2NEON_BARRIER()                     \
    do {                                       \
        __asm__ __volatile__("" ::: "memory"); \
        (void) 0;                              \
    } while (0)

/* Memory barriers
 * __atomic_thread_fence does not include a compiler barrier; instead,
 * the barrier is part of __atomic_load/__atomic_store's "volatile-like"
 * semantics.
 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#include <stdatomic.h>
#endif

FORCE_INLINE void _sse2neon_smp_mb(void)
{
    SSE2NEON_BARRIER();
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && \
    !defined(__STDC_NO_ATOMICS__)
    atomic_thread_fence(memory_order_seq_cst);
#elif defined(__GNUC__) || defined(__clang__)
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#else
    /* FIXME: MSVC support */
#endif
}

/* Architecture-specific build options */
/* FIXME: #pragma GCC push_options is only available on GCC */
#if defined(__GNUC__)
#if defined(__arm__) && __ARM_ARCH == 7
/* According to ARM C Language Extensions Architecture specification,
 * __ARM_NEON is defined to a value indicating the Advanced SIMD (NEON)
 * architecture supported.
 */
#if !defined(__ARM_NEON) || !defined(__ARM_NEON__)
#error "You must enable NEON instructions (e.g. -mfpu=neon) to use SSE2NEON."
#endif
#if !defined(__clang__)
#pragma GCC push_options
#pragma GCC target("fpu=neon")
#endif
#elif defined(__aarch64__)
#if !defined(__clang__)
#pragma GCC push_options
#pragma GCC target("+simd")
#endif
#elif __ARM_ARCH == 8
#if !defined(__ARM_NEON) || !defined(__ARM_NEON__)
#error \
    "You must enable NEON instructions (e.g. -mfpu=neon-fp-armv8) to use SSE2NEON."
#endif
#if !defined(__clang__)
#pragma GCC push_options
#endif
#else
#error "Unsupported target. Must be either ARMv7-A+NEON or ARMv8-A."
#endif
#endif

#include <arm_neon.h>
#if !defined(__aarch64__) && (__ARM_ARCH == 8)
#if defined __has_include && __has_include(<arm_acle.h>)
#include <arm_acle.h>
#endif
#endif

/* Apple Silicon cache lines are double of what is commonly used by Intel, AMD
 * and other Arm microarchtectures use.
 * From sysctl -a on Apple M1:
 * hw.cachelinesize: 128
 */
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
#define SSE2NEON_CACHELINE_SIZE 128
#else
#define SSE2NEON_CACHELINE_SIZE 64
#endif

/* Rounding functions require either Aarch64 instructions or libm failback */
#if !defined(__aarch64__)
#include <math.h>
#endif

/* On ARMv7, some registers, such as PMUSERENR and PMCCNTR, are read-only
 * or even not accessible in user mode.
 * To write or access to these registers in user mode,
 * we have to perform syscall instead.
 */
#if !defined(__aarch64__)
#include <sys/time.h>
#endif

/* "__has_builtin" can be used to query support for built-in functions
 * provided by gcc/clang and other compilers that support it.
 */
#ifndef __has_builtin /* GCC prior to 10 or non-clang compilers */
/* Compatibility with gcc <= 9 */
#if defined(__GNUC__) && (__GNUC__ <= 9)
#define __has_builtin(x) HAS##x
#define HAS__builtin_popcount 1
#define HAS__builtin_popcountll 1

// __builtin_shuffle introduced in GCC 4.7.0
#if (__GNUC__ >= 5) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 7))
#define HAS__builtin_shuffle 1
#else
#define HAS__builtin_shuffle 0
#endif

#define HAS__builtin_shufflevector 0
#define HAS__builtin_nontemporal_store 0
#else
#define __has_builtin(x) 0
#endif
#endif

/**
 * MACRO for shuffle parameter for _mm_shuffle_ps().
 * Argument fp3 is a digit[0123] that represents the fp from argument "b"
 * of mm_shuffle_ps that will be placed in fp3 of result. fp2 is the same
 * for fp2 in result. fp1 is a digit[0123] that represents the fp from
 * argument "a" of mm_shuffle_ps that will be places in fp1 of result.
 * fp0 is the same for fp0 of result.
 */
#define _MM_SHUFFLE(fp3, fp2, fp1, fp0) \
    (((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | ((fp0)))

// /* A few intrinsics accept traditional data types like ints or floats, but
//  * most operate on data types that are specific to SSE.
//  * If a vector type ends in d, it contains doubles, and if it does not have
//  * a suffix, it contains floats. An integer vector type can contain any type
//  * of integer, from chars to shorts to unsigned long longs.
//  */
typedef float32x4_t __m128; /* 128-bit vector containing 4 floats */
typedef int64x2_t __m128i; /* 128-bit vector containing integers */

// /* type-safe casting between types */

#define vreinterpretq_m128_f32(x) (x)

#define vreinterpretq_m128_s32(x) vreinterpretq_f32_s32(x)

#define vreinterpretq_f32_m128(x) (x)

#define vreinterpretq_s32_m128(x) vreinterpretq_s32_f32(x)

#define vreinterpretq_m128i_s8(x) vreinterpretq_s64_s8(x)
#define vreinterpretq_m128i_s16(x) vreinterpretq_s64_s16(x)
#define vreinterpretq_m128i_s32(x) vreinterpretq_s64_s32(x)
#define vreinterpretq_m128i_s64(x) (x)

#define vreinterpretq_m128i_u8(x) vreinterpretq_s64_u8(x)
#define vreinterpretq_m128i_u32(x) vreinterpretq_s64_u32(x)

#define vreinterpretq_s8_m128i(x) vreinterpretq_s8_s64(x)
#define vreinterpretq_s32_m128i(x) vreinterpretq_s32_s64(x)
#define vreinterpretq_s64_m128i(x) (x)

#define vreinterpretq_u8_m128i(x) vreinterpretq_u8_s64(x)
#define vreinterpretq_u32_m128i(x) vreinterpretq_u32_s64(x)
#define vreinterpretq_u64_m128i(x) vreinterpretq_u64_s64(x)

// // Function declaration
// // SSE
// // SSE2
FORCE_INLINE __m128i _mm_and_si128(__m128i, __m128i);
FORCE_INLINE __m128i _mm_or_si128(__m128i, __m128i);
FORCE_INLINE __m128i _mm_set_epi32(int, int, int, int);
FORCE_INLINE __m128i _mm_setzero_si128();
// SSE4.1

// Takes the upper 64 bits of a and places it in the low end of the result
// Takes the lower 64 bits of b and places it into the high end of the result.
FORCE_INLINE __m128 _mm_shuffle_ps_1032(__m128 a, __m128 b)
{
    float32x2_t a32 = vget_high_f32(vreinterpretq_f32_m128(a));
    float32x2_t b10 = vget_low_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(vcombine_f32(a32, b10));
}

// takes the lower two 32-bit values from a and swaps them and places in high
// end of result takes the higher two 32 bit values from b and swaps them and
// places in low end of result.
FORCE_INLINE __m128 _mm_shuffle_ps_2301(__m128 a, __m128 b)
{
    float32x2_t a01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(a)));
    float32x2_t b23 = vrev64_f32(vget_high_f32(vreinterpretq_f32_m128(b)));
    return vreinterpretq_m128_f32(vcombine_f32(a01, b23));
}

FORCE_INLINE __m128 _mm_shuffle_ps_0321(__m128 a, __m128 b)
{
    float32x2_t a21 = vget_high_f32(
        vextq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a), 3));
    float32x2_t b03 = vget_low_f32(
        vextq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b), 3));
    return vreinterpretq_m128_f32(vcombine_f32(a21, b03));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2103(__m128 a, __m128 b)
{
    float32x2_t a03 = vget_low_f32(
        vextq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a), 3));
    float32x2_t b21 = vget_high_f32(
        vextq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b), 3));
    return vreinterpretq_m128_f32(vcombine_f32(a03, b21));
}

FORCE_INLINE __m128 _mm_shuffle_ps_1001(__m128 a, __m128 b)
{
    float32x2_t a01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(a)));
    float32x2_t b10 = vget_low_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(vcombine_f32(a01, b10));
}

FORCE_INLINE __m128 _mm_shuffle_ps_0101(__m128 a, __m128 b)
{
    float32x2_t a01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(a)));
    float32x2_t b01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(b)));
    return vreinterpretq_m128_f32(vcombine_f32(a01, b01));
}

// keeps the low 64 bits of b in the low and puts the high 64 bits of a in the
// high
FORCE_INLINE __m128 _mm_shuffle_ps_3210(__m128 a, __m128 b)
{
    float32x2_t a10 = vget_low_f32(vreinterpretq_f32_m128(a));
    float32x2_t b32 = vget_high_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(vcombine_f32(a10, b32));
}

FORCE_INLINE __m128 _mm_shuffle_ps_0011(__m128 a, __m128 b)
{
    float32x2_t a11 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(a)), 1);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    return vreinterpretq_m128_f32(vcombine_f32(a11, b00));
}

FORCE_INLINE __m128 _mm_shuffle_ps_0022(__m128 a, __m128 b)
{
    float32x2_t a22 =
        vdup_lane_f32(vget_high_f32(vreinterpretq_f32_m128(a)), 0);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    return vreinterpretq_m128_f32(vcombine_f32(a22, b00));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2200(__m128 a, __m128 b)
{
    float32x2_t a00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(a)), 0);
    float32x2_t b22 =
        vdup_lane_f32(vget_high_f32(vreinterpretq_f32_m128(b)), 0);
    return vreinterpretq_m128_f32(vcombine_f32(a00, b22));
}

FORCE_INLINE __m128 _mm_shuffle_ps_3202(__m128 a, __m128 b)
{
    float32_t a0 = vgetq_lane_f32(vreinterpretq_f32_m128(a), 0);
    float32x2_t a22 =
        vdup_lane_f32(vget_high_f32(vreinterpretq_f32_m128(a)), 0);
    float32x2_t a02 = vset_lane_f32(a0, a22, 1); /* TODO: use vzip ?*/
    float32x2_t b32 = vget_high_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(vcombine_f32(a02, b32));
}

FORCE_INLINE __m128 _mm_shuffle_ps_1133(__m128 a, __m128 b)
{
    float32x2_t a33 =
        vdup_lane_f32(vget_high_f32(vreinterpretq_f32_m128(a)), 1);
    float32x2_t b11 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 1);
    return vreinterpretq_m128_f32(vcombine_f32(a33, b11));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2010(__m128 a, __m128 b)
{
    float32x2_t a10 = vget_low_f32(vreinterpretq_f32_m128(a));
    float32_t b2 = vgetq_lane_f32(vreinterpretq_f32_m128(b), 2);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    float32x2_t b20 = vset_lane_f32(b2, b00, 1);
    return vreinterpretq_m128_f32(vcombine_f32(a10, b20));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2001(__m128 a, __m128 b)
{
    float32x2_t a01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(a)));
    float32_t b2 = vgetq_lane_f32(b, 2);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    float32x2_t b20 = vset_lane_f32(b2, b00, 1);
    return vreinterpretq_m128_f32(vcombine_f32(a01, b20));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2032(__m128 a, __m128 b)
{
    float32x2_t a32 = vget_high_f32(vreinterpretq_f32_m128(a));
    float32_t b2 = vgetq_lane_f32(b, 2);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    float32x2_t b20 = vset_lane_f32(b2, b00, 1);
    return vreinterpretq_m128_f32(vcombine_f32(a32, b20));
}

// C equivalent:
//   __m128i _mm_shuffle_epi32_default(__m128i a,
//                                     __constrange(0, 255) int imm) {
//       __m128i ret;
//       ret[0] = a[imm        & 0x3];   ret[1] = a[(imm >> 2) & 0x3];
//       ret[2] = a[(imm >> 4) & 0x03];  ret[3] = a[(imm >> 6) & 0x03];
//       return ret;
//   }
#define _mm_shuffle_epi32_default(a, imm)                                   \
    __extension__({                                                         \
        int32x4_t ret;                                                      \
        ret = vmovq_n_s32(                                                  \
            vgetq_lane_s32(vreinterpretq_s32_m128i(a), (imm) & (0x3)));     \
        ret = vsetq_lane_s32(                                               \
            vgetq_lane_s32(vreinterpretq_s32_m128i(a), ((imm) >> 2) & 0x3), \
            ret, 1);                                                        \
        ret = vsetq_lane_s32(                                               \
            vgetq_lane_s32(vreinterpretq_s32_m128i(a), ((imm) >> 4) & 0x3), \
            ret, 2);                                                        \
        ret = vsetq_lane_s32(                                               \
            vgetq_lane_s32(vreinterpretq_s32_m128i(a), ((imm) >> 6) & 0x3), \
            ret, 3);                                                        \
        vreinterpretq_m128i_s32(ret);                                       \
    })

// Takes the upper 64 bits of a and places it in the low end of the result
// Takes the lower 64 bits of a and places it into the high end of the result.
FORCE_INLINE __m128i _mm_shuffle_epi_1032(__m128i a)
{
    int32x2_t a32 = vget_high_s32(vreinterpretq_s32_m128i(a));
    int32x2_t a10 = vget_low_s32(vreinterpretq_s32_m128i(a));
    return vreinterpretq_m128i_s32(vcombine_s32(a32, a10));
}

// takes the lower two 32-bit values from a and swaps them and places in low end
// of result takes the higher two 32 bit values from a and swaps them and places
// in high end of result.
FORCE_INLINE __m128i _mm_shuffle_epi_2301(__m128i a)
{
    int32x2_t a01 = vrev64_s32(vget_low_s32(vreinterpretq_s32_m128i(a)));
    int32x2_t a23 = vrev64_s32(vget_high_s32(vreinterpretq_s32_m128i(a)));
    return vreinterpretq_m128i_s32(vcombine_s32(a01, a23));
}

// rotates the least significant 32 bits into the most significant 32 bits, and
// shifts the rest down
FORCE_INLINE __m128i _mm_shuffle_epi_0321(__m128i a)
{
    return vreinterpretq_m128i_s32(
        vextq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(a), 1));
}

// rotates the most significant 32 bits into the least significant 32 bits, and
// shifts the rest up
FORCE_INLINE __m128i _mm_shuffle_epi_2103(__m128i a)
{
    return vreinterpretq_m128i_s32(
        vextq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(a), 3));
}

// gets the lower 64 bits of a, and places it in the upper 64 bits
// gets the lower 64 bits of a and places it in the lower 64 bits
FORCE_INLINE __m128i _mm_shuffle_epi_1010(__m128i a)
{
    int32x2_t a10 = vget_low_s32(vreinterpretq_s32_m128i(a));
    return vreinterpretq_m128i_s32(vcombine_s32(a10, a10));
}

// gets the lower 64 bits of a, swaps the 0 and 1 elements, and places it in the
// lower 64 bits gets the lower 64 bits of a, and places it in the upper 64 bits
FORCE_INLINE __m128i _mm_shuffle_epi_1001(__m128i a)
{
    int32x2_t a01 = vrev64_s32(vget_low_s32(vreinterpretq_s32_m128i(a)));
    int32x2_t a10 = vget_low_s32(vreinterpretq_s32_m128i(a));
    return vreinterpretq_m128i_s32(vcombine_s32(a01, a10));
}

// gets the lower 64 bits of a, swaps the 0 and 1 elements and places it in the
// upper 64 bits gets the lower 64 bits of a, swaps the 0 and 1 elements, and
// places it in the lower 64 bits
FORCE_INLINE __m128i _mm_shuffle_epi_0101(__m128i a)
{
    int32x2_t a01 = vrev64_s32(vget_low_s32(vreinterpretq_s32_m128i(a)));
    return vreinterpretq_m128i_s32(vcombine_s32(a01, a01));
}

FORCE_INLINE __m128i _mm_shuffle_epi_2211(__m128i a)
{
    int32x2_t a11 = vdup_lane_s32(vget_low_s32(vreinterpretq_s32_m128i(a)), 1);
    int32x2_t a22 = vdup_lane_s32(vget_high_s32(vreinterpretq_s32_m128i(a)), 0);
    return vreinterpretq_m128i_s32(vcombine_s32(a11, a22));
}

FORCE_INLINE __m128i _mm_shuffle_epi_0122(__m128i a)
{
    int32x2_t a22 = vdup_lane_s32(vget_high_s32(vreinterpretq_s32_m128i(a)), 0);
    int32x2_t a01 = vrev64_s32(vget_low_s32(vreinterpretq_s32_m128i(a)));
    return vreinterpretq_m128i_s32(vcombine_s32(a22, a01));
}

FORCE_INLINE __m128i _mm_shuffle_epi_3332(__m128i a)
{
    int32x2_t a32 = vget_high_s32(vreinterpretq_s32_m128i(a));
    int32x2_t a33 = vdup_lane_s32(vget_high_s32(vreinterpretq_s32_m128i(a)), 1);
    return vreinterpretq_m128i_s32(vcombine_s32(a32, a33));
}

// FORCE_INLINE __m128i _mm_shuffle_epi32_splat(__m128i a, __constrange(0,255)
// int imm)
#if defined(__aarch64__)
#define _mm_shuffle_epi32_splat(a, imm)                          \
    __extension__({                                              \
        vreinterpretq_m128i_s32(                                 \
            vdupq_laneq_s32(vreinterpretq_s32_m128i(a), (imm))); \
    })
#else
#define _mm_shuffle_epi32_splat(a, imm)                                      \
    __extension__({                                                          \
        vreinterpretq_m128i_s32(                                             \
            vdupq_n_s32(vgetq_lane_s32(vreinterpretq_s32_m128i(a), (imm)))); \
    })
#endif

// NEON does not support a general purpose permute intrinsic
// Selects four specific single-precision, floating-point values from a and b,
// based on the mask i.
//
// C equivalent:
//   __m128 _mm_shuffle_ps_default(__m128 a, __m128 b,
//                                 __constrange(0, 255) int imm) {
//       __m128 ret;
//       ret[0] = a[imm        & 0x3];   ret[1] = a[(imm >> 2) & 0x3];
//       ret[2] = b[(imm >> 4) & 0x03];  ret[3] = b[(imm >> 6) & 0x03];
//       return ret;
//   }
//
// https://msdn.microsoft.com/en-us/library/vstudio/5f0858x0(v=vs.100).aspx
#define _mm_shuffle_ps_default(a, b, imm)                                  \
    __extension__({                                                        \
        float32x4_t ret;                                                   \
        ret = vmovq_n_f32(                                                 \
            vgetq_lane_f32(vreinterpretq_f32_m128(a), (imm) & (0x3)));     \
        ret = vsetq_lane_f32(                                              \
            vgetq_lane_f32(vreinterpretq_f32_m128(a), ((imm) >> 2) & 0x3), \
            ret, 1);                                                       \
        ret = vsetq_lane_f32(                                              \
            vgetq_lane_f32(vreinterpretq_f32_m128(b), ((imm) >> 4) & 0x3), \
            ret, 2);                                                       \
        ret = vsetq_lane_f32(                                              \
            vgetq_lane_f32(vreinterpretq_f32_m128(b), ((imm) >> 6) & 0x3), \
            ret, 3);                                                       \
        vreinterpretq_m128_f32(ret);                                       \
    })

// Moves the upper two values of B into the lower two values of A.
//
//   r3 := a3
//   r2 := a2
//   r1 := b3
//   r0 := b2
FORCE_INLINE __m128 _mm_movehl_ps(__m128 __A, __m128 __B)
{
    float32x2_t a32 = vget_high_f32(vreinterpretq_f32_m128(__A));
    float32x2_t b32 = vget_high_f32(vreinterpretq_f32_m128(__B));
    return vreinterpretq_m128_f32(vcombine_f32(b32, a32));
}

// Moves the lower two values of B into the upper two values of A.
//
//   r3 := b1
//   r2 := b0
//   r1 := a1
//   r0 := a0
FORCE_INLINE __m128 _mm_movelh_ps(__m128 __A, __m128 __B)
{
    float32x2_t a10 = vget_low_f32(vreinterpretq_f32_m128(__A));
    float32x2_t b10 = vget_low_f32(vreinterpretq_f32_m128(__B));
    return vreinterpretq_m128_f32(vcombine_f32(a10, b10));
}

// FORCE_INLINE __m128 _mm_shuffle_ps(__m128 a, __m128 b, __constrange(0,255)
// int imm)
#ifdef _sse2neon_shuffle
#define _mm_shuffle_ps(a, b, imm)                                              \
    __extension__({                                                            \
        float32x4_t _input1 = vreinterpretq_f32_m128(a);                       \
        float32x4_t _input2 = vreinterpretq_f32_m128(b);                       \
        float32x4_t _shuf =                                                    \
            vshuffleq_s32(_input1, _input2, (imm) & (0x3), ((imm) >> 2) & 0x3, \
                          (((imm) >> 4) & 0x3) + 4, (((imm) >> 6) & 0x3) + 4); \
        vreinterpretq_m128_f32(_shuf);                                         \
    })
#else  // generic
#define _mm_shuffle_ps(a, b, imm)                          \
    __extension__({                                        \
        __m128 ret;                                        \
        switch (imm) {                                     \
        case _MM_SHUFFLE(1, 0, 3, 2):                      \
            ret = _mm_shuffle_ps_1032((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 3, 0, 1):                      \
            ret = _mm_shuffle_ps_2301((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(0, 3, 2, 1):                      \
            ret = _mm_shuffle_ps_0321((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 1, 0, 3):                      \
            ret = _mm_shuffle_ps_2103((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(1, 0, 1, 0):                      \
            ret = _mm_movelh_ps((a), (b));                 \
            break;                                         \
        case _MM_SHUFFLE(1, 0, 0, 1):                      \
            ret = _mm_shuffle_ps_1001((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(0, 1, 0, 1):                      \
            ret = _mm_shuffle_ps_0101((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(3, 2, 1, 0):                      \
            ret = _mm_shuffle_ps_3210((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(0, 0, 1, 1):                      \
            ret = _mm_shuffle_ps_0011((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(0, 0, 2, 2):                      \
            ret = _mm_shuffle_ps_0022((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 2, 0, 0):                      \
            ret = _mm_shuffle_ps_2200((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(3, 2, 0, 2):                      \
            ret = _mm_shuffle_ps_3202((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(3, 2, 3, 2):                      \
            ret = _mm_movehl_ps((b), (a));                 \
            break;                                         \
        case _MM_SHUFFLE(1, 1, 3, 3):                      \
            ret = _mm_shuffle_ps_1133((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 0, 1, 0):                      \
            ret = _mm_shuffle_ps_2010((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 0, 0, 1):                      \
            ret = _mm_shuffle_ps_2001((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 0, 3, 2):                      \
            ret = _mm_shuffle_ps_2032((a), (b));           \
            break;                                         \
        default:                                           \
            ret = _mm_shuffle_ps_default((a), (b), (imm)); \
            break;                                         \
        }                                                  \
        ret;                                               \
    })
#endif

// Computes the bitwise AND of the 128-bit value in a and the 128-bit value in
// b.
//
//   r := a & b
//
// https://msdn.microsoft.com/en-us/library/vstudio/6d1txsa8(v=vs.100).aspx
FORCE_INLINE __m128i _mm_and_si128(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vandq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Computes the bitwise AND of the 128-bit value in b and the bitwise NOT of the
// 128-bit value in a.
//
//   r := (~a) & b
//
// https://msdn.microsoft.com/en-us/library/vstudio/1beaceh8(v=vs.100).aspx
FORCE_INLINE __m128i _mm_andnot_si128(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vbicq_s32(vreinterpretq_s32_m128i(b),
                  vreinterpretq_s32_m128i(a)));  // *NOTE* argument swap
}

// Shift a left by imm8 bytes while shifting in zeros, and store the results in
// dst.
// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_bslli_si128
#define _mm_bslli_si128(a, imm) _mm_slli_si128(a, imm)

// Applies a type cast to reinterpret four 32-bit floating point values passed
// in as a 128-bit parameter as packed 32-bit integers.
// https://msdn.microsoft.com/en-us/library/bb514099.aspx
FORCE_INLINE __m128i _mm_castps_si128(__m128 a)
{
    return vreinterpretq_m128i_s32(vreinterpretq_s32_m128(a));
}

// Applies a type cast to reinterpret four 32-bit integers passed in as a
// 128-bit parameter as packed 32-bit floating point values.
// https://msdn.microsoft.com/en-us/library/bb514029.aspx
FORCE_INLINE __m128 _mm_castsi128_ps(__m128i a)
{
    return vreinterpretq_m128_s32(vreinterpretq_s32_m128i(a));
}

// Copy the lower 32-bit integer in a to dst.
//
//   dst[31:0] := a[31:0]
//
// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_cvtsi128_si32
FORCE_INLINE int _mm_cvtsi128_si32(__m128i a)
{
    return vgetq_lane_s32(vreinterpretq_s32_m128i(a), 0);
}

// Loads 128-bit value. :
// https://msdn.microsoft.com/en-us/library/atzzad1h(v=vs.80).aspx
FORCE_INLINE __m128i _mm_load_si128(const __m128i *p)
{
    return vreinterpretq_m128i_s32(vld1q_s32((const int32_t *) p));
}

// Loads 128-bit value. :
// https://msdn.microsoft.com/zh-cn/library/f4k12ae8(v=vs.90).aspx
FORCE_INLINE __m128i _mm_loadu_si128(const __m128i *p)
{
    return vreinterpretq_m128i_s32(vld1q_s32((const int32_t *) p));
}

// NEON does not provide a version of this function.
// Creates a 16-bit mask from the most significant bits of the 16 signed or
// unsigned 8-bit integers in a and zero extends the upper bits.
// https://msdn.microsoft.com/en-us/library/vstudio/s090c8fk(v=vs.100).aspx
FORCE_INLINE int _mm_movemask_epi8(__m128i a)
{
    // Use increasingly wide shifts+adds to collect the sign bits
    // together.
    // Since the widening shifts would be rather confusing to follow in little
    // endian, everything will be illustrated in big endian order instead. This
    // has a different result - the bits would actually be reversed on a big
    // endian machine.

    // Starting input (only half the elements are shown):
    // 89 ff 1d c0 00 10 99 33
    uint8x16_t input = vreinterpretq_u8_m128i(a);

    // Shift out everything but the sign bits with an unsigned shift right.
    //
    // Bytes of the vector::
    // 89 ff 1d c0 00 10 99 33
    // \  \  \  \  \  \  \  \    high_bits = (uint16x4_t)(input >> 7)
    //  |  |  |  |  |  |  |  |
    // 01 01 00 01 00 00 01 00
    //
    // Bits of first important lane(s):
    // 10001001 (89)
    // \______
    //        |
    // 00000001 (01)
    uint16x8_t high_bits = vreinterpretq_u16_u8(vshrq_n_u8(input, 7));

    // Merge the even lanes together with a 16-bit unsigned shift right + add.
    // 'xx' represents garbage data which will be ignored in the final result.
    // In the important bytes, the add functions like a binary OR.
    //
    // 01 01 00 01 00 00 01 00
    //  \_ |  \_ |  \_ |  \_ |   paired16 = (uint32x4_t)(input + (input >> 7))
    //    \|    \|    \|    \|
    // xx 03 xx 01 xx 00 xx 02
    //
    // 00000001 00000001 (01 01)
    //        \_______ |
    //                \|
    // xxxxxxxx xxxxxx11 (xx 03)
    uint32x4_t paired16 =
        vreinterpretq_u32_u16(vsraq_n_u16(high_bits, high_bits, 7));

    // Repeat with a wider 32-bit shift + add.
    // xx 03 xx 01 xx 00 xx 02
    //     \____ |     \____ |  paired32 = (uint64x1_t)(paired16 + (paired16 >>
    //     14))
    //          \|          \|
    // xx xx xx 0d xx xx xx 02
    //
    // 00000011 00000001 (03 01)
    //        \\_____ ||
    //         '----.\||
    // xxxxxxxx xxxx1101 (xx 0d)
    uint64x2_t paired32 =
        vreinterpretq_u64_u32(vsraq_n_u32(paired16, paired16, 14));

    // Last, an even wider 64-bit shift + add to get our result in the low 8 bit
    // lanes. xx xx xx 0d xx xx xx 02
    //            \_________ |   paired64 = (uint8x8_t)(paired32 + (paired32 >>
    //            28))
    //                      \|
    // xx xx xx xx xx xx xx d2
    //
    // 00001101 00000010 (0d 02)
    //     \   \___ |  |
    //      '---.  \|  |
    // xxxxxxxx 11010010 (xx d2)
    uint8x16_t paired64 =
        vreinterpretq_u8_u64(vsraq_n_u64(paired32, paired32, 28));

    // Extract the low 8 bits from each 64-bit lane with 2 8-bit extracts.
    // xx xx xx xx xx xx xx d2
    //                      ||  return paired64[0]
    //                      d2
    // Note: Little endian would return the correct value 4b (01001011) instead.
    return vgetq_lane_u8(paired64, 0) | ((int) vgetq_lane_u8(paired64, 8) << 8);
}

// Computes the bitwise OR of the 128-bit value in a and the 128-bit value in b.
//
//   r := a | b
//
// https://msdn.microsoft.com/en-us/library/vstudio/ew8ty0db(v=vs.100).aspx
FORCE_INLINE __m128i _mm_or_si128(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vorrq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Sets the 8 signed 16-bit integer values.
// https://msdn.microsoft.com/en-au/library/3e0fek84(v=vs.90).aspx
FORCE_INLINE __m128i _mm_set_epi16(short i7,
                                   short i6,
                                   short i5,
                                   short i4,
                                   short i3,
                                   short i2,
                                   short i1,
                                   short i0)
{
    int16_t ALIGN_STRUCT(16) data[8] = {i0, i1, i2, i3, i4, i5, i6, i7};
    return vreinterpretq_m128i_s16(vld1q_s16(data));
}

// Sets the 4 signed 32-bit integer values.
// https://msdn.microsoft.com/en-us/library/vstudio/019beekt(v=vs.100).aspx
FORCE_INLINE __m128i _mm_set_epi32(int i3, int i2, int i1, int i0)
{
    int32_t ALIGN_STRUCT(16) data[4] = {i0, i1, i2, i3};
    return vreinterpretq_m128i_s32(vld1q_s32(data));
}

// Returns the __m128i structure with its two 64-bit integer values
// initialized to the values of the two 64-bit integers passed in.
// https://msdn.microsoft.com/en-us/library/dk2sdw0h(v=vs.120).aspx
FORCE_INLINE __m128i _mm_set_epi64x(int64_t i1, int64_t i2)
{
    return vreinterpretq_m128i_s64(
        vcombine_s64(vcreate_s64(i2), vcreate_s64(i1)));
}

// Sets the 16 signed 8-bit integer values.
// https://msdn.microsoft.com/en-us/library/x0cx8zd3(v=vs.90).aspx
FORCE_INLINE __m128i _mm_set_epi8(signed char b15,
                                  signed char b14,
                                  signed char b13,
                                  signed char b12,
                                  signed char b11,
                                  signed char b10,
                                  signed char b9,
                                  signed char b8,
                                  signed char b7,
                                  signed char b6,
                                  signed char b5,
                                  signed char b4,
                                  signed char b3,
                                  signed char b2,
                                  signed char b1,
                                  signed char b0)
{
    int8_t ALIGN_STRUCT(16)
        data[16] = {(int8_t) b0,  (int8_t) b1,  (int8_t) b2,  (int8_t) b3,
                    (int8_t) b4,  (int8_t) b5,  (int8_t) b6,  (int8_t) b7,
                    (int8_t) b8,  (int8_t) b9,  (int8_t) b10, (int8_t) b11,
                    (int8_t) b12, (int8_t) b13, (int8_t) b14, (int8_t) b15};
    return (__m128i) vld1q_s8(data);
}

// Sets the 4 signed 32-bit integer values in reverse order
// https://technet.microsoft.com/en-us/library/security/27yb3ee5(v=vs.90).aspx
FORCE_INLINE __m128i _mm_setr_epi32(int i3, int i2, int i1, int i0)
{
    int32_t ALIGN_STRUCT(16) data[4] = {i3, i2, i1, i0};
    return vreinterpretq_m128i_s32(vld1q_s32(data));
}

// Sets the 128-bit value to zero
// https://msdn.microsoft.com/en-us/library/vstudio/ys7dw0kh(v=vs.100).aspx
FORCE_INLINE __m128i _mm_setzero_si128(void)
{
    return vreinterpretq_m128i_s32(vdupq_n_s32(0));
}

// Shuffles the 4 signed or unsigned 32-bit integers in a as specified by imm.
// https://msdn.microsoft.com/en-us/library/56f67xbk%28v=vs.90%29.aspx
// FORCE_INLINE __m128i _mm_shuffle_epi32(__m128i a,
//                                        __constrange(0,255) int imm)
#ifdef _sse2neon_shuffle
#define _mm_shuffle_epi32(a, imm)                                            \
    __extension__({                                                          \
        int32x4_t _input = vreinterpretq_s32_m128i(a);                       \
        int32x4_t _shuf =                                                    \
            vshuffleq_s32(_input, _input, (imm) & (0x3), ((imm) >> 2) & 0x3, \
                          ((imm) >> 4) & 0x3, ((imm) >> 6) & 0x3);           \
        vreinterpretq_m128i_s32(_shuf);                                      \
    })
#else  // generic
#define _mm_shuffle_epi32(a, imm)                        \
    __extension__({                                      \
        __m128i ret;                                     \
        switch (imm) {                                   \
        case _MM_SHUFFLE(1, 0, 3, 2):                    \
            ret = _mm_shuffle_epi_1032((a));             \
            break;                                       \
        case _MM_SHUFFLE(2, 3, 0, 1):                    \
            ret = _mm_shuffle_epi_2301((a));             \
            break;                                       \
        case _MM_SHUFFLE(0, 3, 2, 1):                    \
            ret = _mm_shuffle_epi_0321((a));             \
            break;                                       \
        case _MM_SHUFFLE(2, 1, 0, 3):                    \
            ret = _mm_shuffle_epi_2103((a));             \
            break;                                       \
        case _MM_SHUFFLE(1, 0, 1, 0):                    \
            ret = _mm_shuffle_epi_1010((a));             \
            break;                                       \
        case _MM_SHUFFLE(1, 0, 0, 1):                    \
            ret = _mm_shuffle_epi_1001((a));             \
            break;                                       \
        case _MM_SHUFFLE(0, 1, 0, 1):                    \
            ret = _mm_shuffle_epi_0101((a));             \
            break;                                       \
        case _MM_SHUFFLE(2, 2, 1, 1):                    \
            ret = _mm_shuffle_epi_2211((a));             \
            break;                                       \
        case _MM_SHUFFLE(0, 1, 2, 2):                    \
            ret = _mm_shuffle_epi_0122((a));             \
            break;                                       \
        case _MM_SHUFFLE(3, 3, 3, 2):                    \
            ret = _mm_shuffle_epi_3332((a));             \
            break;                                       \
        case _MM_SHUFFLE(0, 0, 0, 0):                    \
            ret = _mm_shuffle_epi32_splat((a), 0);       \
            break;                                       \
        case _MM_SHUFFLE(1, 1, 1, 1):                    \
            ret = _mm_shuffle_epi32_splat((a), 1);       \
            break;                                       \
        case _MM_SHUFFLE(2, 2, 2, 2):                    \
            ret = _mm_shuffle_epi32_splat((a), 2);       \
            break;                                       \
        case _MM_SHUFFLE(3, 3, 3, 3):                    \
            ret = _mm_shuffle_epi32_splat((a), 3);       \
            break;                                       \
        default:                                         \
            ret = _mm_shuffle_epi32_default((a), (imm)); \
            break;                                       \
        }                                                \
        ret;                                             \
    })
#endif

// Shift packed 32-bit integers in a left by imm8 while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 3
//     i := j*32
//     IF imm8[7:0] > 31
//       dst[i+31:i] := 0
//     ELSE
//       dst[i+31:i] := ZeroExtend32(a[i+31:i] << imm8[7:0])
//     FI
//   ENDFOR
//
// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_slli_epi32
FORCE_INLINE __m128i _mm_slli_epi32(__m128i a, int imm)
{
    if (_sse2neon_unlikely(imm & ~31))
        return _mm_setzero_si128();
    return vreinterpretq_m128i_s32(
        vshlq_s32(vreinterpretq_s32_m128i(a), vdupq_n_s32(imm)));
}

// Shift packed 64-bit integers in a left by imm8 while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 1
//     i := j*64
//     IF imm8[7:0] > 63
//       dst[i+63:i] := 0
//     ELSE
//       dst[i+63:i] := ZeroExtend64(a[i+63:i] << imm8[7:0])
//     FI
//   ENDFOR
//
// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_slli_epi64
FORCE_INLINE __m128i _mm_slli_epi64(__m128i a, int imm)
{
    if (_sse2neon_unlikely(imm & ~63))
        return _mm_setzero_si128();
    return vreinterpretq_m128i_s64(
        vshlq_s64(vreinterpretq_s64_m128i(a), vdupq_n_s64(imm)));
}

// Shift a left by imm8 bytes while shifting in zeros, and store the results in
// dst.
//
//   tmp := imm8[7:0]
//   IF tmp > 15
//     tmp := 16
//   FI
//   dst[127:0] := a[127:0] << (tmp*8)
//
// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_slli_si128
#define _mm_slli_si128(a, imm)                                         \
    __extension__({                                                    \
        int8x16_t ret;                                                 \
        if (_sse2neon_unlikely(imm == 0))                              \
            ret = vreinterpretq_s8_m128i(a);                           \
        else if (_sse2neon_unlikely((imm) & ~15))                      \
            ret = vdupq_n_s8(0);                                       \
        else                                                           \
            ret = vextq_s8(vdupq_n_s8(0), vreinterpretq_s8_m128i(a),   \
                           ((imm <= 0 || imm > 15) ? 0 : (16 - imm))); \
        vreinterpretq_m128i_s8(ret);                                   \
    })

// Shift packed 32-bit integers in a right by imm8 while shifting in zeros, and
// store the results in dst.
//
//   FOR j := 0 to 3
//     i := j*32
//     IF imm8[7:0] > 31
//       dst[i+31:i] := 0
//     ELSE
//       dst[i+31:i] := ZeroExtend32(a[i+31:i] >> imm8[7:0])
//     FI
//   ENDFOR
//
// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_srli_epi32
// FORCE_INLINE __m128i _mm_srli_epi32(__m128i a, __constrange(0,255) int imm)
#define _mm_srli_epi32(a, imm)                                               \
    __extension__({                                                          \
        __m128i ret;                                                         \
        if (_sse2neon_unlikely((imm) & ~31)) {                               \
            ret = _mm_setzero_si128();                                       \
        } else {                                                             \
            ret = vreinterpretq_m128i_u32(                                   \
                vshlq_u32(vreinterpretq_u32_m128i(a), vdupq_n_s32(-(imm)))); \
        }                                                                    \
        ret;                                                                 \
    })

// Shift a right by imm8 bytes while shifting in zeros, and store the results in
// dst.
//
//   tmp := imm8[7:0]
//   IF tmp > 15
//     tmp := 16
//   FI
//   dst[127:0] := a[127:0] >> (tmp*8)
//
// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_srli_si128
#define _mm_srli_si128(a, imm)                                       \
    __extension__({                                                  \
        int8x16_t ret;                                               \
        if (_sse2neon_unlikely((imm) & ~15))                         \
            ret = vdupq_n_s8(0);                                     \
        else                                                         \
            ret = vextq_s8(vreinterpretq_s8_m128i(a), vdupq_n_s8(0), \
                           (imm > 15 ? 0 : imm));                    \
        vreinterpretq_m128i_s8(ret);                                 \
    })

// Computes the bitwise XOR of the 128-bit value in a and the 128-bit value in
// b.  https://msdn.microsoft.com/en-us/library/fzt08www(v=vs.100).aspx
FORCE_INLINE __m128i _mm_xor_si128(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        veorq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// /* SSE3 */

// Shuffle packed 8-bit integers in a according to shuffle control mask in the
// corresponding 8-bit element of b, and store the results in dst.
// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_shuffle_epi8
FORCE_INLINE __m128i _mm_shuffle_epi8(__m128i a, __m128i b)
{
    int8x16_t tbl = vreinterpretq_s8_m128i(a);   // input a
    uint8x16_t idx = vreinterpretq_u8_m128i(b);  // input b
    uint8x16_t idx_masked =
        vandq_u8(idx, vdupq_n_u8(0x8F));  // avoid using meaningless bits
#if defined(__aarch64__)
    return vreinterpretq_m128i_s8(vqtbl1q_s8(tbl, idx_masked));
#elif defined(__GNUC__)
    int8x16_t ret;
    // %e and %f represent the even and odd D registers
    // respectively.
    __asm__ __volatile__(
        "vtbl.8  %e[ret], {%e[tbl], %f[tbl]}, %e[idx]\n"
        "vtbl.8  %f[ret], {%e[tbl], %f[tbl]}, %f[idx]\n"
        : [ret] "=&w"(ret)
        : [tbl] "w"(tbl), [idx] "w"(idx_masked));
    return vreinterpretq_m128i_s8(ret);
#else
    // use this line if testing on aarch64
    int8x8x2_t a_split = {vget_low_s8(tbl), vget_high_s8(tbl)};
    return vreinterpretq_m128i_s8(
        vcombine_s8(vtbl2_s8(a_split, vget_low_u8(idx_masked)),
                    vtbl2_s8(a_split, vget_high_u8(idx_masked))));
#endif
}

// Compute the bitwise AND of 128 bits (representing integer data) in a and b,
// and set ZF to 1 if the result is zero, otherwise set ZF to 0. Compute the
// bitwise NOT of a and then AND with b, and set CF to 1 if the result is zero,
// otherwise set CF to 0. Return the ZF value.
// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_testz_si128
FORCE_INLINE int _mm_testz_si128(__m128i a, __m128i b)
{
    int64x2_t s64 =
        vandq_s64(vreinterpretq_s64_m128i(a), vreinterpretq_s64_m128i(b));
    return !(vgetq_lane_s64(s64, 0) | vgetq_lane_s64(s64, 1));
}

// /* AES */

#if !defined(__ARM_FEATURE_CRYPTO)
/* clang-format off */
#define SSE2NEON_AES_SBOX(w)                                           \
    {                                                                  \
        w(0x63), w(0x7c), w(0x77), w(0x7b), w(0xf2), w(0x6b), w(0x6f), \
        w(0xc5), w(0x30), w(0x01), w(0x67), w(0x2b), w(0xfe), w(0xd7), \
        w(0xab), w(0x76), w(0xca), w(0x82), w(0xc9), w(0x7d), w(0xfa), \
        w(0x59), w(0x47), w(0xf0), w(0xad), w(0xd4), w(0xa2), w(0xaf), \
        w(0x9c), w(0xa4), w(0x72), w(0xc0), w(0xb7), w(0xfd), w(0x93), \
        w(0x26), w(0x36), w(0x3f), w(0xf7), w(0xcc), w(0x34), w(0xa5), \
        w(0xe5), w(0xf1), w(0x71), w(0xd8), w(0x31), w(0x15), w(0x04), \
        w(0xc7), w(0x23), w(0xc3), w(0x18), w(0x96), w(0x05), w(0x9a), \
        w(0x07), w(0x12), w(0x80), w(0xe2), w(0xeb), w(0x27), w(0xb2), \
        w(0x75), w(0x09), w(0x83), w(0x2c), w(0x1a), w(0x1b), w(0x6e), \
        w(0x5a), w(0xa0), w(0x52), w(0x3b), w(0xd6), w(0xb3), w(0x29), \
        w(0xe3), w(0x2f), w(0x84), w(0x53), w(0xd1), w(0x00), w(0xed), \
        w(0x20), w(0xfc), w(0xb1), w(0x5b), w(0x6a), w(0xcb), w(0xbe), \
        w(0x39), w(0x4a), w(0x4c), w(0x58), w(0xcf), w(0xd0), w(0xef), \
        w(0xaa), w(0xfb), w(0x43), w(0x4d), w(0x33), w(0x85), w(0x45), \
        w(0xf9), w(0x02), w(0x7f), w(0x50), w(0x3c), w(0x9f), w(0xa8), \
        w(0x51), w(0xa3), w(0x40), w(0x8f), w(0x92), w(0x9d), w(0x38), \
        w(0xf5), w(0xbc), w(0xb6), w(0xda), w(0x21), w(0x10), w(0xff), \
        w(0xf3), w(0xd2), w(0xcd), w(0x0c), w(0x13), w(0xec), w(0x5f), \
        w(0x97), w(0x44), w(0x17), w(0xc4), w(0xa7), w(0x7e), w(0x3d), \
        w(0x64), w(0x5d), w(0x19), w(0x73), w(0x60), w(0x81), w(0x4f), \
        w(0xdc), w(0x22), w(0x2a), w(0x90), w(0x88), w(0x46), w(0xee), \
        w(0xb8), w(0x14), w(0xde), w(0x5e), w(0x0b), w(0xdb), w(0xe0), \
        w(0x32), w(0x3a), w(0x0a), w(0x49), w(0x06), w(0x24), w(0x5c), \
        w(0xc2), w(0xd3), w(0xac), w(0x62), w(0x91), w(0x95), w(0xe4), \
        w(0x79), w(0xe7), w(0xc8), w(0x37), w(0x6d), w(0x8d), w(0xd5), \
        w(0x4e), w(0xa9), w(0x6c), w(0x56), w(0xf4), w(0xea), w(0x65), \
        w(0x7a), w(0xae), w(0x08), w(0xba), w(0x78), w(0x25), w(0x2e), \
        w(0x1c), w(0xa6), w(0xb4), w(0xc6), w(0xe8), w(0xdd), w(0x74), \
        w(0x1f), w(0x4b), w(0xbd), w(0x8b), w(0x8a), w(0x70), w(0x3e), \
        w(0xb5), w(0x66), w(0x48), w(0x03), w(0xf6), w(0x0e), w(0x61), \
        w(0x35), w(0x57), w(0xb9), w(0x86), w(0xc1), w(0x1d), w(0x9e), \
        w(0xe1), w(0xf8), w(0x98), w(0x11), w(0x69), w(0xd9), w(0x8e), \
        w(0x94), w(0x9b), w(0x1e), w(0x87), w(0xe9), w(0xce), w(0x55), \
        w(0x28), w(0xdf), w(0x8c), w(0xa1), w(0x89), w(0x0d), w(0xbf), \
        w(0xe6), w(0x42), w(0x68), w(0x41), w(0x99), w(0x2d), w(0x0f), \
        w(0xb0), w(0x54), w(0xbb), w(0x16)                             \
    }
#define SSE2NEON_AES_RSBOX(w)                                          \
    {                                                                  \
        w(0x52), w(0x09), w(0x6a), w(0xd5), w(0x30), w(0x36), w(0xa5), \
        w(0x38), w(0xbf), w(0x40), w(0xa3), w(0x9e), w(0x81), w(0xf3), \
        w(0xd7), w(0xfb), w(0x7c), w(0xe3), w(0x39), w(0x82), w(0x9b), \
        w(0x2f), w(0xff), w(0x87), w(0x34), w(0x8e), w(0x43), w(0x44), \
        w(0xc4), w(0xde), w(0xe9), w(0xcb), w(0x54), w(0x7b), w(0x94), \
        w(0x32), w(0xa6), w(0xc2), w(0x23), w(0x3d), w(0xee), w(0x4c), \
        w(0x95), w(0x0b), w(0x42), w(0xfa), w(0xc3), w(0x4e), w(0x08), \
        w(0x2e), w(0xa1), w(0x66), w(0x28), w(0xd9), w(0x24), w(0xb2), \
        w(0x76), w(0x5b), w(0xa2), w(0x49), w(0x6d), w(0x8b), w(0xd1), \
        w(0x25), w(0x72), w(0xf8), w(0xf6), w(0x64), w(0x86), w(0x68), \
        w(0x98), w(0x16), w(0xd4), w(0xa4), w(0x5c), w(0xcc), w(0x5d), \
        w(0x65), w(0xb6), w(0x92), w(0x6c), w(0x70), w(0x48), w(0x50), \
        w(0xfd), w(0xed), w(0xb9), w(0xda), w(0x5e), w(0x15), w(0x46), \
        w(0x57), w(0xa7), w(0x8d), w(0x9d), w(0x84), w(0x90), w(0xd8), \
        w(0xab), w(0x00), w(0x8c), w(0xbc), w(0xd3), w(0x0a), w(0xf7), \
        w(0xe4), w(0x58), w(0x05), w(0xb8), w(0xb3), w(0x45), w(0x06), \
        w(0xd0), w(0x2c), w(0x1e), w(0x8f), w(0xca), w(0x3f), w(0x0f), \
        w(0x02), w(0xc1), w(0xaf), w(0xbd), w(0x03), w(0x01), w(0x13), \
        w(0x8a), w(0x6b), w(0x3a), w(0x91), w(0x11), w(0x41), w(0x4f), \
        w(0x67), w(0xdc), w(0xea), w(0x97), w(0xf2), w(0xcf), w(0xce), \
        w(0xf0), w(0xb4), w(0xe6), w(0x73), w(0x96), w(0xac), w(0x74), \
        w(0x22), w(0xe7), w(0xad), w(0x35), w(0x85), w(0xe2), w(0xf9), \
        w(0x37), w(0xe8), w(0x1c), w(0x75), w(0xdf), w(0x6e), w(0x47), \
        w(0xf1), w(0x1a), w(0x71), w(0x1d), w(0x29), w(0xc5), w(0x89), \
        w(0x6f), w(0xb7), w(0x62), w(0x0e), w(0xaa), w(0x18), w(0xbe), \
        w(0x1b), w(0xfc), w(0x56), w(0x3e), w(0x4b), w(0xc6), w(0xd2), \
        w(0x79), w(0x20), w(0x9a), w(0xdb), w(0xc0), w(0xfe), w(0x78), \
        w(0xcd), w(0x5a), w(0xf4), w(0x1f), w(0xdd), w(0xa8), w(0x33), \
        w(0x88), w(0x07), w(0xc7), w(0x31), w(0xb1), w(0x12), w(0x10), \
        w(0x59), w(0x27), w(0x80), w(0xec), w(0x5f), w(0x60), w(0x51), \
        w(0x7f), w(0xa9), w(0x19), w(0xb5), w(0x4a), w(0x0d), w(0x2d), \
        w(0xe5), w(0x7a), w(0x9f), w(0x93), w(0xc9), w(0x9c), w(0xef), \
        w(0xa0), w(0xe0), w(0x3b), w(0x4d), w(0xae), w(0x2a), w(0xf5), \
        w(0xb0), w(0xc8), w(0xeb), w(0xbb), w(0x3c), w(0x83), w(0x53), \
        w(0x99), w(0x61), w(0x17), w(0x2b), w(0x04), w(0x7e), w(0xba), \
        w(0x77), w(0xd6), w(0x26), w(0xe1), w(0x69), w(0x14), w(0x63), \
        w(0x55), w(0x21), w(0x0c), w(0x7d)                             \
    }
/* clang-format on */

/* X Macro trick. See https://en.wikipedia.org/wiki/X_Macro */
#define SSE2NEON_AES_H0(x) (x)
static const uint8_t SSE2NEON_sbox[256] = SSE2NEON_AES_SBOX(SSE2NEON_AES_H0);
static const uint8_t SSE2NEON_rsbox[256] = SSE2NEON_AES_RSBOX(SSE2NEON_AES_H0);
#undef SSE2NEON_AES_H0


// Perform one round of an AES decryption flow on data (state) in a using the
// round key in RoundKey, and store the result in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aesdec_si128
FORCE_INLINE __m128i _mm_aesdec_si128(__m128i a, __m128i RoundKey)
{
#if defined(__aarch64__)
    static const uint8_t inv_shift_rows[] = {0x0, 0xd, 0xa, 0x7, 0x4, 0x1,
                                             0xe, 0xb, 0x8, 0x5, 0x2, 0xf,
                                             0xc, 0x9, 0x6, 0x3};
    static const uint8_t ror32by8[] = {0x1, 0x2, 0x3, 0x0, 0x5, 0x6, 0x7, 0x4,
                                       0x9, 0xa, 0xb, 0x8, 0xd, 0xe, 0xf, 0xc};

    uint8x16_t v;
    uint8x16_t w = vreinterpretq_u8_m128i(a);

    // shift rows
    w = vqtbl1q_u8(w, vld1q_u8(inv_shift_rows));

    // sub bytes
    v = vqtbl4q_u8(_sse2neon_vld1q_u8_x4(SSE2NEON_rsbox), w);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(SSE2NEON_rsbox + 0x40), w - 0x40);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(SSE2NEON_rsbox + 0x80), w - 0x80);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(SSE2NEON_rsbox + 0xc0), w - 0xc0);

    // muliplying 'v' by 4 in GF(2^8)
    w = (v << 1) ^ (uint8x16_t) (((int8x16_t) v >> 7) & 0x1b);
    w = (w << 1) ^ (uint8x16_t) (((int8x16_t) w >> 7) & 0x1b);
    v ^= w;
    v ^= (uint8x16_t) vrev32q_u16((uint16x8_t) w);

    // mix columns
    w = (v << 1) ^ (uint8x16_t) (((int8x16_t) v >> 7) &
                                 0x1b);  // muliplying 'v' by 2 in GF(2^8)
    w ^= (uint8x16_t) vrev32q_u16((uint16x8_t) v);
    w ^= vqtbl1q_u8(v ^ w, vld1q_u8(ror32by8));

    // add round key
    return vreinterpretq_m128i_u8(w) ^ RoundKey;

#else /* ARMv7-A NEON implementation */
/* FIXME: optimized for NEON */
#define XT(x) (((x) << 1) ^ ((((x) >> 7) & 1) * 0x1b))
#define MULTIPLY(x, y)                                                     \
    (((y & 1) * x) ^ ((y >> 1 & 1) * XT(x)) ^ ((y >> 2 & 1) * XT(XT(x))) ^ \
     ((y >> 3 & 1) * XT(XT(XT(x)))) ^ ((y >> 4 & 1) * XT(XT(XT(XT(x))))))

    uint8_t i, e, f, g, h, v[4][4];
    uint8_t *_a = (uint8_t *) &a;
    for (i = 0; i < 16; ++i) {
        v[((i / 4) + (i % 4)) % 4][i % 4] = SSE2NEON_rsbox[_a[i]];
    }

    for (i = 0; i < 4; ++i) {
        e = v[i][0];
        f = v[i][1];
        g = v[i][2];
        h = v[i][3];

        v[i][0] = MULTIPLY(e, 0x0e) ^ MULTIPLY(f, 0x0b) ^ MULTIPLY(g, 0x0d) ^
                  MULTIPLY(h, 0x09);
        v[i][1] = MULTIPLY(e, 0x09) ^ MULTIPLY(f, 0x0e) ^ MULTIPLY(g, 0x0b) ^
                  MULTIPLY(h, 0x0d);
        v[i][2] = MULTIPLY(e, 0x0d) ^ MULTIPLY(f, 0x09) ^ MULTIPLY(g, 0x0e) ^
                  MULTIPLY(h, 0x0b);
        v[i][3] = MULTIPLY(e, 0x0b) ^ MULTIPLY(f, 0x0d) ^ MULTIPLY(g, 0x09) ^
                  MULTIPLY(h, 0x0e);
    }
#undef XT
#undef MULTIPLY

    return vreinterpretq_m128i_u8(vld1q_u8((uint8_t *) v)) ^ RoundKey;
#endif
}

// Perform the last round of an AES encryption flow on data (state) in a using
// the round key in RoundKey, and store the result in dst.
// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_aesenclast_si128
FORCE_INLINE __m128i _mm_aesenclast_si128(__m128i a, __m128i RoundKey)
{
#if defined(__aarch64__)
    static const uint8_t shift_rows[] = {
        0x0, 0x5, 0xa, 0xf, 0x4, 0x9, 0xe, 0x3,
        0x8, 0xd, 0x2, 0x7, 0xc, 0x1, 0x6, 0xb,
    };

    uint8x16_t v;
    uint8x16_t w = vreinterpretq_u8_m128i(a);

    // shift rows
    w = vqtbl1q_u8(w, vld1q_u8(shift_rows));

    // sub bytes
    v = vqtbl4q_u8(_sse2neon_vld1q_u8_x4(SSE2NEON_sbox), w);
    // 'w-0x40' equals to 'vsubq_u8(w, vdupq_n_u8(0x40))'
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(SSE2NEON_sbox + 0x40), w - 0x40);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(SSE2NEON_sbox + 0x80), w - 0x80);
    v = vqtbx4q_u8(v, _sse2neon_vld1q_u8_x4(SSE2NEON_sbox + 0xc0), w - 0xc0);

    //  add round key
    return vreinterpretq_m128i_u8(v) ^ RoundKey;

#else /* ARMv7-A implementation */
    uint8_t v[16] = {
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 0)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 5)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 10)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 15)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 4)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 9)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 14)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 3)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 8)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 13)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 2)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 7)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 12)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 1)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 6)],
        SSE2NEON_sbox[vgetq_lane_u8(vreinterpretq_u8_m128i(a), 11)],
    };

    return vreinterpretq_m128i_u8(vld1q_u8(v)) ^ RoundKey;
#endif
}

// Emits the Advanced Encryption Standard (AES) instruction aeskeygenassist.
// This instruction generates a round key for AES encryption. See
// https://kazakov.life/2017/11/01/cryptocurrency-mining-on-ios-devices/
// for details.
//
// https://msdn.microsoft.com/en-us/library/cc714138(v=vs.120).aspx
FORCE_INLINE __m128i _mm_aeskeygenassist_si128(__m128i key, const int rcon)
{
    uint32_t X1 = _mm_cvtsi128_si32(_mm_shuffle_epi32(key, 0x55));
    uint32_t X3 = _mm_cvtsi128_si32(_mm_shuffle_epi32(key, 0xFF));
    for (int i = 0; i < 4; ++i) {
        ((uint8_t *) &X1)[i] = SSE2NEON_sbox[((uint8_t *) &X1)[i]];
        ((uint8_t *) &X3)[i] = SSE2NEON_sbox[((uint8_t *) &X3)[i]];
    }
    return _mm_set_epi32(((X3 >> 8) | (X3 << 24)) ^ rcon, X3,
                         ((X1 >> 8) | (X1 << 24)) ^ rcon, X1);
}
#undef SSE2NEON_AES_SBOX
#undef SSE2NEON_AES_RSBOX

#else /* __ARM_FEATURE_CRYPTO */

// Perform one round of an AES decryption flow on data (state) in a using the
// round key in RoundKey, and store the result in dst.
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aesdec_si128
FORCE_INLINE __m128i _mm_aesdec_si128(__m128i a, __m128i RoundKey)
{
    return vreinterpretq_m128i_u8(veorq_u8(
        vaesimcq_u8(vaesdq_u8(vreinterpretq_u8_m128i(a), vdupq_n_u8(0))),
        vreinterpretq_u8_m128i(RoundKey)));
}

// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_aesenclast_si128
FORCE_INLINE __m128i _mm_aesenclast_si128(__m128i a, __m128i RoundKey)
{
    return _mm_xor_si128(vreinterpretq_m128i_u8(vaeseq_u8(
                             vreinterpretq_u8_m128i(a), vdupq_n_u8(0))),
                         RoundKey);
}

// Assist in expanding the AES cipher key by computing steps towards generating
// a round key for encryption cipher using data from a and an 8-bit round
// constant specified in imm8, and store the result in dst."
// https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html#text=_mm_aeskeygenassist_si128
FORCE_INLINE __m128i _mm_aeskeygenassist_si128(__m128i a, const int rcon)
{
    // AESE does ShiftRows and SubBytes on A
    uint8x16_t u8 = vaeseq_u8(vreinterpretq_u8_m128i(a), vdupq_n_u8(0));

    uint8x16_t dest = {
        // Undo ShiftRows step from AESE and extract X1 and X3
        u8[0x4], u8[0x1], u8[0xE], u8[0xB],  // SubBytes(X1)
        u8[0x1], u8[0xE], u8[0xB], u8[0x4],  // ROT(SubBytes(X1))
        u8[0xC], u8[0x9], u8[0x6], u8[0x3],  // SubBytes(X3)
        u8[0x9], u8[0x6], u8[0x3], u8[0xC],  // ROT(SubBytes(X3))
    };
    uint32x4_t r = {0, (unsigned) rcon, 0, (unsigned) rcon};
    return vreinterpretq_m128i_u8(dest) ^ vreinterpretq_m128i_u32(r);
}
#endif

#if defined(__GNUC__) || defined(__clang__)
#pragma pop_macro("ALIGN_STRUCT")
#pragma pop_macro("FORCE_INLINE")
#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC pop_options
#endif

#endif
