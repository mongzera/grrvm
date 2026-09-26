#include "grrvm/evaluation.h"
#include "grrvm/opcodes.h"
#include "grrvm/type_conditional.h"
#include "grrvm/vm.h"
#include "grrvm/vm_error_handle.h"
#include "grrvm/vm_log.h"
#include "grrvm/vm_thread.h"

void eval_conditional_operand(VM_Thread *thread, word opcode){
    switch (opcode) {
        case CMPEQ: {
            prim_val a, b, res;

            if (!pop_stack(thread, &b)) {
                vm_error_pop_underflow(thread, "CMPEQ");
                return;
            }

            if (!pop_stack(thread, &a)) {
                vm_error_pop_underflow(thread, "CMPEQ");
                return;
            }

            if(!type_safe_eq(a, b, &res)){
                vm_error("TYPE PROMOTION", "Data type is non-numeric! CMPEQ");
                return;
            }

            if(!push_stack(thread, res)){
                vm_error("STACK OVERFLOW", "Stack overflow! CMPEQ");
                return;
            }
        } break;
        case CMPNEQ: {
            prim_val a, b, res;

            if (!pop_stack(thread, &b)) {
                vm_error_pop_underflow(thread, "CMPNEQ");
                return;
            }

            if (!pop_stack(thread, &a)) {
                vm_error_pop_underflow(thread, "CMPNEQ");
                return;
            }

            if(!type_safe_neq(a, b, &res)){
                vm_error("TYPE PROMOTION", "Data type is non-numeric! CMPNEQ");
                return;
            }

            if(!push_stack(thread, res)){
                vm_error("STACK OVERFLOW", "Stack overflow! CMPNEQ");
                return;
            }
        } break;
        case CMPLT: {
            prim_val a, b, res;

            if (!pop_stack(thread, &b)) {
                vm_error_pop_underflow(thread, "CMPLT");
                return;
            }

            if (!pop_stack(thread, &a)) {
                vm_error_pop_underflow(thread, "CMPLT");
                return;
            }

            if(!type_safe_lt(a, b, &res)){
                vm_error("TYPE PROMOTION", "Data type is non-numeric! CMPLT");
                return;
            }

            if(!push_stack(thread, res)){
                vm_error("STACK OVERFLOW", "Stack overflow! CMPLT");
                return;
            }
        } break;
        case CMPLTE: {
            prim_val a, b, res;

            if (!pop_stack(thread, &b)) {
                vm_error_pop_underflow(thread, "CMPLTE");
                return;
            }

            if (!pop_stack(thread, &a)) {
                vm_error_pop_underflow(thread, "CMPLTE");
                return;
            }

            if(!type_safe_lte(a, b, &res)){
                vm_error("TYPE PROMOTION", "Data type is non-numeric! CMPLTE");
                return;
            }

            if(!push_stack(thread, res)){
                vm_error("STACK OVERFLOW", "Stack overflow! CMPLTE");
                return;
            }
        } break;
        case CMPGT: {
            prim_val a, b, res;

            if (!pop_stack(thread, &b)) {
                vm_error_pop_underflow(thread, "CMPGT");
                return;
            }

            if (!pop_stack(thread, &a)) {
                vm_error_pop_underflow(thread, "CMPGT");
                return;
            }

            if(!type_safe_gt(a, b, &res)){
                vm_error("TYPE PROMOTION", "Data type is non-numeric! CMPGT");
                return;
            }

            if(!push_stack(thread, res)){
                vm_error("STACK OVERFLOW", "Stack overflow! CMPGT");
                return;
            }
        } break;
        case CMPGTE: {
            prim_val a, b, res;

            if (!pop_stack(thread, &b)) {
                vm_error_pop_underflow(thread, "CMPGTE");
                return;
            }

            if (!pop_stack(thread, &a)) {
                vm_error_pop_underflow(thread, "CMPGTE");
                return;
            }

            if(!type_safe_gte(a, b, &res)){
                vm_error("TYPE PROMOTION", "Data type is non-numeric! CMPGTE");
                return;
            }

            if(!push_stack(thread, res)){
                vm_error("STACK OVERFLOW", "Stack overflow! CMPGTE");
                return;
            }
        } break;
        default: {
            vm_error("CONDITIONAL OPERAND", "Unrecognized conditional opcode [0x%X]", opcode);
            set_thread_inactive(thread);
            break;
        }

    }
}
