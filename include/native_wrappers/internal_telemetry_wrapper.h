#ifndef GRRVM_NATIVE_WRAPPERS_INTERNAL_TELEMETRY_WRAPPER_H
#define GRRVM_NATIVE_WRAPPERS_INTERNAL_TELEMETRY_WRAPPER_H

#include "grrvm/vm.h"
#include "grrvm/vm_thread.h"

void wrap_set_gc_runtime_telemetry_flag(VM_Thread* thread);
void wrap_dump_gc_runtime_telemetry(VM_Thread* thread);

void wrap_set_ext_frag_telemetry_flag(VM_Thread* thread);
void wrap_dump_ext_frag_telemetry(VM_Thread* thread);

void __wrap__timer_us(VM_Thread* thread);

void register_internal_telemetry_wrappers(void);
#endif /* GRRVM_NATIVE_WRAPPERS_INTERNAL_TELEMETRY_WRAPPER_H */
