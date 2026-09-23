#ifndef VM_HAL_TIMER
#define VM_HAL_TIMER

#include <stdint.h>

// Returns the system uptime in nanoseconds using a 64-bit unsigned integer
inline uint64_t hal_clock_ns(void);

#endif // VM_HAL_TIMER
