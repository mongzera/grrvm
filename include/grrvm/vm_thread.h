#ifndef VM_THREAD_H
#define VM_THREAD_H

#include "types.h"
#include "vm.h"


void vm_new_thread(VM *vm, word program_counter, int id);


int push_stack(VM_Thread *thread, prim_val val);

int pop_stack(VM_Thread *thread, prim_val *out_val);

prim_val* get_stack(VM_Thread *thread, int offset);

int get_local_stack(VM_Thread *thread, int offset, prim_val *out_val);

int set_local_stack(VM_Thread *thread, int offset, prim_val val);

static inline word get_instruction(VM_Thread *thread) {
    return thread->vm->program[thread->pc++];
}

static inline void set_instruction(VM_Thread *thread, word pc) {
    thread->pc = pc;
}

#endif
