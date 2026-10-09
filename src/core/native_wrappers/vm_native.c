#include "grrvm/vm_native.h"
#include "grrvm/types.h"
#include "grrvm/vm_log.h"
#include "native_wrappers/gpio_wrapper.h"
#include "native_wrappers/heap_dump_wrapper.h"
#include "native_wrappers/internal_telemetry_wrapper.h"
#include "native_wrappers/math_wrapper.h"
#include <stddef.h>

typedef struct {
    g_u32    hash;
    NativeFn function;
} NativeEntry;

typedef struct {
    NativeEntry entries[MAX_NATIVE_FUNCTIONS];
    g_u16       count;
} NativeRegistry;

/* Encapsulated registry state */
static NativeRegistry registry;

void native_registry_init(void) {
    registry.count = 0;

    // register native functions here
    register_math_wrappers();
    register_internal_telemetry_wrappers();
    register_heap_dump_wrappers();
    register_gpio_wrappers();
}

g_u32 native_hash(const char* str) {
    g_u32 hash = FNV1A_OFFSET_BASIS;
    while (*str) {
        hash ^= (g_u8)(*str);
        hash *= FNV1A_PRIME;
        str++;
    }
    return hash;
}

void native_register(const char* symbol_name, NativeFn fn) {
    if (registry.count >= MAX_NATIVE_FUNCTIONS) {
        vm_error("NATIVE", "Registry full! Cannot register: %s", symbol_name);
        return;
    }

    g_u32 hash = native_hash(symbol_name);

    // Collision and duplicate check
    for (g_u16 i = 0; i < registry.count; i++) {
        if (registry.entries[i].hash == hash) {
            vm_error("FATAL", "Hash collision or duplicate for symbol: %s (Hash: 0x%08X)",
                     symbol_name, hash);
            return;
        }
    }

    g_u16 idx = registry.count++;
    registry.entries[idx].hash = hash;
    registry.entries[idx].function = fn;
    vm_info("NATIVE", "Native function registered: %s (Hash: 0x%08X)", symbol_name, hash);
}

NativeFn native_lookup(g_u32 hash) {
    for (g_u16 i = 0; i < registry.count; i++) {
        if (registry.entries[i].hash == hash) {
            return registry.entries[i].function;
        }
    }
    return NULL; // Indicates undefined native invocation
}
