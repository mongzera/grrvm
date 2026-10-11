#include "grrvm/opcodes.h"
#include "grrvm/prim_val.h"
#include "grrvm/vm_global_table.h"
#include "grrvm/vm_log.h"
#include "grrvm/vm_mem.h"
#include "grrvm/vm_thread.h"
#include "grrvm/vm.h"
#include <assert.h>

int has_global_heap(VM *vm){
    return vm->global_heap_block_address >= 0;
}

prim_state store_global(VM* vm, word global_var_id, word offset, prim_val value){
    assert(vm != NULL);
    prim_val reference = vm->global_var_table.rows[global_var_id].address;
    return set_vm_mem(vm, reference.data, offset, value);
}

int load_global(VM* vm, word global_var_id, word offset, prim_val *out){
    assert(vm != NULL);
    prim_val reference = vm->global_var_table.rows[global_var_id].address;
    return get_vm_mem(vm, reference.data + offset, out);
}

void eval_memory_global_operand (VM_Thread* thread, word opcode){
    VM* vm = thread->vm;
    switch (opcode) {
        case STORE:{

            assert(has_global_heap(vm));
            word global_var_id = get_instruction(thread);

            prim_val val;
            if(!pop_stack(thread, &val)){
                vm_error("GLOBAL STORE", "Failed to pop stack for STORE!");
                set_thread_inactive(thread);
            }

            if(store_global(thread->vm, global_var_id, 0, val) == STATE_ERROR){
                vm_error("GLOBAL STORE", "Failed to store prim_val!");
                set_thread_inactive(thread);
            }

            break;
        }

        case LOAD:{
            assert(has_global_heap(thread->vm));

            word global_var_id = get_instruction(thread);
            prim_val out;

            if(!load_global(vm, global_var_id, 0, &out)){
                vm_error("GLOBAL STORE", "Failed to load prim_val from global heap!");
                set_thread_inactive(thread);
            }

            if(!push_stack(thread, out)){
                vm_error("GLOBAL STORE", "Failed to push global load!");
                set_thread_inactive(thread);
            }
            break;
        }
    }
}
