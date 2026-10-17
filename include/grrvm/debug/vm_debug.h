#ifndef VM_DEBUG_H
#define VM_DEBUG_H

#include "grrvm/vm_thread.h"
#include <stdint.h>
void print_vm_stack(VM_Thread *thread, uint8_t levels);

#endif
