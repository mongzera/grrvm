#include "grrvm/hal/vm_hal_math.h"
#include <fcntl.h>
#include <unistd.h>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

static _Thread_local g_u32 g_posix_rng_state = 0xACE14B54U;

void hal_rng_init(g_u32 seed) {
    if (seed != 0) {
        g_posix_rng_state = seed;
        return;
    }

    // Try reading true system OS entropy from /dev/urandom
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        g_u32 os_seed = 0;
        if (read(fd, &os_seed, sizeof(os_seed)) == sizeof(os_seed)) {
            g_posix_rng_state = os_seed;
        }
        close(fd);
    }
}

g_u32 hal_rng_next_u32(void) {
#if defined(__x86_64__) && defined(__RDRND__)
    // Hardware acceleration via Intel/AMD RDRAND instruction if available
    unsigned int rand_val;
    if (_rdrand32_step(&rand_val)) {
        return (g_u32)rand_val;
    }
#endif

    // Fast 64-bit LCG / PCG-style PRNG fallback
    g_u32 x = g_posix_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_posix_rng_state = x;
    return x;
}

g_f32 hal_rng_next_f32(void) {
    return (g_f32)(hal_rng_next_u32() >> 8) * (1.0f / 16777216.0f);
}

g_i32 hal_rng_range_i32(g_i32 min, g_i32 max) {
    if (min >= max) return min;
    g_u32 range = (g_u32)(max - min + 1);

    return min + (g_i32)(hal_rng_next_u32() % range);
}

g_f32 hal_inv_sqrt(g_f32 x) {
#if defined(__x86_64__)
    // Direct x86 SSE hardware vector instruction for inverse square root
    _mm_store_ss(&x, _mm_rsqrt_ss(_mm_load_ss(&x)));
    return x;
#else
    return 1.0f / sqrtf(x);
#endif
}
