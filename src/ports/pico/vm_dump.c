#include "grrvm/vm_dump.h"
#include "grrvm/common/vm_dump_common.h"
#include <stdio.h>

void dump_vm_heap_terminal(const VM *vm) {
    if (!vm) {
        printf("[VM Dump Error] Invalid VM pointer.\n");
        return;
    }

    printf("VM HEAP DUMP (PICO UART/USB)\n");
    printf("Heap Slots: %d\n\n", (int)VM_HEAP_SLOTS);
    dump_heap_to_stream(stdout, vm->ram, VM_HEAP_SLOTS);
}

bool dump_vm_heap_file(const VM *vm) {
    if (!vm) {
        printf("[VM Dump Error] Invalid VM pointer.\n");
        return false;
    }

    printf("[VM Dump] Pico target has no local file system. Redirecting file dump to terminal output:\n\n");
    dump_vm_heap_terminal(vm);
    return true;
}
