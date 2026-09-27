#ifndef VM_GC_H
#define VM_GC_H

#include "grrvm/vm.h"
/*
 *  This features a Mark-Sweep garbage collector.
 *  Very Simplistic Implementation
 */

void run_gc(VM* vm);
void mark_object(VM* vm);
void sweep_objects(VM* vm);
void set_gc_tag(VM* vm, word array_header_addr, byte tag);
void set_tagged(VM* vm, prim_val reference);
void set_untagged(VM* vm, prim_val reference);

#endif
