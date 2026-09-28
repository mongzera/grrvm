#ifndef NATIVE_MATH_WRAPPER_H
#define NATIVE_MATH_WRAPPER_H

#include "grrvm/vm_thread.h"

// Expose the wrapper functions
void wrap_math_abs(VM_Thread* thread);
void wrap_math_max(VM_Thread* thread);

// Optional: A bulk registration function for this specific module
void register_math_wrappers(void);

#endif /* NATIVE_MATH_WRAPPER_H */
