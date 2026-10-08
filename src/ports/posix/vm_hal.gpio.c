#include "grrvm/hal/vm_hal_gpio.h"
#include <stdio.h>

void hal_gpio_init(hal_gpio_pin_t pin, hal_gpio_mode_t mode, hal_gpio_pull_t pull) {
    (void)pin;
    (void)mode;
    (void)pull;
    printf("[POSIX GPIO] hal_gpio_init: Not Implemented (Handle: 0x%08X)\n", pin.handle);
}

void hal_gpio_write(hal_gpio_pin_t pin, hal_gpio_state_t state) {
    (void)pin;
    (void)state;
    printf("[POSIX GPIO] hal_gpio_write: Not Implemented (Handle: 0x%08X, State: %d)\n", pin.handle, state);
}

void hal_gpio_set(hal_gpio_pin_t pin) {
    (void)pin;
    printf("[POSIX GPIO] hal_gpio_set: Not Implemented (Handle: 0x%08X)\n", pin.handle);
}

void hal_gpio_clear(hal_gpio_pin_t pin) {
    (void)pin;
    printf("[POSIX GPIO] hal_gpio_clear: Not Implemented (Handle: 0x%08X)\n", pin.handle);
}

void hal_gpio_toggle(hal_gpio_pin_t pin) {
    (void)pin;
    printf("[POSIX GPIO] hal_gpio_toggle: Not Implemented (Handle: 0x%08X)\n", pin.handle);
}

hal_gpio_state_t hal_gpio_read(hal_gpio_pin_t pin) {
    (void)pin;
    printf("[POSIX GPIO] hal_gpio_read: Not Implemented (Handle: 0x%08X)\n", pin.handle);
    return HAL_GPIO_STATE_LOW;
}
