#ifndef VM_MEM_H
#define VM_MEM_H

#include "grrvm/config.h"
#include "grrvm/types.h"
#include "grrvm/vm.h"
#include "grrvm/vm_log.h"

static inline prim_val *get_vm_mem(VM* vm, word address){
    // check address range
    if(address < 0 || address >= VM_MAX_RAM){
        vm_error("MEMORY", "OUT OF BOUNDS ACCESS!");
        return 0;
    }

    prim_val *memslot = &vm->ram[address];
    return memslot;
}

static inline prim_state set_vm_mem(VM* vm, word address, prim_val data){
    // check address range
    if( address >= VM_MAX_RAM){
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

#endif
