#include "grrvm/evaluation.h"
#include "grrvm/opcodes.h"
#include "grrvm/type_checks.h"
#include "grrvm/vm.h"
#include "grrvm/vm_thread.h"
void eval_bitwise_operand(VM_Thread *thread, word opcode){
    switch (opcode) {
        case NOT:{
            prim_val *a;

            a = get_stack(thread, 0);
            if(!a) return;


            // check if a & b are integers
            if(!type_check_int(*a)){
                vm_error("TYPE ERROR", "NOT operand must be an integer!");
                set_thread_inactive(thread);
                return;
            }

            a->data = ~a->data;

            break;
        }

        case AND:{
            prim_val b, a;

            if(!pop_stack(thread, &b)){
                vm_error("STACK UNDERFLOW", "AND operand b not found on stack!");
                set_thread_inactive(thread);
                return;
            }
            if(!pop_stack(thread, &a)){
                vm_error("STACK UNDERFLOW", "AND operand a not found on stack!");
                set_thread_inactive(thread);
                return;
            }

            // check if a & b are integers
            if(!type_check_int(a) || !type_check_int(b)){
                vm_error("TYPE ERROR", "AND operands must be integers!");
                set_thread_inactive(thread);
                return;
            }

            prim_val res = make_prim_val(a.data & b.data, STATE_OPEN, TYPE_U32);
            if(!push_stack(thread, res)){
                vm_error("STACK OVERFLOW", "AND result not pushed to stack!");
                set_thread_inactive(thread);
                return;
            }
            break;
        }

        case OR:{
            prim_val b, a;

            if(!pop_stack(thread, &b)){
                vm_error("STACK UNDERFLOW", "OR operand b not found on stack!");
                set_thread_inactive(thread);
                return;
            }
            if(!pop_stack(thread, &a)){
                vm_error("STACK UNDERFLOW", "OR operand a not found on stack!");
                set_thread_inactive(thread);
                return;
            }

            // check if a & b are integers
            if(!type_check_int(a) || !type_check_int(b)){
                vm_error("TYPE ERROR", "AND operands must be integers!");
                set_thread_inactive(thread);
                return;
            }

            prim_val res = make_prim_val(a.data | b.data, STATE_OPEN, TYPE_U32);
            if(!push_stack(thread, res)){
                vm_error("STACK OVERFLOW", "OR result not pushed to stack!");
                set_thread_inactive(thread);
                return;
            }
            break;
        }

        case XOR:{
            prim_val b, a;

            if(!pop_stack(thread, &b)){
                vm_error("STACK UNDERFLOW", "XOR operand b not found on stack!");
                set_thread_inactive(thread);
                return;
            }
            if(!pop_stack(thread, &a)){
                vm_error("STACK UNDERFLOW", "XOR operand a not found on stack!");
                set_thread_inactive(thread);
                return;
            }

            // check if a & b are integers
            if(!type_check_int(a) || !type_check_int(b)){
                vm_error("TYPE ERROR", "AND operands must be integers!");
                set_thread_inactive(thread);
                return;
            }

            prim_val res = make_prim_val(a.data ^ b.data, STATE_OPEN, TYPE_U32);
            if(!push_stack(thread, res)){
                vm_error("STACK OVERFLOW", "XOR result not pushed to stack!");
                set_thread_inactive(thread);
                return;
            }
            break;
        }

        case LSHIFT:{
            prim_val b, a;

            if(!pop_stack(thread, &b)){
                vm_error("STACK UNDERFLOW", "XOR operand b not found on stack!");
                set_thread_inactive(thread);
                return;
            }
            if(!pop_stack(thread, &a)){
                vm_error("STACK UNDERFLOW", "XOR operand a not found on stack!");
                set_thread_inactive(thread);
                return;
            }

            // check if a & b are integers
            if(!type_check_int(a) || !type_check_int(b)){
                vm_error("TYPE ERROR", "AND operands must be integers!");
                set_thread_inactive(thread);
                return;
            }

            prim_val res = make_prim_val(a.data << b.data, STATE_OPEN, TYPE_U32);
            if(!push_stack(thread, res)){
                vm_error("STACK OVERFLOW", "XOR result not pushed to stack!");
                set_thread_inactive(thread);
                return;
            }
            break;
        }

        case RSHIFT:{
            prim_val b, a;

            if(!pop_stack(thread, &b)){
                vm_error("STACK UNDERFLOW", "XOR operand b not found on stack!");
                set_thread_inactive(thread);
                return;
            }
            if(!pop_stack(thread, &a)){
                vm_error("STACK UNDERFLOW", "XOR operand a not found on stack!");
                set_thread_inactive(thread);
                return;
            }

            // check if a & b are integers
            if(!type_check_int(a) || !type_check_int(b)){
                vm_error("TYPE ERROR", "AND operands must be integers!");
                set_thread_inactive(thread);
                return;
            }

            prim_val res = make_prim_val(a.data >> b.data, STATE_OPEN, TYPE_U32);
            if(!push_stack(thread, res)){
                vm_error("STACK OVERFLOW", "XOR result not pushed to stack!");
                set_thread_inactive(thread);
                return;
            }
            break;
        }

        default:{
            vm_error("INVALID OPCODE", "Unknown bitwise operation!");
            set_thread_inactive(thread);
            return;
        }
    }
}
