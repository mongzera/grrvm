#ifndef VM_MEM_H
#define VM_MEM_H


#include "grrvm/types.h"
#include "grrvm/vm.h"
#include "grrvm/vm_log.h"

// TODO: Add thread-ownership checking, this is important for thread safety and only the owner can modify if memory is STATE_LOCKED
//
//  direct-memory access
static inline prim_val *get_vm_mem(VM* vm, word address){
    // check address range
    if(address < 0 || address >= VM_HEAP_SLOTS){
        vm_error("MEMORY", "OUT OF BOUNDS ACCESS!");
        return 0;
    }

    prim_val *memslot = &vm->ram[address];
    return memslot;
}
static inline prim_state set_vm_mem(VM* vm, word address, prim_val data){
    // check address range
    if( address >= VM_HEAP_SLOTS){
        vm_error("MEMORY", "OUT OF BOUNDS ACCESS!");
        return -1;
    }

    prim_val *memslot = &vm->ram[address];
    prim_state memstate = get_prim_state(*memslot);

    if(memstate != STATE_OPEN) return memstate;

    memslot->data = data.data;
    memslot->metadata = data.metadata;
    return 0;
}

// heap allocation
void alloc_heap(VM* vm, prim_val size, prim_val address);
void free_heap(VM* vm, prim_val address);


#endif
