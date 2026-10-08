#ifndef VM_DUMP_COMMON_H
#define VM_DUMP_COMMON_H

#include "grrvm/vm.h"
#include <stdio.h>

void dump_heap_to_stream(FILE *out, const prim_val *heap, size_t capacity);

#endif // VM_DUMP_COMMON_H
