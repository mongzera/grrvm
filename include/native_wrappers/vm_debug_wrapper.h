#ifndef GRRVM_VM_DEBUG_WRAPPER_H
#define GRRVM_VM_DEBUG_WRAPPER_H

#include "grrvm/vm_thread.h"

// Expose the wrapper functions
void __wrap__vm_stack_debug(VM_Thread* thread);


// Optional: A bulk registration function for this specific module
void register_vm_debug_wrappers(void);

#endif /* GRRVM_VM_DEBUG_WRAPPER_H */
