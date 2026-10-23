#ifndef GRRVM_HAL_LOAD_VM_H
#define GRRVM_HAL_LOAD_VM_H
#include <stddef.h>
#include "grrvm/vm.h"

VM* load_vm(const void* data, size_t size);

#endif /* GRRVM_HAL_LOAD_VM_H */
