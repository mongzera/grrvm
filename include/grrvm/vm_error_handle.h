#ifndef GRRVM_VM_ERROR_HANDLE_H
#define GRRVM_VM_ERROR_HANDLE_H

#include "grrvm/vm.h"
#include "grrvm/vm_log.h"

static inline void vm_error_pop_underflow(VM_Thread *thread, char* emitter){
    vm_error("STACK UNDERFLOW", "[%s] Cannot pop on empty stack!", emitter);
    set_thread_inactive(thread);
}

static inline void vm_error_push_overflow(VM_Thread *thread, char* emitter){
    vm_error("STACK OVERFLOW", "[%s] Cannot push on full stack!", emitter);
    set_thread_inactive(thread);
}

#endif /* GRRVM_VM_ERROR_HANDLE_H */
