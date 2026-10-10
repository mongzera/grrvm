#ifndef GRRVM_NATIVE_WRAPPERS_GPIO_WRAPPER_H
#define GRRVM_NATIVE_WRAPPERS_GPIO_WRAPPER_H
#include "grrvm/vm.h"


// Expose the wrapper functions
void __wrap__gpio_init  (VM_Thread* thread);
void __wrap__gpio_write (VM_Thread* thread);
void __wrap__gpio_read  (VM_Thread* thread);
void __wrap__gpio_set   (VM_Thread* thread);
void __wrap__gpio_clear (VM_Thread* thread);
void __wrap__gpio_toggle(VM_Thread* thread);

void register_gpio_wrappers(void);

#endif /* GRRVM_NATIVE_WRAPPERS_GPIO_WRAPPER_H */
