#include "grrvm/opcodes.h"
#include "grrvm/types.h"
#include "grrvm/vm.h"
#include "grrvm/evaluation.h"
#include "grrvm/vm_thread.h"

void eval_memory_local_operand(VM_Thread *thread, word opcode) {
    switch (opcode) {
        case STORE_L:{
            g_int index = (g_int)(word)get_instruction(thread);
            prim_val val;

            if(!pop_stack(thread, &val)){
                vm_error("MEMORY LOCAL OPERAND", "Failed to pop stack for STORE_L");
                set_thread_inactive(thread);
                break;
            }

            if(!set_local_stack(thread, index, val)){
                vm_error("MEMORY LOCAL OPERAND", "Failed to store local for STORE_L");
                set_thread_inactive(thread);
            }
            break;
        }
        case LOAD_L:{
            g_int index = (g_int)(word)get_instruction(thread);
            prim_val val;
            if(!get_local_stack(thread, index, &val)){
                vm_error("MEMORY LOCAL OPERAND", "Failed to load local for LOAD_L");
                set_thread_inactive(thread);
            }

            if(!push_stack(thread, val)){
                vm_error("MEMORY LOCAL OPERAND", "Failed to push stack for LOAD_L");
                set_thread_inactive(thread);
            }

            break;
        }
        default:
            vm_error("MEMORY LOCAL OPERAND", "Unrecognized memory local opcode [0x%X]", opcode);
            set_thread_inactive(thread);
            break;
    }
}
