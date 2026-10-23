#include "native_wrappers/heap_dump_wrapper.h"
#include "grrvm/vm_thread.h"
#include "grrvm/vm_dump.h"
#include "grrvm/vm_native.h"

void __wrap__heap_dump_terminal(VM_Thread *thread) {
    dump_vm_heap_terminal(thread->vm);
}

void __wrap__heap_dump_file(VM_Thread *thread) {
    dump_vm_heap_file(thread->vm);

}

void register_heap_dump_wrappers(void){
    native_register("dump_heap_stdout", __wrap__heap_dump_terminal);
    native_register("dump_heap_file", __wrap__heap_dump_file);
}
