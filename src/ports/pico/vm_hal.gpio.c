#include "grrvm/hal/vm_hal_gpio.h"
#include "hardware/gpio.h" // Pico SDK

void hal_gpio_init(hal_gpio_pin_t pin, hal_gpio_mode_t mode, hal_gpio_pull_t pull) {
    uint32_t pin_num = pin.handle;

    gpio_init(pin_num); // SDK initialization call

    if (mode == HAL_GPIO_MODE_OUTPUT_PP || mode == HAL_GPIO_MODE_OUTPUT_OD) {
        gpio_set_dir(pin_num, GPIO_OUT);
    } else {
        gpio_set_dir(pin_num, GPIO_IN);
    }

    gpio_set_pulls(pin_num, (pull == HAL_GPIO_PULL_UP), (pull == HAL_GPIO_PULL_DOWN));
}

void hal_gpio_write(hal_gpio_pin_t pin, hal_gpio_state_t state) {
    gpio_put(pin.handle, state == HAL_GPIO_STATE_HIGH);
}

void hal_gpio_set(hal_gpio_pin_t pin) {
    gpio_put(pin.handle, true);
}

void hal_gpio_clear(hal_gpio_pin_t pin) {
    gpio_put(pin.handle, false);
}

void hal_gpio_toggle(hal_gpio_pin_t pin) {
    gpio_xor_mask(1u << pin.handle);
}

hal_gpio_state_t hal_gpio_read(hal_gpio_pin_t pin) {
    return gpio_get(pin.handle) ? HAL_GPIO_STATE_HIGH : HAL_GPIO_STATE_LOW;
}
