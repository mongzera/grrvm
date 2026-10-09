#include "grrvm/vm_thread.h"
#include "grrvm/vm.h"
#include "grrvm/vm_log.h"
#include <string.h>

void vm_new_thread(VM *vm, word program_counter, int id){

    if(id < 0 || id >= VM_MAX_THREADS) {
        vm_error("VM THREAD", "Thread ID is invalid!");
        exit(-1);
    }

    VM_Thread* thread = &vm->vm_threads[id];
    memset(thread, 0, sizeof(VM_Thread));
    thread->status = 0;
    thread->n_local_vars = 0;
    thread->sfp = 0;
    thread->sp = -1;
    thread->csp = -1;
    thread->vm = vm;
    thread->pc = program_counter;
    thread->pc_checkpoint = program_counter;

    set_thread_active(thread);
}

int push_stack(VM_Thread *thread, prim_val val) {
    if (thread->sp >= VM_OP_STACK_MAX - 1) {
        return 0; // Stack overflow
    }
    thread->sp++;
    thread->op_stack[thread->sp] = val;
    return 1;
}

int pop_stack(VM_Thread *thread, prim_val *out_val) {
    if (thread->sp < 0) {
        return 0; // Stack underflow
    }

    *out_val = thread->op_stack[thread->sp];
    thread->sp--;
    return 1;
}

prim_val* get_stack(VM_Thread *thread, int offset) {
    int index = thread->sp - offset;
    if (index < 0 || index >= VM_OP_STACK_MAX) {
        vm_error("STACK ERROR", "STACK OUT OF BOUNDS!");
        set_thread_inactive(thread);
        return NULL; // Out of bounds
    }
    return &thread->op_stack[index];
}

int get_local_stack(VM_Thread *thread, int offset, prim_val *out_val) {
    int index = thread->sfp + offset;

    // Fixed bound check: use VM_CALL_STACK_FRAME_MAX instead of VM_OP_STACK_MAX
    // since you are accessing the call_stack_frame array.
    if (index < 0 || index >= VM_CALL_STACK_FRAME_MAX) {
        return 0; // Out of bounds
    }

    if (out_val != NULL) {
        *out_val = thread->call_stack_frame[index];
    }
    return 1;
}

int set_local_stack(VM_Thread *thread, int offset, prim_val val) {
    int index = thread->sfp + offset;

    // Fixed bound check: use VM_CALL_STACK_FRAME_MAX here as well.
    if (index < 0 || index >= VM_CALL_STACK_FRAME_MAX) {
        return 0; // Out of bounds
    }

    thread->call_stack_frame[index] = val;

    // Dynamically expand the known size of the local scope.
    // If you write to offset 2, you have locals 0, 1, and 2 (so n_local_vars becomes 3).
    if (offset >= thread->n_local_vars) {
        thread->n_local_vars = offset + 1;
    }

    return 1;
}

void thread_push_call_stack(VM_Thread *thread, word target_address) {
    // 1. Validate call stack depth
    if (thread->csp >= VM_CALL_STACK_MAX - 1) {
        vm_error("CALL STACK OVERFLOW", "Max execution depth reached!");
        set_thread_inactive(thread);
        return;
    }

    // 2. Validate program bounds BEFORE mutating state
    if (target_address >= thread->vm->program_size) {
        vm_error("BRANCHING OPERAND", "CALL target out of program bounds!");
        set_thread_inactive(thread);
        return;
    }

    // 3. Save state to call stack frame
    thread->csp++;
    thread->call_stack[thread->csp].previous_pc = thread->pc;
    thread->call_stack[thread->csp].previous_sfp = thread->sfp;
    thread->call_stack[thread->csp].previous_sp = thread->sp;
    thread->call_stack[thread->csp].previous_n_local_vars = thread->n_local_vars;

    // 4. Advance Frame Pointer for new function scope
    thread->sfp = thread->sfp + thread->n_local_vars;
    thread->n_local_vars = 0;

    // 5. Transfer control
    thread->pc = (g_int)target_address;
}

void thread_pop_call_stack(VM_Thread *thread) {
    // 1. Guard against Underflow
    if (thread->csp < 0) {
        vm_error("CALL STACK UNDERFLOW", "RET executed outside of a function context!");
        set_thread_inactive(thread);
        return;
    }

    // 2. Fetch caller state
    g_int prev_pc        = thread->call_stack[thread->csp].previous_pc;
    g_int prev_sfp       = thread->call_stack[thread->csp].previous_sfp;
    g_int prev_n_locals  = thread->call_stack[thread->csp].previous_n_local_vars;

    // Pop call frame
    thread->csp--;

    // 3. Restore caller's variable frame tracking
    thread->sfp = prev_sfp;
    thread->n_local_vars = prev_n_locals;

    // 4. Return control to caller
    thread->pc = prev_pc;

    // NOTE: Do NOT forcibly set 'thread->sp = prev_sp;' here if arguments
    // are popped by STORE_L or if values are returned on the operand stack.
    // Let the operand stack naturally manage its depth via PUSH/POP/STORE_L.
}
