// GC Runtime Telemetry
#include "grrvm/telemetry/gc_telemetry.h"
#include "grrvm/hal/vm_hal_timer.h"
#include "grrvm/telemetry/vm_telemetry.h"
#include "grrvm/types.h"
#include "grrvm/vm_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t telemetry_flags = 0;

// GC RUNTIME TELEMETRY
uint32_t telementry_gc_runtime_counter = 0;
telemetry_t *gc_runtime_telemetry = NULL;

// EXT FRAG TELEMETRY
uint32_t telementry_ext_frag_counter = 0;
telemetry_ext_frag *gc_ext_frag_telemetry = NULL;

void init_runtime_telemetry(void) {
    if (gc_runtime_telemetry == NULL) {
        gc_runtime_telemetry = malloc(sizeof(telemetry_t) * MAX_TELEMETRY_ENTRIES);

    }
    if (gc_runtime_telemetry != NULL) {
        memset(gc_runtime_telemetry, 0, sizeof(telemetry_t) * MAX_TELEMETRY_ENTRIES);
    }
    telementry_gc_runtime_counter = 0;
}

void uninit_runtime_telemetry(void) {
    if (gc_runtime_telemetry != NULL) {
        free(gc_runtime_telemetry);
        gc_runtime_telemetry = NULL;
    }
    telementry_gc_runtime_counter = 0;
}

void init_ext_frag_telemetry(void) {
    if (gc_ext_frag_telemetry == NULL) {
        gc_ext_frag_telemetry = malloc(sizeof(telemetry_ext_frag) * MAX_TELEMETRY_ENTRIES);
    }
    if (gc_ext_frag_telemetry != NULL) {
        memset(gc_ext_frag_telemetry, 0, sizeof(telemetry_ext_frag) * MAX_TELEMETRY_ENTRIES);
    }
    telementry_ext_frag_counter = 0;
}

void uninit_ext_frag_telemetry(void) {
    if (gc_ext_frag_telemetry != NULL) {
        free(gc_ext_frag_telemetry);
        gc_ext_frag_telemetry = NULL;
    }
    telementry_ext_frag_counter = 0;
}

void telemetry_flag_set(TelemetryFlags flag) {
    telemetry_flags |= (uint32_t)flag;

    if (flag & TELEMETRY_GC_RUNTIME) {
        init_runtime_telemetry();
    }
    if (flag & TELEMETRY_EXT_FRAG) {
        init_ext_frag_telemetry();
    }
}

void telemetry_flag_clear(TelemetryFlags flag) {
    telemetry_flags &= ~(uint32_t)flag;

    if (flag & TELEMETRY_GC_RUNTIME) {
        uninit_runtime_telemetry();
    }
    if (flag & TELEMETRY_EXT_FRAG) {
        uninit_ext_frag_telemetry();
    }
}

byte telemetry_flag_is_set(TelemetryFlags flag) {
    return (telemetry_flags & (uint32_t)flag) != 0;
}

void dump_gc_runtime_telemetry(void) {
    if (gc_runtime_telemetry != NULL) {
        for (g_u32 i = 0; i < telementry_gc_runtime_counter; i++) {
            printf("[GC Telemetry] GC Runtime Telemetry: %u-%u, Delta: %.9f s\n",
                    gc_runtime_telemetry[i].timestamp_a,
                    gc_runtime_telemetry[i].timestamp_b,
                    (gc_runtime_telemetry[i].timestamp_b - gc_runtime_telemetry[i].timestamp_a) / 1000000.0);
        }
    } else {
        printf("[GC Telemetry] No GC Runtime Telemetry to dump, enable GC Runtime Telemetry first!\n");
    }

    telemetry_flag_clear(TELEMETRY_GC_RUNTIME);
}

void dump_ext_frag_telemetry(void) {
    if (gc_ext_frag_telemetry != NULL) {
        for (g_u32 i = 0; i < telementry_ext_frag_counter; i++) {
            printf("[GC Telemetry] External Fragment Telemetry: PreGC: %.9f, PostGC: %.9f\n",
                    gc_ext_frag_telemetry[i].pre_gc_ext_frag,
                    gc_ext_frag_telemetry[i].post_gc_ext_frag);
        }
    } else {
        printf("[GC Telemetry] No External Fragment Telemetry to dump, enable External Fragment Telemetry first!\n");
    }

    telemetry_flag_clear(TELEMETRY_EXT_FRAG);
}

void pre_gc_telemetry(VM* vm) {

    if (telemetry_flag_is_set(TELEMETRY_GC_RUNTIME) &&
        gc_runtime_telemetry != NULL &&
        telementry_gc_runtime_counter < MAX_TELEMETRY_ENTRIES) {

        gc_runtime_telemetry[telementry_gc_runtime_counter].timestamp_a = hal_clock_us();
    }

    if (telemetry_flag_is_set(TELEMETRY_EXT_FRAG) &&
        gc_ext_frag_telemetry != NULL &&
        telementry_ext_frag_counter < MAX_TELEMETRY_ENTRIES) {

        VM_HeapStats heap_stats;
        vm_heap_stats(vm, &heap_stats);
        gc_ext_frag_telemetry[telementry_ext_frag_counter].pre_gc_ext_frag = measure_ext_frag(heap_stats);
    }
}

void post_gc_telemetry(VM* vm) {
    if (telemetry_flag_is_set(TELEMETRY_EXT_FRAG) &&
        gc_ext_frag_telemetry != NULL &&
        telementry_ext_frag_counter < MAX_TELEMETRY_ENTRIES) {

        VM_HeapStats heap_stats;
        vm_heap_stats(vm, &heap_stats);
        gc_ext_frag_telemetry[telementry_ext_frag_counter].post_gc_ext_frag = measure_ext_frag(heap_stats);
        telementry_ext_frag_counter++;
        if(telementry_ext_frag_counter >= MAX_TELEMETRY_ENTRIES) {
            telementry_ext_frag_counter = 0;
        }
    }

    if (telemetry_flag_is_set(TELEMETRY_GC_RUNTIME) &&
        gc_runtime_telemetry != NULL &&
        telementry_gc_runtime_counter < MAX_TELEMETRY_ENTRIES) {

        gc_runtime_telemetry[telementry_gc_runtime_counter].timestamp_b = hal_clock_us();
        telementry_gc_runtime_counter++;
        if(telementry_gc_runtime_counter >= MAX_TELEMETRY_ENTRIES) {
            telementry_gc_runtime_counter = 0;
        }
    }
}


float measure_ext_frag(VM_HeapStats heap_stats){
    return heap_stats.buddy_free_slots ? 1.0f - ((float)heap_stats.longest_buddy_block / heap_stats.buddy_free_slots) : 0.0f;
}
