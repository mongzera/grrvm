#include "../../../include/grrvm/hal/vm_hal_timer.h"
#include "pico/stdlib.h"

uint64_t hal_clock_ns(void) {
    // time_us_64() returns system uptime in microseconds (uint64_t).
    // Multiply by 1000 to convert microseconds to nanoseconds.
    return time_us_64() * 1000ULL;
}
