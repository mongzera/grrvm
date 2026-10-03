#ifndef HEAP_DUMP_WRAPPER_H
#define HEAP_DUMP_WRAPPER_H

#include "grrvm/vm.h"

void __wrap__heap_dump_terminal(VM_Thread *thread);

void __wrap__heap_dump_file(VM_Thread *thread);

void register_heap_dump_wrappers(void);


#endif
