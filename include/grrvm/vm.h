#ifndef GRRVM_VM_H
#define GRRVM_VM_H

/*
 * Core VM structures (VM, VM_Thread).
 *
 * Include layering (strictly one-way, no cycles):
 *   types.h, default_config.h, vm_log.h, vm_math.h      leaves
 *   prim_val.h                                          value model
 *   vm_global_table.h, vm_alloc.h                       need prim_val only
 *   vm.h                                                needs all of the above
 *   vm_mem.h, vm_thread.h, vm_gc.h, ...                 need the full VM -> include vm.h
 *
 * vm.h must NOT include anything from the last row.
 */

#include "grrvm/types.h"
#include "grrvm/prim_val.h"
#include "grrvm/vm_global_table.h"
#include "grrvm/vm_alloc.h"

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
    struct VM* vm;
    prim_val op_stack[VM_OP_STACK_MAX];
    prim_val call_stack_frame[VM_CALL_STACK_FRAME_MAX];
    VM_CallStack call_stack[VM_CALL_STACK_MAX];

} VM_Thread;


typedef struct VM {
    word _program_start;
    word program_size;
    word program[VM_PROGRAM_MAX_SIZE];
    prim_val ram[VM_HEAP_SLOTS];
    global_var_table global_var_table;
    VM_Allocator allocator;
    VM_Thread vm_threads[VM_MAX_THREADS];
} VM;

void vm_start(VM* vm_instance);
void vm_loop(VM* vm);
void vm_terminate(VM* vm);
void vm_new_thread(VM* vm, word program_counter, int id);

static inline void set_thread_active(VM_Thread* thread) {
    thread->status = THREAD_ACTIVE;
}

static inline void set_thread_inactive(VM_Thread* thread) {
    thread->status = THREAD_INACTIVE;
}

static inline byte is_thread_active(const VM_Thread* thread) {
    return (thread->status == THREAD_ACTIVE);
}

#endif /* GRRVM_VM_H */
