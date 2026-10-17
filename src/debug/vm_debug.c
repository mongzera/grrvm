#include "grrvm/debug/vm_debug.h"

void print_vm_stack(VM_Thread *thread, uint8_t levels) {
    if (!thread) {
        printf("[VM Debug] Thread is NULL\n");
        return;
    }

    printf("======================================================== VM STACK DEBUG ========================================================\n");
    printf("Status: %d | PC: %ld | SP: %ld | CSP: %ld | SFP: %ld | Local Vars: %ld\n",
           thread->status, (long)thread->pc, (long)thread->sp,
           (long)thread->csp, (long)thread->sfp, (long)thread->n_local_vars);
    printf("--------------------------------------------------------------------------------------------------------------------------------\n");
    printf("%-30s | %-95s\n", "OPERAND STACK (op_stack)", "CALL STACK (call_stack frames)");
    printf("%-30s | %-95s\n", "------------------------------", "---------------------------------------------------------------------------------------------");

    // Determine max rows to iterate based on levels, SP, and CSP
    int max_rows = levels;
    int op_max = thread->sp + 1;
    int call_max = thread->csp;
    if (op_max > max_rows) max_rows = op_max;
    if (call_max > max_rows) max_rows = call_max;
    if (max_rows > levels && levels > 0) max_rows = levels;

    for (int i = 0; i < max_rows; i++) {
        // Format Operand Stack Side (printed from top downwards: sp - i)
        char op_buf[64] = "";
        int op_idx = thread->sp - i;
        if (op_idx >= 0 && op_idx <= thread->sp && op_idx < VM_OP_STACK_MAX) {
            snprintf(op_buf, sizeof(op_buf), "[%3d] data: %lu (f: %.2f) | m: %u",
                     op_idx,
                     (unsigned long)thread->op_stack[op_idx].data,
                     thread->op_stack[op_idx].float_data,
                     (unsigned int)thread->op_stack[op_idx].metadata);
        } else {
            snprintf(op_buf, sizeof(op_buf), "%s", "");
        }

        // Format Call Stack Side (printed from top/recent frame downwards: csp - 1 - i)
        char call_buf[128] = "";
        long call_idx = (thread->csp - 1) - i;
        if (thread->csp > 0 && call_idx >= 0 && call_idx < thread->csp && call_idx < VM_CALL_STACK_MAX) {
            VM_CallStack *frame = &thread->call_stack[call_idx];
            snprintf(call_buf, sizeof(call_buf), "[Fr %ld] prev_pc: %ld | prev_sfp: %ld | prev_sp: %ld | prev_loc: %ld",
                     call_idx,
                     (long)frame->previous_pc,
                     (long)frame->previous_sfp,
                     (long)frame->previous_sp,
                     (long)frame->previous_n_local_vars);
        } else {
            snprintf(call_buf, sizeof(call_buf), "%s", "");
        }

        printf("%-30s | %-95s\n", op_buf, call_buf);
    }

    printf("================================================================================================================================\n");
}
