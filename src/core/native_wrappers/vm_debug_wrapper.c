
#include "native_wrappers/vm_debug_wrapper.h"
#include "grrvm/debug/vm_debug.h"
#include "grrvm/vm_log.h"
#include "grrvm/vm_native.h"
#include "grrvm/vm_thread.h"
#include <stdint.h>


void __wrap__vm_stack_debug(VM_Thread* thread){
    prim_val levels;
    if(!pop_stack(thread, &levels)){
        vm_error("NATIVE ERROR", "CANNOT POP VALUE FOR VM_STACK_DEBUG!");
    }
    print_vm_stack(thread, (uint8_t)levels.data);
}


void register_vm_debug_wrappers(void){
    native_register("vm_stack_debug", __wrap__vm_stack_debug);
}
