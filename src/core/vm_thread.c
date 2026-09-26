#include "grrvm/vm_thread.h"
#include "grrvm/vm.h"
#include "grrvm/vm_log.h"

void vm_new_thread(VM *vm, word program_counter, int id){

    if(id < 0 || id >= VM_MAX_THREADS) {
        vm_error("VM THREAD", "Thread ID is invalid!");
        exit(-1);
    }

    VM_Thread* thread = &vm->vm_threads[id];
    thread->status = 0;
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
    if (out_val != NULL) {
        *out_val = thread->op_stack[thread->sp];
    }
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
    // 2. Guard against Call Stack Overflow
    if (thread->csp >= VM_CALL_STACK_MAX - 1) {
        vm_error("CALL STACK OVERFLOW", "Max execution depth reached!");
        set_thread_inactive(thread);
        return;
    }

    // 3. Save current execution state to the call stack frame
    thread->csp++;
    thread->call_stack[thread->csp].previous_pc = thread->pc;
    thread->call_stack[thread->csp].previous_sfp = thread->sfp;

    // Save the new state tracking variables
    thread->call_stack[thread->csp].previous_sp = thread->sp;
    thread->call_stack[thread->csp].previous_n_local_vars = thread->n_local_vars;

    // 4. Advance the Stack Frame Pointer (sfp)
    // By adding n_local_vars to sfp, the new function's local variables
    // will start exactly where the caller's variables end, preventing overwrites.
    thread->sfp = thread->sfp + thread->n_local_vars;

    // Reset n_local_vars to 0 for the new scope
    thread->n_local_vars = 0;

    // 5. Check absolute program bounds before jumping
    if (target_address >= thread->vm->program_size) {
        vm_error("BRANCHING OPERAND", "CALL target out of program bounds!");
        set_thread_inactive(thread);
        return;
    }

    // 6. Transfer control to the target function
    thread->pc = target_address;
}

void thread_pop_call_stack(VM_Thread *thread){
    // 1. Guard against Call Stack Underflow
    if (thread->csp < 0) {
        vm_error("CALL STACK UNDERFLOW", "RET executed outside of a function context!");
        set_thread_inactive(thread);
        return;
    }

    // 2. Fetch the caller's state from the call stack
    g_int prev_pc       = thread->call_stack[thread->csp].previous_pc;
    g_int prev_sfp      = thread->call_stack[thread->csp].previous_sfp;
    g_int prev_sp       = thread->call_stack[thread->csp].previous_sp;
    g_int prev_n_locals = thread->call_stack[thread->csp].previous_n_local_vars;

    // Step back down to the caller's stack frame
    thread->csp--;

    // 3. Collapse the Operand Stack (sp)
    // This safely clears out all micro-operations (math, evaluation data)
    // allocated by the function that just finished.
    thread->sp = prev_sp;

    // 4. Restore the caller's local variable tracking
    thread->sfp = prev_sfp;
    thread->n_local_vars = prev_n_locals;

    // 5. Return to the caller's instruction address
    thread->pc = prev_pc;
}
