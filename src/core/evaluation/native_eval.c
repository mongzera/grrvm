#include "grrvm/evaluation.h"
#include "grrvm/opcodes.h"
#include "grrvm/vm_log.h"
#include "grrvm/vm_native.h"
#include "grrvm/vm_thread.h"

void eval_native_operand(VM_Thread* thread, word opcode) {
    switch (opcode) {
        case INVOKE_NATIVE: {

            word symbol_hash = get_instruction(thread);

            // Look up the function pointer
            NativeFn native_func = native_lookup(symbol_hash);

            if (native_func != NULL) {
                // Execute the native C function
                native_func(thread);

            } else {

                vm_error("EXEC", "Unresolved native hash: 0x%08X at PC: %d",
                            symbol_hash, thread->pc - 1);
                set_thread_inactive(thread);
            }
            break;
        }

        default: {
            vm_error("NATIVE OPCODE", "Unknown native opcode: %d", opcode);
            break;
        }
    }
}
