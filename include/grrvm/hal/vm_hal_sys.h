#ifndef GRRVM_HAL_VM_HAL_SYS_H
#define GRRVM_HAL_VM_HAL_SYS_H

#include "grrvm/types.h"

void vm_hal_sys_init(void);
word vm_hal_sys_get_ticks_ms(void);
void vm_hal_sys_delay_ms(word ms);

#endif /* GRRVM_HAL_VM_HAL_SYS_H */
