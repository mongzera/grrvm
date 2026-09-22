#include "grrvm/evaluation.h"
#include "grrvm/vm.h"
#include "grrvm/vm_log.h"
#include "grrvm/opcodes.h"
#include "grrvm/vm_thread.h"

void eval_stack_operand(VM_Thread *thread, word opcode) {

    byte has_error = 0;
    switch (opcode) {
        case HALT: {
            set_thread_inactive(thread);
            break;
        }

        case PUSH: {
            word raw_data = get_instruction(thread);
            prim_val val = make_prim_val(raw_data, STATE_OPEN, TYPE_I32);
            if(get_prim_type(val) != TYPE_FLOAT) vm_info("PUSH", "VALUE: %d", val.data);
            else vm_info("PUSH", "VALUE: %f", val.float_data);
            if (!push_stack(thread, val)) {
                vm_error("STACK OVERFLOW", "Thread operand stack limit reached during PUSH!");
                has_error = 1;
            }
            break;
        }

        case PUSH_ADDR: {
            // word raw_data = get_instruction(thread);
            // prim_val val = make_prim_val(raw_data, STATE_OPEN, TYPE_I32);
            // if(get_prim_type(val) != TYPE_FLOAT) vm_info("PUSH", "VALUE: %d", raw_data);
            // else vm_info("PUSH", "VALUE: %f", raw_data);
            // if (!push_stack(thread, val)) {
            //     vm_error("STACK OVERFLOW", "Thread operand stack limit reached during PUSH!");
            //     has_error = 1;
            // }
            // break;

            // NOTE: Unimplemented
            vm_error("Unimplemented", "PUSH_ADDR");
            has_error = 1;
            break;
        }

        case PUSH_T: {
            prim_type data_type = get_instruction(thread);
            word raw_data = get_instruction(thread);
            prim_val val = make_prim_val(raw_data, STATE_OPEN, data_type);
            if(get_prim_type(val) != TYPE_FLOAT) vm_info("PUSH_T", "VALUE: %d", val.data);
            else vm_info("PUSH_T", "FLOAT VALUE: %f", val.float_data);

            if (!push_stack(thread, val)) {
                vm_error("STACK OVERFLOW", "Thread operand stack limit reached during PUSH_T!");
                has_error = 1;
            }

            break;
        }

        case POP: {
            prim_val val;
            if (!pop_stack(thread, &val)) {
                vm_error("STACK UNDERFLOW", "Attempted to POP from an empty stack!");
                has_error = 1;
            }

            if(get_prim_type(val) != TYPE_FLOAT) vm_info("POP", "VALUE: %d", val.data);
            else vm_info("POP", "VALUE: %f", val.float_data);
            break;
        }

        case DUP: {
            prim_val *top = get_stack(thread, 0);
            if (top == NULL) {
                vm_error("STACK UNDERFLOW", "Attempted to DUP on an empty stack!");
                has_error = 1;
                return;
            }

            if (!push_stack(thread, *top)) {
                vm_error("STACK OVERFLOW", "Cannot DUP; operand stack is full!");
                has_error = 1;
            }
            break;
        }

        case SWAP: {
            prim_val *top = get_stack(thread, 0);
            prim_val *second = get_stack(thread, 1);

            if (top == NULL || second == NULL) {
                vm_error("STACK UNDERFLOW", "SWAP requires at least 2 values on the stack!");
                has_error = 1;
                return;
            }

            prim_val temp = *top;
            *top = *second;
            *second = temp;
            break;
        }

        case ROT: {
            prim_val *c = get_stack(thread, 0); // Top
            prim_val *b = get_stack(thread, 1); // Second
            prim_val *a = get_stack(thread, 2); // Third

            if (c == NULL || b == NULL || a == NULL) {
                vm_error("STACK UNDERFLOW", "ROT requires at least 3 values on the stack!");
                has_error = 1;
                return;
            }

            prim_val temp_a = *a;
            *a = *b;
            *b = *c;
            *c = temp_a;
            break;
        }

        default: {
            vm_error("STACK OPERAND", "Unrecognized stack opcode [0x%X]", opcode);
            has_error = 1;
            break;
        }
    }



    if(has_error) set_thread_inactive(thread);
}
