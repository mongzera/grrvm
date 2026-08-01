#include "grrvm/evaluation.h"
#include "grrvm/opcodes.h"
#include "grrvm/type_conditional.h"
#include "grrvm/vm.h"
#include "grrvm/vm_error_handle.h"
#include "grrvm/vm_log.h"
#include "grrvm/vm_thread.h"

void eval_arithmetic_operand(VM_Thread *thread, word opcode){
    switch (opcode) {
        case CMPEQ: {
            prim_val* a = 0;
            prim_val* b = 0;
            prim_val* res = 0;

            if (!pop_stack(thread, b)) {
                vm_error_pop_underflow(thread, "POP");
                return;
            }

            if (!pop_stack(thread, a)) {
                vm_error_pop_underflow(thread, "POP");
                return;
            }

            if(!type_safe_eq(*a, *b, res)){
                vm_error("TYPE PROMOTION", "Data type is non-numeric! CMPEQ");
                return;
            }

            if(!push_stack(thread, *res)){

            }
        } break;
        default: {
            vm_error("ARITHMETIC OPERAND", "Unrecognized arithmetic opcode [0x%X]", opcode);
            set_thread_inactive(thread);
            break;
        }

    }
}
