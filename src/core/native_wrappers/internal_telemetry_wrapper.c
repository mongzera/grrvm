#include "native_wrappers/internal_telemetry_wrapper.h"
#include "grrvm/telemetry/gc_telemetry.h"
#include "grrvm/vm.h"
#include "grrvm/vm_log.h"
#include "grrvm/vm_native.h"
#include "grrvm/vm_thread.h"

void wrap_set_gc_runtime_telemetry_flag(VM_Thread* thread){
    prim_val flag;
    if(!pop_stack(thread, &flag)){
        vm_error("NATIVE", "Failed to pop stack for native gc runtime telemetry flag");
        return;
    }

    if(flag.data) {
        telemetry_flag_set(TELEMETRY_GC_RUNTIME);
    }else{
        telemetry_flag_clear(TELEMETRY_GC_RUNTIME);
    }

    vm_info("RUNTIME TELEMETRY", "Attempting to set GC runtime telemetry flag: %d", flag.data);
}

void wrap_dump_gc_runtime_telemetry(VM_Thread* thread){
    dump_gc_runtime_telemetry();
}

void wrap_set_ext_frag_telemetry_flag(VM_Thread* thread){
    prim_val flag;
    if(!pop_stack(thread, &flag)){
        vm_error("NATIVE", "Failed to pop stack for native external fragmentation telemetry flag");
        return;
    }

    if(flag.data) {
        telemetry_flag_set(TELEMETRY_EXT_FRAG);
    }else{
        telemetry_flag_clear(TELEMETRY_EXT_FRAG);
    }

    vm_info("EXTERNAL FRAG TELEMETRY", "Attempting to set external fragmentation telemetry flag: %d", flag.data);
}

void wrap_dump_ext_frag_telemetry(VM_Thread* thread){
    dump_ext_frag_telemetry();
}

void register_internal_telemetry_wrappers(void) {
    native_register("set_gc_runtime_telemetry_flag", wrap_set_gc_runtime_telemetry_flag);
    native_register("dump_gc_runtime_telemetry", wrap_dump_gc_runtime_telemetry);

    native_register("set_ext_frag_telemetry_flag", wrap_set_ext_frag_telemetry_flag);
    native_register("dump_ext_frag_telemetry", wrap_dump_ext_frag_telemetry);
}
