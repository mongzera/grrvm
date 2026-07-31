#ifndef VM_HAL_GPIO_H
#define VM_HAL_GPIO_H

#include "grrvm/types.h"
void vm_hal_gpio_set_mode(word pin, word mode);
void vm_hal_gpio_write(word pin, word data);
word vm_hal_gpio_read(word pin);

#endif
