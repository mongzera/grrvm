#include "native_wrappers/math_wrapper.h"
#include "grrvm/hal/vm_hal_math.h"
#include "grrvm/type_checks.h"
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

/* rand_seed(seed) - fixes the RNG sequence so a workload draws the same
 * block sizes on every run and every runtime. Required for an evaluation
 * that claims reproducibility: the POSIX port otherwise seeds from
 * /dev/urandom at boot, and Pico from hal_rng_init(0). A seed of 0 means
 * "reseed from entropy" and must not be used for measurement. */
void wrap_math_rand_seed(VM_Thread* thread) {
    prim_val seed;

    if(!pop_stack(thread, &seed)){
        vm_error("NATIVE ERROR", "Pop stack failed");
        set_thread_inactive(thread);
        return;
    }

    if(!type_check_int(seed)){
        vm_error("NATIVE ERROR", "rand_seed expects an integer seed");
        set_thread_inactive(thread);
        return;
    }

    if(seed.data == 0){
        vm_error("NATIVE ERROR", "rand_seed(0) reseeds from entropy; use a "
                 "non-zero seed for a reproducible run");
        set_thread_inactive(thread);
        return;
    }

    hal_rng_init((g_u32)seed.data);
}

// Bulk register all functions in this file
void register_math_wrappers(void) {
    native_register("math_rng_range", wrap_math_rng_range);
    native_register("rand_seed", wrap_math_rand_seed);
}
