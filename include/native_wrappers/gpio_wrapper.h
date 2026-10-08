#ifndef GPIO_WRAPPER_H
#define GPIO_WRAPPER_H
#include "grrvm/vm.h"


// Expose the wrapper functions
void __wrap__gpio_init  (VM_Thread* thread);
void __wrap__gpio_write (VM_Thread* thread);
void __wrap__gpio_read  (VM_Thread* thread);
void __wrap__gpio_set   (VM_Thread* thread);
void __wrap__gpio_clear (VM_Thread* thread);
void __wrap__gpio_toggle(VM_Thread* thread);

// void             hal_gpio_init(hal_gpio_pin_t pin, hal_gpio_mode_t mode, hal_gpio_pull_t pull);
// void             hal_gpio_write(hal_gpio_pin_t pin, hal_gpio_state_t state);
// void             hal_gpio_set(hal_gpio_pin_t pin);
// void             hal_gpio_clear(hal_gpio_pin_t pin);
// void             hal_gpio_toggle(hal_gpio_pin_t pin);
// hal_gpio_state_t hal_gpio_read(hal_gpio_pin_t pin);


void register_gpio_wrappers(void);

#endif
