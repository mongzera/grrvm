#include "grr_port_config.h"
#include "grrvm/telemetry/gc_telemetry.h"
#include "grrvm/types.h"
#include "grrvm/vm.h"
#include "grrvm/vm_alloc.h"
#include "grrvm/vm_gc.h"
#include "grrvm/vm_log.h"


void run_gc(VM* vm) {
    vm_info("GC", "Invoked due to lack of Memory");

    pre_gc_telemetry(vm);
    mark_object(vm);
    sweep_objects(vm);
    post_gc_telemetry(vm);

}


void mark_object(VM* vm) {
    // Loop all threads.
    for (g_u32 i = 0; i < VM_MAX_THREADS; i++) {
        VM_Thread *thread = &vm->vm_threads[i];

        // Skip inactive threads.
        if (!is_thread_active(thread)) {
            continue;
        }


        // Check the call stack from the topmost frame towards the bottom.
        for (g_int j = thread->sfp + thread->n_local_vars; j >= 0; j--) {

            prim_val reference = thread->call_stack_frame[j];

            if (get_prim_type(reference) == TYPE_REFERENCE) {
                set_tagged(vm, reference);

                vm_info(
                    "GC",
                    "Tagged reference: %d",
                    reference.data
                );
            }
        }


        // Check the operand stack from the top towards the bottom.
        for (g_int j = thread->sp; j >= 0; j--) {

            prim_val reference = thread->op_stack[j];

            if (get_prim_type(reference) == TYPE_REFERENCE) {
                set_tagged(vm, reference);

                vm_info(
                    "GC",
                    "Tagged reference: %d",
                    reference.data
                );
            }
        }


    }
}


void sweep_objects(VM* vm) {
    for (word i = 0; i < VM_HEAP_SLOTS; i++) {
        prim_val header = vm->ram[i];

        /*
         * Every heap allocation begins with a TYPE_LENGTH header.
         *
         * header.data contains the physical allocation block size,
         * including the header itself.
         *
         * Example:
         *
         *   n_slots = 4
         *
         *   [ H ][ E ][ E ][ E ][ E ][ unused ... ]
         *     <--------- block_size --------->
         *
         * The allocator may own more slots than the logical array uses.
         */
        if (get_prim_type(header) != TYPE_LENGTH) {
            continue;
        }

        word block_size = header.data;

        /*
         * A corrupted header must not be allowed to make the
         * sweep cursor leave the heap.
         */
        if (block_size == 0 ||
            block_size > VM_HEAP_SLOTS - i) {
            vm_error(
                "GC",
                "Invalid allocation block size %u at address %u!",
                block_size,
                i
            );

            return;
        }

        if (header.gc_mark == 0) {
            /*
             * Reference points to the first element, while the
             * allocator expects the data address.
             */
            g_free_direct(vm, i + 1);

            vm_info(
                "GC",
                "Freed array at address %u (block size %u)",
                i,
                block_size
            );
        } else {
            /*
             * Object survived this collection.
             *
             * Clear the mark so that the next GC starts with
             * an unmarked heap.
             */
            set_gc_tag(vm, i, 0);
        }

        /*
         * Skip the entire physical allocation block.
         *
         * The for-loop's i++ advances us to the next allocation.
         *
         * Example:
         *
         *   i = 100
         *   block_size = 8
         *
         *   allocation occupies [100..107]
         *
         *   i += 7
         *   for-loop i++
         *   i == 108
         */
        i += block_size - 1;
    }
}


/*
 * Sets the GC tag of the allocation header.
 */
void set_gc_tag(VM* vm, word array_header_addr, byte tag) {
    prim_val *header = &vm->ram[array_header_addr];

    if (get_prim_type(*header) != TYPE_LENGTH) {
        vm_error(
            "GC",
            "Attempted to tag non-array reference: Address %u",
            array_header_addr
        );

        return;
    }

    header->gc_mark = tag;
}


/*
 * Sets the GC tag of the allocation header to 1 (tagged).
 *
 * A reference points to the first element of the array,
 * so subtract 1 to obtain the allocation header.
 */
void set_tagged(VM* vm, prim_val reference) {
    if (reference.data == 0) {
        vm_error("GC", "Attempted to tag invalid null reference!");
        return;
    }

    set_gc_tag(vm, reference.data - 1, 1);
}


/*
 * Sets the GC tag of the allocation header to 0 (untagged).
 *
 * A reference points to the first element of the array,
 * so subtract 1 to obtain the allocation header.
 */
void set_untagged(VM* vm, prim_val reference) {
    if (reference.data == 0) {
        vm_error("GC", "Attempted to untag invalid null reference!");
        return;
    }

    set_gc_tag(vm, reference.data - 1, 0);
}
