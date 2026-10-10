#ifndef GRRVM_VM_GLOBAL_TABLE_H
#define GRRVM_VM_GLOBAL_TABLE_H

/* Global variable table. Split out of vm_mem.h because struct VM embeds it
 * by value, so it has to be defined before vm.h - while vm_mem.h needs the
 * full VM and therefore has to come after vm.h. */

#include <stdint.h>
#include "grrvm/prim_val.h"

typedef struct {
    prim_val* address;
} global_var_table_row;

typedef struct {
    global_var_table_row* rows;
    uint32_t row_count;
} global_var_table;

#endif /* GRRVM_VM_GLOBAL_TABLE_H */
