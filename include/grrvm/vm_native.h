#ifndef GRRVM_VM_NATIVE_H
#define GRRVM_VM_NATIVE_H

#include "grrvm/types.h"
#include "grrvm/vm_thread.h"

#define MAX_NATIVE_FUNCTIONS 128
#define FNV1A_PRIME          0x01000193U
#define FNV1A_OFFSET_BASIS   0x811C9DC5U

/* Native function signature matching VM_Thread execution context */
typedef void (*NativeFn)(VM_Thread* thread);

/* Public API */
void native_registry_init(void);
void native_register(const char* symbol_name, NativeFn fn);
NativeFn native_lookup(g_u32 hash);
g_u32 native_hash(const char* str);

/* GCC/Clang macro for automatic native function registration at boot */
#if defined(__GNUC__) || defined(__clang__)
    #define REGISTER_NATIVE(fn_name) \
        static void __attribute__((constructor)) __reg_##fn_name(void) { \
            native_register(#fn_name, fn_name); \
        }
#else
    #define REGISTER_NATIVE(fn_name)
#endif

#endif /* GRRVM_VM_NATIVE_H */
