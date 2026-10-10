#ifndef GRRVM_VM_H
#define GRRVM_VM_H

/*
 * Core VM structures (VM, VM_Thread).
 *
 * Include layering (strictly one-way, no cycles):
 *   types.h, default_config.h, vm_log.h, vm_math.h      leaves
 *   prim_val.h                                          value model
 *   vm_global_table.h, vm_alloc.h                       need prim_val only
 *   vm.h                                                needs all of the above
 *   vm_mem.h, vm_thread.h, vm_gc.h, ...                 need the full VM -> include vm.h
 *
 * vm.h must NOT include anything from the last row.
 */

#include "grrvm/types.h"
#include "grrvm/prim_val.h"
#include "grrvm/vm_global_table.h"
#include "grrvm/vm_alloc.h"

typedef struct VM {
    word _program_start;
    word program_size;
    word global_heap_block_address;
    word* program;
    prim_val* ram;
    global_var_table global_var_table;
    VM_Allocator allocator;
    struct VM_Thread* vm_threads;
} VM;

VM* vm_create(void);
void vm_start(VM* vm_instance);
void vm_loop(VM* vm);
void vm_terminate(VM* vm);
void vm_new_thread(VM* vm, word program_counter, int id);


#endif /* GRRVM_VM_H */
