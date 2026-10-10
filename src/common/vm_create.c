#include "grr_port_config.h"
#include "grrvm/prim_val.h"
#include "grrvm/vm.h"
#include "grrvm/vm_thread.h"
#include <stdlib.h>

VM* vm_create(void){
    VM* vm = (VM*)malloc(sizeof(VM));
    vm->global_heap_block_address = 0;
    vm->program = malloc((sizeof(word) * VM_PROGRAM_MAX_SIZE));
    vm->ram = malloc((sizeof(prim_val) * VM_HEAP_SLOTS));
    vm->vm_threads = malloc((sizeof(VM_Thread) * VM_MAX_THREADS));

    for(int i = 0; i < VM_MAX_THREADS; i++){
        set_thread_inactive(&vm->vm_threads[i]);
    }
    vm_alloc_init(vm);


    return vm;
}
