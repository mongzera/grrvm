#ifndef GRRVM_NATIVE_WRAPPERS_INTERNAL_TELEMETRY_WRAPPER_H
#define GRRVM_NATIVE_WRAPPERS_INTERNAL_TELEMETRY_WRAPPER_H

#include "grrvm/vm.h"
#include "grrvm/vm_thread.h"

void wrap_set_gc_runtime_telemetry_flag(VM_Thread* thread);
void wrap_dump_gc_runtime_telemetry(VM_Thread* thread);

void wrap_set_ext_frag_telemetry_flag(VM_Thread* thread);
void wrap_dump_ext_frag_telemetry(VM_Thread* thread);

void __wrap__timer_us(VM_Thread* thread);

/* Telemetry control surface (see grrvm/telemetry/gc_telemetry.h).
 * telemetry_flush is the only native that prints; it is meant to be the
 * last thing a guest program invokes. */
void wrap_telemetry_reset(VM_Thread* thread);
void wrap_telemetry_flush(VM_Thread* thread);
void wrap_bench_alloc(VM_Thread* thread);
void wrap_set_gc_enabled(VM_Thread* thread);

/* full_heap_scan: O(VM_HEAP_SLOTS) layout scan, records one FULLSCAN row
 * and pushes Ef_full (TYPE_FLOAT). See grrvm/vm_alloc.h. */
void wrap_full_heap_scan(VM_Thread* thread);

void register_internal_telemetry_wrappers(void);
#endif /* GRRVM_NATIVE_WRAPPERS_INTERNAL_TELEMETRY_WRAPPER_H */
