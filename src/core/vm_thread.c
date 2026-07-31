#include "../../include/grrvm/vm.h"
#include "grrvm/config.h"
#include "grrvm/vm_log.h"
#include <stdlib.h>

void vm_new_thread(VM *vm, word program_counter, int id){

    if(id < 0 || id >= VM_MAX_THREADS) {
        vm_error("VM THREAD", "Thread ID is invalid!");
        exit(-1);
    }

    VM_Thread* thread = &vm->vm_threads[id];
    thread->status = 0;
    thread->sfp = -1;
    thread->sp = -1;
    thread->csp = -1;
    thread->vm = vm;
    thread->pc = program_counter;
    thread->pc_checkpoint = program_counter;

    set_thread_active(thread);
}
