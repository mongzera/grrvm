#ifndef GRRVM_COMMON_VM_DUMP_COMMON_H
#define GRRVM_COMMON_VM_DUMP_COMMON_H

#include "grrvm/vm.h"
#include <stddef.h>
#include <stdio.h>

void dump_heap_to_stream(FILE *out, const prim_val *heap, size_t capacity);

#endif /* GRRVM_COMMON_VM_DUMP_COMMON_H */
