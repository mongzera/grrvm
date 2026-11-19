#ifndef GRRVM_TELEMETRY_VM_TELEMETRY_H
#define GRRVM_TELEMETRY_VM_TELEMETRY_H

/*
 * Sample storage for the three SOP variables:
 *
 *   external fragmentation (Ef)  -> telemetry_frag
 *   GC pause / jitter            -> telemetry_pause
 *   allocation speed             -> telemetry_alloc
 *
 * All buffers are static and sized at compile time, so exhausting them is
 * visible in the linker map instead of appearing as a runtime malloc
 * failure on the Pico. Each stream saturates rather than wraps: when a
 * buffer is full the sample is dropped and counted, and the dump reports
 * both the recorded and attempted totals (see gc_telemetry.c).
 *
 * Sizing notes (Pico: VM_HEAP_SLOTS 25600, 2 threads, 64-slot op stack):
 *   pause  2 KB   - one entry per collection, churn workloads trigger
 *                   tens to low hundreds of collections, not thousands.
 *   frag   4 KB   - heap_stat pairs, costs O(#trees + #slab caches) each.
 *   alloc  1 KB   - one entry per timed batch; the SOP's n = 30 needs 30.
 *
 * Override any of these from a port config before including this header.
 */

#include <stdint.h>

#ifndef VM_TELEM_MAX_PAUSE
#define VM_TELEM_MAX_PAUSE 512u
#endif

#ifndef VM_TELEM_MAX_FRAG
#define VM_TELEM_MAX_FRAG 128u
#endif

#ifndef VM_TELEM_MAX_ALLOC
#define VM_TELEM_MAX_ALLOC 64u
#endif

/* Upper bound on allocations held live inside one timed batch. Bounds the
 * reference stack the benchmark needs; a larger batch is clamped, which
 * caps peak heap use at VM_TELEM_BENCH_MAX_LIVE * max_slots slots. */
#ifndef VM_TELEM_BENCH_MAX_LIVE
#define VM_TELEM_BENCH_MAX_LIVE 256u
#endif

/* One stop-the-world pause: the time run_gc() spent marking and sweeping,
 * excluding the heap-statistics scans (see pre/post_gc_telemetry). */
typedef struct {
    uint32_t delta_us;
} telemetry_pause;

/* Raw heap components before and after one collection. Ef is computed
 * host-side from these so the denominator (total free = buddy + slab) is
 * recomputable and auditable rather than baked into a single float. */
typedef struct {
    uint32_t pre_buddy_free;
    uint32_t pre_slab_free;
    uint32_t pre_longest;
    uint32_t post_buddy_free;
    uint32_t post_slab_free;
    uint32_t post_longest;
} telemetry_frag;

/* One timed allocation batch: `allocs` successful allocations completed in
 * `elapsed_us`. Host computes speed as elapsed_us / allocs, so a batch that
 * stopped early on exhaustion is reported honestly instead of being
 * divided by the requested count. */
typedef struct {
    uint32_t allocs;
    uint32_t elapsed_us;
} telemetry_alloc;

#endif /* GRRVM_TELEMETRY_VM_TELEMETRY_H */
