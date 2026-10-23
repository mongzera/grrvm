#ifndef GRRVM_VM_THREAD_H
#define GRRVM_VM_THREAD_H

#include "grr_port_config.h"
#include "grrvm/prim_val.h"
#include "grrvm/types.h"
#include "grrvm/vm.h"


typedef struct VM_CallStack {
    g_int previous_pc;
    g_int previous_sfp;
    g_int previous_sp;           // Added: to track the caller's micro-op stack base
    g_int previous_n_local_vars; // Added: to remember how many locals the caller had
} VM_CallStack;

typedef enum {
    THREAD_INACTIVE = 0x0,
    THREAD_ACTIVE,
    THREAD_WAIT
} thread_status;

typedef struct VM_Thread{
    thread_status status;
    g_int pc;
    g_int pc_checkpoint;
    g_int sp;
    g_int csp;
    g_int sfp;
    g_int n_local_vars;
    VM* vm;
    prim_val op_stack[VM_OP_STACK_MAX];
    prim_val call_stack_frame[VM_CALL_STACK_FRAME_MAX];
    VM_CallStack call_stack[VM_CALL_STACK_MAX];

} VM_Thread;

static inline void set_thread_active(VM_Thread* thread) {
    thread->status = THREAD_ACTIVE;
}

static inline void set_thread_inactive(VM_Thread* thread) {
    thread->status = THREAD_INACTIVE;
}

static inline byte is_thread_active(const VM_Thread* thread) {
    return (thread->status == THREAD_ACTIVE);
}

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
