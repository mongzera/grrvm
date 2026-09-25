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
            // If CALL targets an absolute PC address, keep it as 'target'.
            // If it targets a relative offset, do: thread->pc + offset
            word target_address = get_instruction(thread);

            // 2. Guard against Call Stack Overflow
            if (thread->csp >= VM_CALL_STACK_MAX) {
                vm_error("CALL STACK OVERFLOW", "Max execution depth reached!");
                set_thread_inactive(thread);
                return;
            }

            // 3. Save current execution state to the call stack frame
            thread->call_stack[thread->csp].previous_pc = thread->pc;
            thread->call_stack[thread->csp].previous_sfp = thread->sfp;
            thread->csp++;

            // 4. Align the Stack Frame Pointer (sfp) to the current Operand Stack Pointer (sp)
            // This isolates local scopes: local index 0 becomes whatever sits at thread->op_stack[sfp]
            thread->sfp = (word)thread->sp;

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

            // 2. Step back down to the caller's call stack frame frame
            thread->csp--;
            word prev_pc  = thread->call_stack[thread->csp].previous_pc;
            word prev_sfp = thread->call_stack[thread->csp].previous_sfp;

            // 3. Collapse the Operand Stack Frame
            // This immediately frees all evaluation data and local variables allocated by the function
            thread->sp = (g_int)thread->sfp;

            // 4. Restore the caller's frame pointers
            thread->sfp = prev_sfp;
            thread->pc  = prev_pc;
            break;
        }

        default: {
            vm_error("BRANCHING OPERAND", "Unrecognized branching opcode [0x%X]", opcode);
            set_thread_inactive(thread);
            break;
        }

    }
}
