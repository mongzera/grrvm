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

        case CALL:

            break;
        case RET:

            break;
        default: {
            vm_error("BRANCHING OPERAND", "Unrecognized branching opcode [0x%X]", opcode);
            set_thread_inactive(thread);
            break;
        }

    }
}
