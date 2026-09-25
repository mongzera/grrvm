#include "grrvm/vm_thread.h"

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
        return NULL; // Out of bounds
    }
    return &thread->op_stack[index];
}

int get_local_stack(VM_Thread *thread, int offset, prim_val *out_val) {
    int index = thread->sfp + offset;
    if (index < 0 || index >= VM_OP_STACK_MAX) {
        return 0; // Out of bounds
    }
    if (out_val != NULL) {
        *out_val = thread->call_stack_frame[index];
    }
    return 1;
}

int set_local_stack(VM_Thread *thread, int offset, prim_val val) {
    int index = thread->sfp + offset;
    if (index < 0 || index >= VM_OP_STACK_MAX) {
        return 0; // Out of bounds
    }
    thread->call_stack_frame[index] = val;
    return 1;
}
