#include "native_wrappers/gc_wrapper.h"
#include "grrvm/vm_gc.h"
#include "grrvm/vm_native.h"

void __wrap__gc_run(VM_Thread* thread){
    run_gc(thread->vm);
}

void register_gc_wrappers(void){
    native_register("gc_run",__wrap__gc_run);
}
