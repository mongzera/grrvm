#ifndef GRRVM_NATIVE_WRAPPER_GC_H
#define GRRVM_NATIVE_WRAPPER_GC_H

#include "grrvm/vm_thread.h"
void __wrap__gc_run(VM_Thread* thread);

void register_gc_wrappers(void);
#endif
