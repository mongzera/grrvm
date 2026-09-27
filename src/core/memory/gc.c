#include "grr_port_config.h"
#include "grrvm/types.h"
#include "grrvm/vm.h"
#include "grrvm/vm_alloc.h"
#include "grrvm/vm_gc.h"
#include "grrvm/vm_log.h"

void run_gc(VM* vm) {
    vm_info("GC", "GC Invoked due to lack of Memory");
    mark_object(vm);
    sweep_objects(vm);
}

void mark_object(VM* vm) {
    // loop all Threads
    for(g_u32 i = 0; i < VM_MAX_THREADS; i++) {
        VM_Thread thread = vm->vm_threads[i];

        // skip inactive threads
        if(!is_thread_active(&thread)){
            continue;
        }

        // check topmost thread call stack frame towards the bottom
        for(g_int i = thread.sfp; i >= 0; i--) {
            prim_val reference = thread.call_stack_frame[i];
            if(get_prim_type(reference) == TYPE_REFERENCE) {
                set_tagged(vm, reference);
                vm_info("GC", "Tagged reference: %d", reference.data);
            }
        }

        // check topmost thread operand stack towards the bottom
        for(g_int i = thread.sp; i >= 0; i--) {
            prim_val reference = thread.op_stack[i];
            if(get_prim_type(reference) == TYPE_REFERENCE) {
                set_tagged(vm, reference);
                vm_info("GC", "Tagged reference: %d", reference.data);
            }
        }
    }
}

void sweep_objects(VM* vm) {
    for(uint32_t i = 0; i < VM_HEAP_SLOTS; i++) {
        prim_val array_length = vm->ram[i];
        //vm_info("GC", "IDX: %u TAG: %u", i, array_length.gc_mark);
        if(get_prim_type(array_length) != TYPE_LENGTH) {
            continue;
        }

        if(array_length.gc_mark == 0) {
            g_free_direct(vm, i+1);
            i += array_length.data + 1; // skip over the array elements
            vm_info("GC", "Freed array at address %u", i-1);
            continue;
        }

        set_gc_tag(vm, i, 0);

    }
}

/*
 * Sets the GC tag of the array header
 */
void set_gc_tag(VM* vm, word array_header_addr, byte tag) {
    prim_val *array = &vm->ram[array_header_addr];
    if(get_prim_type(*array) != TYPE_LENGTH){
        vm_error("GC", "Attempted to tag non-array reference: Address %u", array_header_addr);
        return;
    }

    array->gc_mark = tag;
}

/*
 * Sets the GC tag of the array header to 1 (tagged)
 * Subtracts 1 from the data field to target for the array header, rather than the first element
 */
void set_tagged(VM* vm, prim_val reference) {
    reference.data -= 1;
    set_gc_tag(vm, reference.data, 1);
}

/*
 * Sets the GC tag of the array header to 0 (untagged)
 * Subtracts 1 from the data field to target for the array header, rather than the first element
 */
void set_untagged(VM* vm, prim_val reference) {
    reference.data -= 1;
    set_gc_tag(vm, reference.data, 0);
}
