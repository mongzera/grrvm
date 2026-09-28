#include "grrvm/hal/vm_hal_math.h"
#include "pico/stdlib.h"
#include "hardware/structs/rosc.h"

static g_u32 g_pico_rng_state = 0xACE14B54U;

/**
 * Samples raw entropy from the RP2040 ROSC (Ring Oscillator) hardware counter.
 * The ROSC runs asynchronously from the CPU clock, yielding low-level jitter.
 */
static inline g_u32 read_rosc_entropy(void) {
    g_u32 entropy = 0;
    for (int i = 0; i < 32; i++) {
        // Read low bit of ROSC random bit generator register
        entropy = (entropy << 1) | (rosc_hw->randombit & 1);
    }
    return entropy;
}

void hal_rng_init(g_u32 seed) {
    g_u32 rosc_seed = read_rosc_entropy();

    if (seed != 0) {
        g_pico_rng_state = seed ^ rosc_seed;
    } else {
        g_pico_rng_state = (rosc_seed != 0) ? rosc_seed : 0xACE14B54U;
    }
}

g_u32 hal_rng_next_u32(void) {
    // 32-bit Xorshift PRNG executing in single-cycle Cortex-M0+ instructions
    g_u32 x = g_pico_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_pico_rng_state = x;
    return x;
}

g_f32 hal_rng_next_f32(void) {
    // 24-bit float mantissa normalization
    return (g_f32)(hal_rng_next_u32() >> 8) * (1.0f / 16777216.0f);
}

g_i32 hal_rng_range_i32(g_i32 min, g_i32 max) {
    if (min >= max) return min;
    g_u32 range = (g_u32)(max - min + 1);

    // Pure 32-bit unbiased range calculation (modulo reduction)
    return min + (g_i32)(hal_rng_next_u32() % range);
}

g_f32 hal_inv_sqrt(g_f32 x) {
    // On RP2040 (Cortex-M0+ without hardware FPU), Fast InvSqrt beats standard software float division
    g_f32 x2 = x * 0.5f;
    g_f32 y = x;

    union { g_f32 f; g_i32 i; } u;
    u.f = y;
    u.i = 0x5f3759df - (u.i >> 1);
    y = u.f;
    y = y * (1.5f - (x2 * y * y)); // 1st Newton-Raphson iteration

    return y;
}
