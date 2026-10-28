#include "grr_port_config.h"
#include "grrvm/prim_val.h"
#include "grrvm/vm.h"
#include "grrvm/vm_thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VM* vm_create(void){
    VM* vm = (VM*)malloc(sizeof(VM));
    if (vm == NULL) {
        return NULL;
    }

    // Zero everything first so partially-initialized VMs (allocation
    // failure below, parser failure in the caller) never expose dangling
    // pointers such as global_var_table.rows.
    memset(vm, 0, sizeof(VM));

    vm->global_heap_block_address = -1;
    vm->program = malloc((sizeof(word) * VM_PROGRAM_MAX_SIZE));
    vm->ram = malloc((sizeof(prim_val) * VM_HEAP_SLOTS));
    vm->vm_threads = malloc((sizeof(VM_Thread) * VM_MAX_THREADS));

    if (vm->program == NULL || vm->ram == NULL || vm->vm_threads == NULL) {
        free(vm->program);
        free(vm->ram);
        free(vm->vm_threads);
        free(vm);
        return NULL;
    }

    for(int i = 0; i < VM_MAX_THREADS; i++){
        set_thread_inactive(&vm->vm_threads[i]);
    }

    vm_alloc_init(vm);

    printf("Allocated Heap Size: %u", sizeof(vm->ram));

    return vm;
}
