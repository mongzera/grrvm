#include "grrvm/evaluation.h""
#include "grrvm/vm_mem.h"
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

            if(get_prim_type(reference) == TYPE_NULL){
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

        case H_STORE:{
            prim_val value;
            prim_val reference;


            if(!pop_stack(thread, &reference)){
                vm_error("MEMORY HEAP OPERAND", "Failed to pop stack for H_FREE");
                set_thread_inactive(thread);
                break;
            }

            if(!pop_stack(thread, &value)){
                vm_error("MEMORY HEAP OPERAND", "Failed to pop stack for H_FREE");
                set_thread_inactive(thread);
                break;
            }

            if(!type_check_non_negative(reference) && reference.data != 0){
                vm_error("MEMORY HEAP OPERAND", "Expected positive integer for H_STORE");
                set_thread_inactive(thread);
                break;
            }

            vm_info("H_STORE", "Storing to address: %u", reference.data);

            if(set_vm_mem(thread->vm, reference.data, 0, value) == STATE_ERROR){
                vm_error("MEMORY HEAP OPERAND", "Failed to set memory at address %u", reference.data);
                set_thread_inactive(thread);
                break;
            }
            break;
        }

        case H_LOAD:{
            prim_val reference;

            if(!pop_stack(thread, &reference)){
                vm_error("MEMORY HEAP OPERAND", "Failed to pop stack for H_FREE");
                set_thread_inactive(thread);
                break;
            }

            if(!type_check_non_negative(reference) && reference.data != 0){
                vm_error("MEMORY HEAP OPERAND", "Expected positive integer for H_LOAD");
                set_thread_inactive(thread);
                break;
            }

            prim_val out;

            vm_info("H_LOAD", "Loading from address: %u", reference.data);

            if(!get_vm_mem(thread->vm, reference.data, &out)){
                vm_error("MEMORY HEAP OPERAND", "Failed to set memory at address %u", reference.data);
                set_thread_inactive(thread);
                break;
            }

            if(!push_stack(thread, out)){
                vm_error("MEMORY HEAP OPERAND", "Failed to push stack for H_LOAD");
                set_thread_inactive(thread);
                break;
            }

            break;
        }

        case H_STORE_OFF:{
            prim_val value;
            prim_val offset;
            prim_val reference;


            if(!pop_stack(thread, &reference)){
                vm_error("MEMORY HEAP OPERAND", "Failed to pop stack for H_STORE_OFF: Reference");
                set_thread_inactive(thread);
                break;
            }

            if(!pop_stack(thread, &offset)){
                vm_error("MEMORY HEAP OPERAND", "Failed to pop stack for H_STORE_OFF: Offset");
                set_thread_inactive(thread);
                break;
            }

            if(!pop_stack(thread, &value)){
                vm_error("MEMORY HEAP OPERAND", "Failed to pop stack for H_STORE_OFF: Value");
                set_thread_inactive(thread);
                break;
            }

            if(!type_check_non_negative(reference) && reference.data != 0){
                vm_error("MEMORY HEAP OPERAND", "Expected positive integer for H_STORE_OFF");
                set_thread_inactive(thread);
                break;
            }

            if(!type_check_non_negative(offset) && offset.data != 0){
                vm_error("MEMORY HEAP OPERAND", "Expected positive integer for H_STORE_OFF: Offset");
                set_thread_inactive(thread);
                break;
            }

            vm_info("H_STORE_OFF", "Storing to address: %u + %u", reference.data, offset.data);

            if(set_vm_mem(thread->vm, reference.data, offset.data, value) == STATE_ERROR){
                vm_error("MEMORY HEAP OPERAND", "Failed to set memory at address %u", reference.data + offset.data);
                set_thread_inactive(thread);
                break;
            }
            break;
        }

        case H_LOAD_OFF:{
            prim_val offset;
            prim_val reference;

            if(!pop_stack(thread, &reference)){
                vm_error("MEMORY HEAP OPERAND", "Failed to pop stack for H_LOAD_OFF: Reference");
                set_thread_inactive(thread);
                break;
            }

            if(!pop_stack(thread, &offset)){
                vm_error("MEMORY HEAP OPERAND", "Failed to pop stack for H_LOAD_OFF: Offset");
                set_thread_inactive(thread);
                break;
            }

            if(!type_check_non_negative(reference) && reference.data != 0){
                vm_error("MEMORY HEAP OPERAND", "Expected positive integer for H_LOAD_OFF: Reference");
                set_thread_inactive(thread);
                break;
            }

            if(!type_check_non_negative(offset) && offset.data != 0){
                vm_error("MEMORY HEAP OPERAND", "Expected positive integer for H_LOAD_OFF: Offset");
                set_thread_inactive(thread);
                break;
            }

            prim_val out;

            vm_info("H_LOAD_OFF", "Loading from address: %u + %u", reference.data, offset.data);

            if(!get_vm_mem(thread->vm, reference.data + offset.data, &out)){
                vm_error("MEMORY HEAP OPERAND", "Failed to set memory at address %u + %u", reference.data, offset.data);
                set_thread_inactive(thread);
                break;
            }

            if(!push_stack(thread, out)){
                vm_error("MEMORY HEAP OPERAND", "Failed to push stack for H_LOAD_OFF");
                set_thread_inactive(thread);
                break;
            }

            break;
        }

        default:
            vm_error("MEMORY HEAP OPERAND", "Unrecognized memory heap opcode [0x%X]", opcode);
            set_thread_inactive(thread);
            break;
    }
}
