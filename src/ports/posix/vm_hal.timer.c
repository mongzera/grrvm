#include "../../../include/grrvm/hal/vm_hal_timer.h"
#include <time.h>


uint64_t hal_clock_ns(void) {
    struct timespec ts;

    // Fetch the high-resolution monotonic system clock
    clock_gettime(CLOCK_MONOTONIC, &ts);

    // Convert seconds and nanoseconds into a single, unified 64-bit nanosecond value
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}
