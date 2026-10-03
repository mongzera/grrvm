#ifndef VM_DUMP_H
#define VM_DUMP_H

#include "vm.h"
#include <stdbool.h>

// Dumps heap state directly to stdout (terminal/UART)
void dump_vm_heap_terminal(const VM *vm);

// Dumps heap state to a timestamped file under ./dump/ on POSIX/Windows.
// On Pico/Embedded targets (no filesystem), gracefully falls back to terminal.
// Returns true if output was written successfully, false on I/O error.
bool dump_vm_heap_file(const VM *vm);

#endif // VM_DUMP_H
