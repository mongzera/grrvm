#ifndef MAX_POW2_H
#define MAX_POW2_H

#include <stdint.h>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

/**
 * @brief Finds the largest power of 2 less than or equal to x.
 *        Example: Input 136 (10001000) -> Returns 128 (10000000).
 *
 * @param x The 32-bit unsigned integer to evaluate.
 * @return The largest power of 2 that fits inside x, or 0 if x is 0.
 */
static inline uint32_t max_power_of_two_32(uint32_t x) {
    // Handle edge case where x is 0
    if (x == 0) {
        return 0;
    }

#if defined(__GNUC__) || defined(__clang__)
    // GCC/Clang hardware-accelerated 32-bit Leading Zero Count
    int leading_zeros = __builtin_clz(x);
    return (uint32_t)1 << (31 - leading_zeros);

#elif defined(_MSC_VER)
    // MSVC hardware-accelerated 32-bit Bit Scan Reverse
    unsigned long index;
    _BitScanReverse(&index, x);
    return (uint32_t)1 << index;

#else
    // Fallback portable branchless bit-smearing for unknown compilers
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x - (x >> 1);
#endif
}

#endif /* MAX_POW2_H */
