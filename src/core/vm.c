#include "grrvm/vm.h"
#include "grrvm/config.h"
#include "grrvm/evaluation.h"
#include "grrvm/opcodes.h"
#include "grrvm/vm_log.h"
#include "grrvm/vm_thread.h"
#include <stdlib.h>

void vm_start(VM* vm_instance){
    // initialize system
    // initialize hal
    // initialize vm_instance
    vm_loop(vm_instance);

}

void vm_loop(VM* vm){
    while(1){
        for (int i = 0; i < VM_MAX_THREADS; i++) {
            VM_Thread* thread = &vm->vm_threads[i];

            if(!is_thread_active(thread)) continue;

            word opcode = get_instruction(thread);

            switch (get_opcode_cat(opcode)) {
                case OPC_STACK_OPERAND      : eval_stack_operand(thread, opcode); break;
                case OPC_ARITHMETIC         : eval_arithmetic_operand(thread, opcode); break;
                case OPC_CONDITIONAL        : eval_conditional_operand(thread, opcode); break;
                // case OPC_BRANCHING          : eval_branching_operand(thread, opcode); break;
                // case OPC_BITWISE            : eval_bitwise_operand(thread, opcode); break;
                // case OPC_MEMORY_LOCAL       : eval_memory_local_operand(thread, opcode); break;
                // case OPC_MEMORY_GLOBAL      : eval_memory_global_operand(thread, opcode); break;
                // case OPC_MEMORY_HEAP        : eval_memory_heap_operand(thread, opcode); break;
                default:{
                    vm_error("OPCODE INVALID", "Cannot recognize [0x%X] as opcode!", opcode);
                    set_thread_inactive(thread);
                    break;
                }
            }
        }
    }
}

void vm_terminate(VM* vm){
    // free all, not just this
    free(vm);
}
