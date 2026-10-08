#include "native_wrappers/gpio_wrapper.h"
#include "grrvm/hal/vm_hal_gpio.h"
#include "grrvm/type_checks.h"
#include "grrvm/vm_native.h"
#include "grrvm/vm_thread.h"


// Expose the wrapper functions
// Helper inline function to convert VM integer to hal_gpio_pin_t struct
static inline hal_gpio_pin_t make_hal_pin(int32_t val) {
    hal_gpio_pin_t pin = { .handle = (uint32_t)val };
    return pin;
}

void __wrap__gpio_init(VM_Thread* thread) {
    prim_val pull, mode, pin;

    if (!pop_stack(thread, &pull) || !pop_stack(thread, &mode) || !pop_stack(thread, &pin)) {
        vm_error("NATIVE ERROR", "Pop stack failed for gpio_init parameters");
        set_thread_inactive(thread);
        return;
    }

    if (!type_check_int(pull) || !type_check_int(mode) || !type_check_int(pin)) {
        vm_error("NATIVE ERROR", "Expected integer arguments for gpio_init");
        set_thread_inactive(thread);
        return;
    }

    hal_gpio_init(
        make_hal_pin(pin.data),
        (hal_gpio_mode_t)mode.data,
        (hal_gpio_pull_t)pull.data
    );
}

void __wrap__gpio_write(VM_Thread* thread) {
    prim_val state, pin;

    if (!pop_stack(thread, &state) || !pop_stack(thread, &pin)) {
        vm_error("NATIVE ERROR", "Pop stack failed for gpio_write parameters");
        set_thread_inactive(thread);
        return;
    }

    if (!type_check_int(state) || !type_check_int(pin)) {
        vm_error("NATIVE ERROR", "Expected integer arguments for gpio_write");
        set_thread_inactive(thread);
        return;
    }

    hal_gpio_write(make_hal_pin(pin.data), (hal_gpio_state_t)state.data);
}

void __wrap__gpio_read(VM_Thread* thread) {
    prim_val pin;

    if (!pop_stack(thread, &pin)) {
        vm_error("NATIVE ERROR", "Pop stack failed for gpio_read parameter");
        set_thread_inactive(thread);
        return;
    }

    if (!type_check_int(pin)) {
        vm_error("NATIVE ERROR", "Expected integer argument for gpio_read");
        set_thread_inactive(thread);
        return;
    }

    hal_gpio_state_t state = hal_gpio_read(make_hal_pin(pin.data));

    // Cast state explicitly to (word) to resolve -Wsign-conversion
    prim_val result = make_prim_val((word)state, STATE_OPEN, TYPE_I32);
    push_stack(thread, result);
}

void __wrap__gpio_set(VM_Thread* thread) {
    prim_val pin;

    if (!pop_stack(thread, &pin) || !type_check_int(pin)) {
        vm_error("NATIVE ERROR", "Invalid argument for gpio_set");
        set_thread_inactive(thread);
        return;
    }

    hal_gpio_set(make_hal_pin(pin.data));
}

void __wrap__gpio_clear(VM_Thread* thread) {
    prim_val pin;

    if (!pop_stack(thread, &pin) || !type_check_int(pin)) {
        vm_error("NATIVE ERROR", "Invalid argument for gpio_clear");
        set_thread_inactive(thread);
        return;
    }

    hal_gpio_clear(make_hal_pin(pin.data));
}

void __wrap__gpio_toggle(VM_Thread* thread) {
    prim_val pin;

    if (!pop_stack(thread, &pin) || !type_check_int(pin)) {
        vm_error("NATIVE ERROR", "Invalid argument for gpio_toggle");
        set_thread_inactive(thread);
        return;
    }

    hal_gpio_toggle(make_hal_pin(pin.data));
}

void register_gpio_wrappers(void){
    native_register("gpio_init",    __wrap__gpio_init);
    native_register("gpio_write",   __wrap__gpio_write);
    native_register("gpio_read",    __wrap__gpio_read);
    native_register("gpio_set",     __wrap__gpio_set);
    native_register("gpio_clear",   __wrap__gpio_clear);
    native_register("gpio_toggle",  __wrap__gpio_toggle);
}
