#ifndef VM_HAL_MATH_H
#define VM_HAL_MATH_H

#include "grrvm/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * 1. HARDWARE RANDOM NUMBER GENERATOR (RNG) API
 * ========================================================================= */

/**
 * Initializes the hardware RNG peripheral or seeds the fallback PRNG.
 * Call this during VM boot sequence.
 */
void hal_rng_init(g_u32 seed);

/**
 * Returns a raw 32-bit random unsigned integer.
 * On MCUs with a hardware TRNG (e.g., RP2040, ESP32, STM32), this reads from
 * the hardware RNG register. Falls back to a deterministic PRNG (Xorshift32)
 * if platform hardware support is unavailable.
 */
g_u32 hal_rng_next_u32(void);

/**
 * Returns a pseudo-random 32-bit float in the range [0.0f, 1.0f).
 */
g_f32 hal_rng_next_f32(void);

/**
 * Returns a random integer in the range [min, max] inclusive.
 */
g_i32 hal_rng_range_i32(g_i32 min, g_i32 max);


/* =========================================================================
 * 2. CORE PLATFORM MATH PRIMITIVES
 * ========================================================================= */

/**
 * Fast Inverse Square Root (1 / sqrt(x))
 * Useful for robotics vector normalization and fast physics calculations.
 */
g_f32 hal_inv_sqrt(g_f32 x);

/**
 * Clamps a float value between low and high bounds.
 */
static inline g_f32 _hal_clamp_f32(g_f32 val, g_f32 low, g_f32 high) {
    if (val < low) return low;
    if (val > high) return high;
    return val;
}

/**
 * Linear Interpolation (lerp)
 */
static inline g_f32 hal_lerp_f32(g_f32 a, g_f32 b, g_f32 t) {
    return a + t * (b - a);
}

#ifdef __cplusplus
}
#endif



#endif
