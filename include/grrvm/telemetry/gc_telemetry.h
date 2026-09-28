#ifndef GC_TELEMETRY_H
#define GC_TELEMETRY_H

#include "grrvm/types.h"
#include "grrvm/vm.h"
#include "grrvm/vm_alloc.h"


typedef enum {
    TELEMETRY_GC_RUNTIME = 1,
    TELEMETRY_EXT_FRAG,
} TelemetryFlags;

void telemetry_flag_set(TelemetryFlags flag);
void telemetry_flag_clear(TelemetryFlags flag);
byte telemetry_flag_is_set(TelemetryFlags flag);

void dump_gc_runtime_telemetry(void);
void dump_ext_frag_telemetry(void);

float measure_ext_frag(VM_HeapStats heap_stats);

void pre_gc_telemetry(VM* vm);
void post_gc_telemetry(VM* vm);

#endif // GC_TELEMETRY_H
