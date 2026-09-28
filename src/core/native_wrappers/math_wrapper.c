#include "native_wrappers/math_wrapper.h"
#include "grrvm/hal/vm_hal_math.h"
#include "grrvm/type_checks.h"
#include "grrvm/types.h"
#include "grrvm/vm.h"
#include "grrvm/vm_log.h"
#include "grrvm/vm_native.h"
#include "grrvm/vm_thread.h"

void wrap_math_rng_range(VM_Thread* thread) {
    prim_val min, max;

    if(!pop_stack(thread, &max)){
        vm_error("NATIVE ERROR", "Pop stack failed");
        set_thread_inactive(thread);
        return;
    }

    if(!pop_stack(thread, &min)){
        vm_error("NATIVE ERROR", "Pop stack failed");
        set_thread_inactive(thread);
        return;
    }

    if(!type_check_int(min) || !type_check_int(max)){
        vm_error("NATIVE ERROR", "Expected integer arguments");
        set_thread_inactive(thread);
        return;
    }

    prim_val result = make_prim_val(hal_rng_range_i32(min.data, max.data), STATE_OPEN, TYPE_I32);
    push_stack(thread, result);

}

// Bulk register all functions in this file
void register_math_wrappers(void) {
    native_register("math_rng_range", wrap_math_rng_range);
}
