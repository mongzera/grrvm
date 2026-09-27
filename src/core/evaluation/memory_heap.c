#include "grrvm/evaluation.h""
#include "grrvm/type_conditional.h"
#include "grrvm/vm.h"
#include "grrvm/type_checks.h"
#include "grrvm/vm_alloc.h"
#include "grrvm/vm_thread.h"
#include "grrvm/vm_log.h"
#include "grrvm/opcodes.h"

void eval_memory_heap_operand(VM_Thread *thread, word opcode){

    switch (opcode) {
        case H_ALLOC:{
            prim_val n_slots;

            if(!pop_stack(thread, &n_slots)){
                vm_error("MEMORY HEAP OPERAND", "Failed to pop stack for H_ALLOC");
                set_thread_inactive(thread);
                break;
            }

            if(!type_check_non_negative(n_slots) || n_slots.data == 0){
                vm_error("MEMORY HEAP OPERAND", "Expected positive integer for H_ALLOC");
                set_thread_inactive(thread);
                break;
            }

            prim_val reference = g_malloc(thread, n_slots.data);

            if(get_prim_state(reference) == STATE_GARBAGE){
                vm_error("MEMORY HEAP OPERAND", "Failed to allocate memory for H_ALLOC");
                set_thread_inactive(thread);
                break;
            }

            if(!push_stack(thread, reference)){
                vm_error("MEMORY HEAP OPERAND", "Failed to push stack for H_ALLOC");
                set_thread_inactive(thread);
                break;
            }

            vm_info("MEMORY HEAP OPERAND", "[n_slots=%d, address=%d]", n_slots.data, reference.data);
            break;
        }

        case H_FREE:{
            prim_val reference;

            if(!pop_stack(thread, &reference)){
                vm_error("MEMORY HEAP OPERAND", "Failed to pop stack for H_FREE");
                set_thread_inactive(thread);
                break;
            }

            if(!type_check_non_negative(reference) && reference.data != 0){
                vm_error("MEMORY HEAP OPERAND", "Expected positive integer for H_FREE");
                set_thread_inactive(thread);
                break;
            }

            g_free(thread, reference);
            break;
        }

        default:
            vm_error("MEMORY HEAP OPERAND", "Unrecognized memory heap opcode [0x%X]", opcode);
            set_thread_inactive(thread);
            break;
    }
}
