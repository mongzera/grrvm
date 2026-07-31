#ifndef VM_HAL_GPIO
#define VM_HAL_GPIO

#include "grrvm/types.h"

void vm_hal_sys_init(void);
word vm_hal_sys_get_ticks_ms(void);
void vm_hal_sys_delay_ms(word ms);

#endif
