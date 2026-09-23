#include "grrvm/opcodes.h"
#include "grrvm/vm.h"
#include "grrvm/vm_thread.h"
#include "grrvm/vm_log.h"
#include "grrvm/evaluation.h"
#include "grrvm/type_arithmetic.h"

void eval_arithmetic_operand(VM_Thread *thread, word opcode) {

    switch (opcode) {
        case ADD: {
            prim_val b, a, res;
            if (!pop_stack(thread, &b) || !pop_stack(thread, &a)) {
                vm_error("STACK UNDERFLOW", "ADD requires at least 2 values on the stack!");
                set_thread_inactive(thread);
                return;
            }
            if (!type_safe_add(a, b, &res)) {
                vm_error("TYPE ERROR", "Invalid operands for ADD operation!");
                set_thread_inactive(thread);
                return;
            }
            push_stack(thread, res);
            break;
        }

        case SUB: {
            prim_val b, a, res;
            if (!pop_stack(thread, &b) || !pop_stack(thread, &a)) {
                vm_error("STACK UNDERFLOW", "SUB requires at least 2 values on the stack!");
                set_thread_inactive(thread);
                return;
            }
            if (!type_safe_sub(a, b, &res)) {
                vm_error("TYPE ERROR", "Invalid operands for SUB operation!");
                set_thread_inactive(thread);
                return;
            }
            push_stack(thread, res);
            break;
        }

        case MUL: {
            prim_val b, a, res;
            if (!pop_stack(thread, &b) || !pop_stack(thread, &a)) {
                vm_error("STACK UNDERFLOW", "MUL requires at least 2 values on the stack!");
                set_thread_inactive(thread);
                return;
            }
            if (!type_safe_mul(a, b, &res)) {
                vm_error("TYPE ERROR", "Invalid operands for MUL operation!");
                set_thread_inactive(thread);
                return;
            }
            push_stack(thread, res);
            break;
        }

        case DIV: {
            prim_val b, a, res;
            int div_by_zero = 0;
            if (!pop_stack(thread, &b) || !pop_stack(thread, &a)) {
                vm_error("STACK UNDERFLOW", "DIV requires at least 2 values on the stack!");
                set_thread_inactive(thread);
                return;
            }
            if (!type_safe_div(a, b, &res, &div_by_zero)) {
                if (div_by_zero) {
                    vm_error("DIVISION BY ZERO", "Attempted to divide by zero!");
                } else {
                    vm_error("TYPE ERROR", "Invalid operands for DIV operation!");
                }
                set_thread_inactive(thread);
                return;
            }
            push_stack(thread, res);
            break;
        }

        case MOD: {
            prim_val b, a, res;
            int div_by_zero = 0;
            if (!pop_stack(thread, &b) || !pop_stack(thread, &a)) {
                vm_error("STACK UNDERFLOW", "MOD requires at least 2 values on the stack!");
                set_thread_inactive(thread);
                return;
            }
            if (!type_safe_mod(a, b, &res, &div_by_zero)) {
                if (div_by_zero) {
                    vm_error("DIVISION BY ZERO", "Attempted modulo by zero!");
                } else {
                    vm_error("TYPE ERROR", "Invalid operands or FLOAT type used for MOD!");
                }
                set_thread_inactive(thread);
                return;
            }
            push_stack(thread, res);
            break;
        }

        case INC: {
            g_i32 amount = (g_i32)(word)get_instruction(thread);

            prim_val *top = get_stack(thread, 0);
            if (top == NULL) {
                vm_error("STACK UNDERFLOW", "INC requires at least 1 value on the stack!");
                set_thread_inactive(thread);
                return;
            }

            // Create a primitive value holding the instruction argument.
            // Using TYPE_I32 ensures negative offsets/amounts are preserved correctly.
            prim_val step = make_prim_val(amount, STATE_OPEN, TYPE_I32);
            prim_val res;

            if (!type_safe_add(*top, step, &res)) {
                vm_error("TYPE ERROR", "Cannot INC non-numeric stack top!");
                set_thread_inactive(thread);
                return;
            }

            *top = res;
            break;
        }

        case DEC: {
            g_i32 amount = (g_i32)(word)get_instruction(thread);

            prim_val *top = get_stack(thread, 0);
            if (top == NULL) {
                vm_error("STACK UNDERFLOW", "DEC requires at least 1 value on the stack!");
                set_thread_inactive(thread);
                return;
            }

            prim_val step = make_prim_val(amount, STATE_OPEN, TYPE_I32);
            prim_val res;

            if (!type_safe_sub(*top, step, &res)) {
                vm_error("TYPE ERROR", "Cannot DEC non-numeric stack top!");
                set_thread_inactive(thread);
                return;
            }

            *top = res;
            break;
        }

        default: {
            vm_error("ARITHMETIC OPERAND", "Unrecognized arithmetic opcode [0x%X]", opcode);
            set_thread_inactive(thread);
            break;
        }
    }
}
