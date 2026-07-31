#ifndef VM_THREAD_H
#define VM_THREAD_H

#include "grrvm/config.h"
#include "grrvm/types.h"
#include "grrvm/vm.h"
#include <string.h>


static inline int push_stack(VM_Thread *thread, prim_val val) {
    if (thread->sp >= VM_OP_STACK_MAX - 1) {
        return 0; // Stack overflow
    }
    thread->sp++;
    thread->op_stack[thread->sp] = val;
    return 1;
}

static inline int pop_stack(VM_Thread *thread, prim_val *out_val) {
    if (thread->sp < 0) {
        return 0; // Stack underflow
    }
    if (out_val != NULL) {
        *out_val = thread->op_stack[thread->sp];
    }
    thread->sp--;
    return 1;
}

static inline prim_val* get_stack(VM_Thread *thread, int offset) {
    int index = thread->sp - offset;
    if (index < 0 || index >= VM_OP_STACK_MAX) {
        return NULL; // Out of bounds
    }
    return &thread->op_stack[index];
}

static inline word get_instruction(VM_Thread *thread) {
    return thread->vm->program[thread->pc++];
}

static inline void set_instruction(VM_Thread *thread, word pc) {
    thread->pc = pc;
}

#endif
