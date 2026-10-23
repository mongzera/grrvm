#ifndef GRRVM_HAL_VM_HAL_GPIO_H
#define GRRVM_HAL_VM_HAL_GPIO_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    HAL_GPIO_MODE_INPUT = 0,
    HAL_GPIO_MODE_OUTPUT_PP,  // Push-Pull Output
    HAL_GPIO_MODE_OUTPUT_OD,  // Open-Drain Output
    HAL_GPIO_MODE_ANALOG      // Analog / High-Z
} hal_gpio_mode_t;

typedef enum {
    HAL_GPIO_PULL_NONE = 0,
    HAL_GPIO_PULL_UP,
    HAL_GPIO_PULL_DOWN
} hal_gpio_pull_t;

typedef enum {
    HAL_GPIO_STATE_LOW  = 0,
    HAL_GPIO_STATE_HIGH = 1
} hal_gpio_state_t;

// 32-bit packed handle passed by value
typedef struct {
    uint32_t handle;
} hal_gpio_pin_t;

// Standard HAL API
void             hal_gpio_init(hal_gpio_pin_t pin, hal_gpio_mode_t mode, hal_gpio_pull_t pull);
void             hal_gpio_write(hal_gpio_pin_t pin, hal_gpio_state_t state);
void             hal_gpio_set(hal_gpio_pin_t pin);
void             hal_gpio_clear(hal_gpio_pin_t pin);
void             hal_gpio_toggle(hal_gpio_pin_t pin);
hal_gpio_state_t hal_gpio_read(hal_gpio_pin_t pin);

#endif /* GRRVM_HAL_VM_HAL_GPIO_H */
