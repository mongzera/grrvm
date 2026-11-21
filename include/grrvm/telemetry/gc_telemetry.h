#ifndef GRRVM_TELEMETRY_GC_TELEMETRY_H
#define GRRVM_TELEMETRY_GC_TELEMETRY_H

#include "grrvm/types.h"
#include "grrvm/vm.h"
#include "grrvm/vm_alloc.h" /* VM_HeapStats */

/*
 * Telemetry contract
 * ------------------
 * 1. Sampling never prints. pre/post_gc_telemetry() only write into static
 *    sample arrays, and telemetry_bench_alloc() only times allocator calls.
 *    Everything printed goes through telemetry_flush(), which the guest
 *    invokes once at the end of the run via INVOKE_NATIVE. USB-CDC or
 *    UART blocking therefore cannot enter a single sample.
 * 2. Heap statistics are sampled outside the timed GC window, so the Ef
 *    metric never contaminates the pause measurement.
 * 3. Streams saturate and report (recorded, total). A full buffer loses
 *    samples loudly instead of wrapping silently.
 *
 * Guest usage:
 *   INVOKE_NATIVE telemetry_reset          ; clear buffers
 *   INVOKE_NATIVE set_gc_enabled  (0|1)    ; isolate allocator from collector
 *   ... workload ...
 *   INVOKE_NATIVE full_heap_scan           ; optional Ef_full checkpoint
 *   INVOKE_NATIVE telemetry_flush          ; single CSV stream, once
 */

typedef enum {
    TELEMETRY_GC_RUNTIME = 1,
    TELEMETRY_EXT_FRAG   = 2,
} TelemetryFlags;

void telemetry_flag_set(TelemetryFlags flag);
void telemetry_flag_clear(TelemetryFlags flag);
byte telemetry_flag_is_set(TelemetryFlags flag);

/* Clears the telemetry flags, all sample buffers and all counters. Does
 * not touch the GC enable state or the heap. Intended to be the first
 * telemetry call of a run, before any flag is enabled. */
void telemetry_reset_all(void);

/* Emits the whole CSV stream (header, PAUSE/FRAG/ALLOC rows, END+crc).
 * This is the only function that writes to stdio. */
void telemetry_flush(void);

/* Deprecated per-stream dumps, kept for guest programs that call them
 * directly. They emit their own rows without the header/CRC trailer;
 * prefer telemetry_flush(). */
void dump_gc_runtime_telemetry(void);
void dump_ext_frag_telemetry(void);

/* Ef = 1 - (largest allocatable block) / (total free slots).
 * Returns NAN when the heap has no free slots at all: a full heap has no
 * meaningful fragmentation figure and must not be reported as "0.0". */
float measure_ext_frag(VM_HeapStats heap_stats);

/* Ef over the full heap scan: 1 - longest contiguous free run / free slots.
 * This is the algorithm-independent counterpart of measure_ext_frag():
 * it uses the physical layout in ram[] (buddy and slab together) instead of
 * each allocator's own bookkeeping, so a linear block heap can compute the
 * same figure. Returns NAN when the heap has no free slots. */
float measure_full_heap_frag(VM_FullHeapStats heap_stats);

/* Appends one rounded-up (or dropped, if the static buffer is saturated)
 * FULLSCAN sample. Called by the full_heap_scan native; no output happens
 * until telemetry_flush(). */
void telemetry_record_full_heap_scan(const VM_FullHeapStats* heap_stats);

void pre_gc_telemetry(VM* vm);
void post_gc_telemetry(VM* vm);

/* Runs `batches` timed batches of `per_batch` allocations, each between
 * `min_slots` and `max_slots` drawn from a seeded LCG so the identical
 * size sequence can be replayed by another runtime. Allocations use the
 * no-GC fast path; frees happen after the timer stops, outside the window.
 * Returns the number of batches recorded. */
uint32_t telemetry_bench_alloc(VM* vm, word batches, word per_batch,
                               word min_slots, word max_slots, g_u32 seed);

/* Strong-definition hook: ports that must wait for a console to attach
 * (e.g. USB CDC enumeration) implement this. The shared core provides a
 * weak no-op. Called once at the start of telemetry_flush(), after the
 * guest program has finished. */
void telemetry_pre_flush_hook(void);

#endif /* GRRVM_TELEMETRY_GC_TELEMETRY_H */
