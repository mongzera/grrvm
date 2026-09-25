#include "grrvm/evaluation.h"
#include "grrvm/opcodes.h"
#include "grrvm/type_conditional.h"
#include "grrvm/types.h"
#include "grrvm/vm.h"
#include "grrvm/vm_error_handle.h"
#include "grrvm/vm_log.h"
#include "grrvm/vm_thread.h"

byte check_offset_limit(g_i32 instruction_offset, VM_Thread *thread) {
    if (instruction_offset < 0) {
        uint32_t abs_offset = -(uint32_t)instruction_offset;

        if (abs_offset > thread->pc) {
            vm_error("BRANCHING OPERAND", "Program Counter underflow!");
            set_thread_inactive(thread);
            return 0;
        }
    } else {
        uint32_t offset = (uint32_t)instruction_offset;

        // Check if jumping forward exceeds program boundary
        if (thread->pc + offset > thread->vm->program_size) {
            vm_error("BRANCHING OPERAND", "Program Counter overflow!");
            set_thread_inactive(thread);
            return 0;
        }
    }
    return 1;
}

void eval_branching_operand(VM_Thread *thread, word opcode){

    prim_val zero = {.data = 0};
    set_val_type(&zero, TYPE_U32);

    switch (opcode) {
        case JUMP: {
            g_i32 instruction_offset = (g_i32)(word)get_instruction(thread);
            if (!check_offset_limit(instruction_offset, thread)) return;

            thread->pc += instruction_offset;
            break;
        }
        case JZ: {
            g_i32 instruction_offset = (g_i32)(word)get_instruction(thread);
            prim_val top;

            if (!pop_stack(thread, &top)) {
                vm_error_pop_underflow(thread, "POP");
                return;
            }

            prim_val result;
            if(!type_safe_eq(top, zero, &result)){
                vm_error("TYPE PROMOTION", "Data type is non-numeric! JZ");
                set_thread_inactive(thread);
                break;
            }

            if(result.data == 0) break;

            if (!check_offset_limit(instruction_offset, thread)) return;

            thread->pc += instruction_offset;
            break;
        }
        case JNZ: {
            g_i32 instruction_offset = (g_i32)(word)get_instruction(thread);
            prim_val top;
            if (!pop_stack(thread, &top)) {
                vm_error_pop_underflow(thread, "POP");
                return;
            }

            prim_val result;
            if(!type_safe_neq(top, zero, &result)){
                vm_error("TYPE PROMOTION", "Data type is non-numeric! JNZ");
                set_thread_inactive(thread);
                break;
            }
            //vm_info("JNZ", "Result: %d", result.data);
            if(result.data == 0) break;

            if (!check_offset_limit(instruction_offset, thread)) return;

            thread->pc += instruction_offset;
            break;
        }

        case CALL: {
            // 1. Fetch the target address/offset from the instruction stream
            word target_address = get_instruction(thread);

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
            break;
        }

        case RET: {
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
            break;
        }


        default: {
            vm_error("BRANCHING OPERAND", "Unrecognized branching opcode [0x%X]", opcode);
            set_thread_inactive(thread);
            break;
        }

    }
}
