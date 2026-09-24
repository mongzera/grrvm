#include "grrvm/vm.h"
#include "grrvm/config.h"
#include "grrvm/evaluation.h"
#include "grrvm/hal/vm_hal_timer.h"
#include "grrvm/opcodes.h"
#include "grrvm/vm_log.h"
#include "grrvm/vm_thread.h"
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

void vm_start(VM* vm_instance){
    // initialize system
    // initialize hal
    // initialize vm_instance
    // initialize all threads as inactive except for thread[0]
    for(int i = 1; i < VM_MAX_THREADS; i++){
        set_thread_inactive(&vm_instance->vm_threads[i]);
    }
    vm_new_thread(vm_instance, vm_instance->_program_start, 0);
    uint64_t initial_time = hal_clock_ns();
    vm_loop(vm_instance);
    uint64_t final_time = hal_clock_ns();
    vm_info("PROGRAM EXECUTION", "Initial time: %lu, Final time: %lu, Delta: %lfs", initial_time, final_time, (double)(final_time - initial_time) / 1000000000.0);
}

void vm_loop(VM* vm){
    int consecutive_skip = 0;
    while(1){
        // stop when non of the threads are active
        if(consecutive_skip > VM_MAX_THREADS) break;

        for (int i = 0; i < VM_MAX_THREADS; i++) {
            VM_Thread* thread = &vm->vm_threads[i];

            if(!is_thread_active(thread)){
                consecutive_skip++;
                continue;
            }

            consecutive_skip = 0;
            //uint32_t pc = thread->pc;
            word opcode = get_instruction(thread);
            //vm_info("PROGRAM", "PC: %d, OPCODE: 0x%X", pc, opcode);


            switch (get_opcode_cat(opcode)) {
                case OPC_STACK_OPERAND      : eval_stack_operand(thread, opcode); break;
                case OPC_ARITHMETIC         : eval_arithmetic_operand(thread, opcode); break;
                case OPC_CONDITIONAL        : eval_conditional_operand(thread, opcode); break;
                case OPC_BRANCHING          : eval_branching_operand(thread, opcode); break;
                // case OPC_BITWISE            : eval_bitwise_operand(thread, opcode); break;
                case OPC_MEMORY_LOCAL       : eval_memory_local_operand(thread, opcode); break;
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
