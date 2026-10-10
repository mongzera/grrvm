#ifndef GRRVM_VM_THREAD_H
#define GRRVM_VM_THREAD_H

#include "grrvm/types.h"
#include "grrvm/vm.h"

/* vm_new_thread() is declared in vm.h */

int push_stack(VM_Thread *thread, prim_val val);

int pop_stack(VM_Thread *thread, prim_val *out_val);

prim_val* get_stack(VM_Thread *thread, int offset);

int get_local_stack(VM_Thread *thread, int offset, prim_val *out_val);

int set_local_stack(VM_Thread *thread, int offset, prim_val val);

void thread_push_call_stack(VM_Thread *thread, word target_address);

void thread_pop_call_stack(VM_Thread *thread);

static inline word get_instruction(VM_Thread *thread) {
    return thread->vm->program[thread->pc++];
}

static inline void set_instruction(VM_Thread *thread, word pc) {
    thread->pc = pc;
}

#endif /* GRRVM_VM_THREAD_H */
