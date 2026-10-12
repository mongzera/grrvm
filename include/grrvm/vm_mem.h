#ifndef GRRVM_VM_MEM_H
#define GRRVM_VM_MEM_H

/* Direct / heap memory accessors. Needs the full VM, so it sits *above* vm.h:
 * include it explicitly where get_vm_mem / set_vm_mem / set_heap_block_type
 * are used (vm.h no longer pulls it in). */

#include <assert.h>
#include "grrvm/prim_val.h"
#include "grrvm/vm.h"
#include "grrvm/vm_alloc.h"
#include "grrvm/vm_log.h"
#include "grrvm/type_conversion.h"
#include "grrvm/type_promotion.h"

// TODO: Add thread-ownership checking, this is important for thread safety and only the owner can modify if memory is STATE_LOCKED
//
//  direct-memory access
static inline int get_vm_mem(VM* vm, word address, prim_val* out){
    // check address range
    if(address >= VM_HEAP_SLOTS){
        vm_error("MEMORY", "OUT OF BOUNDS ACCESS!");
        return 0;
    }

    prim_val *memslot = &vm->ram[address];
    *out = *memslot;
    return 1;
}

static inline prim_state set_vm_mem(VM* vm, word address_base, word offset, prim_val data) {
    if (address_base + offset >= VM_HEAP_SLOTS) {
        vm_error("MEMORY", "OUT OF HEAP BOUNDS ACCESS!");
        return STATE_ERROR;
    }

    /* GC invariant: references may only live on thread stacks (op_stack /
     * call_stack_frame), never in ram[]. The collector scans only the stacks
     * plus the global heap block itself, so a reference stored here would be
     * untraced and its target swept while still "reachable" in the holder.
     * The ISA has no dedicated instruction for this, but H_ALLOC + H_STORE
     * could otherwise construct it on an uninitialized (TYPE_NULL) slot. */
    if (get_prim_type(data) == TYPE_REFERENCE) {
        assert(get_prim_type(data) != TYPE_REFERENCE &&
               "references must not be stored into ram[] slots");
        vm_error("MEMORY", "Type Error: References cannot be stored into memory slots!");
        return STATE_ERROR;
    }

    word block_root = address_base-1;
    prim_val *heap_block = &vm->ram[block_root];

    // there will be instances where the block_size is not TYPE_HEAP_BLOCK. So we iterate in reverse to find it.

    while(get_prim_type(*heap_block) != TYPE_HEAP_BLOCK) {

        block_root--;
        heap_block--;
    }


    if(address_base + offset > block_root + get_heap_block_size(heap_block->data)){

        vm_error("MEMORY", "OUT OF ARRAY BOUNDS ACCESS! add_base: %u | off: %u | block_root: %u | heap_block_size: %u", address_base, offset, block_root, get_heap_block_size(heap_block->data));
        return STATE_ERROR;
    }

    word address = address_base + offset;

    prim_val *memslot = &vm->ram[address];
    prim_state memstate = get_prim_state(*memslot);

    if (memstate != STATE_OPEN) return memstate;

    prim_type memtype = get_prim_type(*memslot);
    prim_type datatype = get_prim_type(data);

    // Uninitialized slot: store converted val directly
    if (memtype == TYPE_NULL) {
        *memslot = convert_prim(data, datatype);
        return STATE_OPEN;
    }

    int mem_type_rank = get_type_rank(memtype);
    int data_type_rank = get_type_rank(datatype);

    if (data_type_rank > mem_type_rank) {
        vm_error("MEMORY", "Type Error: Cannot assign higher-rank type to lower-rank slot!");
        return STATE_ERROR;
    }

    prim_type promoted_type;
    if (!promote_types(memtype, datatype, &promoted_type)) {
        vm_error("MEMORY", "Type Error: Cannot assign mismatched types!");
        return STATE_ERROR;
    }

    // Re-encodes the payload bit pattern and updates metadata
    *memslot = convert_prim(data, promoted_type);
    return STATE_OPEN;
}

static inline prim_state set_heap_block_type(VM* vm, word address, prim_type type) {
    if (address == 0 || address >= VM_HEAP_SLOTS) {
        vm_error("MEMORY", "OUT OF BOUNDS ACCESS!");
        return STATE_ERROR;
    }

    prim_val *memslot = &vm->ram[address-1]; // target array block start
    if (get_prim_type(*memslot) != TYPE_HEAP_BLOCK) {
        vm_error("MEMORY", "Type Error: H_SET_TYPE target is not a heap block!");
        return STATE_ERROR;
    }

    /* memslot->data is the packed metadata word (block_size << 16 | n_slots),
     * NOT a raw slot count. Convert exactly the logical n_slots data slots -
     * never past the physical block, never past the heap. */
    word n_slots = get_n_slots(memslot->data);
    if (n_slots == 0 || (address - 1 + n_slots) >= VM_HEAP_SLOTS) {
        vm_error("MEMORY", "Corrupt heap block metadata at 0x%X!", (unsigned)(address - 1));
        return STATE_ERROR;
    }

    for(word i = 1; i <= n_slots; i++) {
        prim_val *slot = &memslot[i];
        *slot = convert_prim(*slot, type);
    }
    return STATE_OPEN;
}

#endif /* GRRVM_VM_MEM_H */
