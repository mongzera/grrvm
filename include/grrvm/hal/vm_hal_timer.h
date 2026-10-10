#ifndef GRRVM_HAL_VM_HAL_TIMER_H
#define GRRVM_HAL_VM_HAL_TIMER_H

#include <stdint.h>

// Returns the system uptime in nanoseconds using a 64-bit unsigned integer
uint64_t hal_clock_ns(void);
uint32_t hal_clock_us(void);


#endif /* GRRVM_HAL_VM_HAL_TIMER_H */
