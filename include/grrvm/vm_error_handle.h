#ifndef VM_ERROR_HANDLE
#define VM_ERROR_HANDLE

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

#endif
