#ifndef VM_MEM_H
#define VM_MEM_H


#include "grrvm/type_promotion.h"
#include "grrvm/type_conversion.h"
#include "grrvm/types.h"
#include "grrvm/vm.h"
#include "grrvm/vm_log.h"

// TODO: Add thread-ownership checking, this is important for thread safety and only the owner can modify if memory is STATE_LOCKED
//
//  direct-memory access
static inline int get_vm_mem(VM* vm, word address, prim_val* out){
    // check address range
    if(address < 0 || address >= VM_HEAP_SLOTS){
        vm_error("MEMORY", "OUT OF BOUNDS ACCESS!");
        return 0;
    }

    prim_val *memslot = &vm->ram[address];
    *out = *memslot;
    return 1;
}

static inline prim_state set_vm_mem(VM* vm, word address, prim_val data) {
    if (address >= VM_HEAP_SLOTS) {
        vm_error("MEMORY", "OUT OF BOUNDS ACCESS!");
        return STATE_ERROR;
    }

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
    if (address >= VM_HEAP_SLOTS) {
        vm_error("MEMORY", "OUT OF BOUNDS ACCESS!");
        return STATE_ERROR;
    }

    prim_val *memslot = &vm->ram[address-1]; // target array block start
    for(int i = 1; i < memslot->data; i++) {
        prim_val *slot = &memslot[i];
        *slot = convert_prim(*slot, type);
    }
    return STATE_OPEN;
}


#endif
