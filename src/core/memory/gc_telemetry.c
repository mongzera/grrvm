/*
 * Telemetry for the three SOP variables:
 *
 *   external fragmentation (Ef), GC pause ("jitter"), allocation speed.
 *
 * Design rules (see the contract comment in gc_telemetry.h):
 *
 *   - Sampling never prints. The store-then-flush split is what keeps
 *     USB-CDC/UART blocking out of every sample; all output happens in
 *     telemetry_flush(), invoked once after the guest program finishes.
 *   - Buffers are static and saturate instead of wrapping. Each stream
 *     keeps a monotonic total, so a full buffer is reported as a
 *     (recorded, total) pair rather than silently overwriting samples.
 *   - Heap statistics are sampled *outside* the timed GC window so the Ef
 *     scan never inflates the pause it is measured alongside.
 *   - Ef has two definitions. The pre/post FRAG stream is allocator-aware
 *     (buddy longest[] + slab free lists); the on-demand FULLSCAN stream is
 *     a layout-only walk of the block headers, which is the definition that
 *     also applies to a linear block heap such as MicroPython's. See
 *     vm_full_heap_scan() in vm_alloc.h.
 */

#include "grrvm/telemetry/gc_telemetry.h"
#include "grrvm/hal/vm_hal_timer.h"
#include "grrvm/telemetry/vm_telemetry.h"
#include "grrvm/types.h"
#include "grrvm/vm_alloc.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

uint32_t telemetry_flags = 0;

/* ---------------- Sample storage ---------------- */

static telemetry_pause    pause_samples[VM_TELEM_MAX_PAUSE];
static telemetry_frag     frag_samples[VM_TELEM_MAX_FRAG];
static telemetry_alloc    alloc_samples[VM_TELEM_MAX_ALLOC];
static telemetry_fullscan fullscan_samples[VM_TELEM_MAX_FULLSCAN];

static uint32_t pause_recorded, pause_total;
static uint32_t frag_recorded,  frag_total;
static uint32_t alloc_recorded, alloc_total;
static uint32_t fullscan_recorded, fullscan_total;

/* One collection is in flight: a pre-hook has stored the start timestamp
 * and/or the pre-GC heap components. */
static uint32_t pause_start_us;
static byte     pause_pending;
static byte     frag_pending;

/* Params of the last bench_alloc call, echoed in the dump header so the
 * exact allocation-size sequence can be replayed on another runtime. */
static struct {
    word batches, per_batch, min_slots, max_slots;
    g_u32 seed;
    byte ran;
} bench_params;

/* NOTE: a churn workload cannot be driven from C. The collector's only roots
 * are the thread stacks (call_stack_frame / op_stack) and the global heap
 * block header, so a block whose reference is held in a C static is swept at
 * the very next collection. Any workload that must keep "islands" alive
 * across collections has to hold them in guest locals - see
 * guest_programs/gc_jitter_test.grr. */

/* ---------------- Flags ---------------- */

void telemetry_flag_set(TelemetryFlags flag) {
    /* Idempotent: enabling a stream that is already on must not wipe the
     * samples collected so far (the previous version memset on every set). */
    telemetry_flags |= (uint32_t)flag;
}

void telemetry_flag_clear(TelemetryFlags flag) {
    telemetry_flags &= ~(uint32_t)flag;
}

byte telemetry_flag_is_set(TelemetryFlags flag) {
    return (telemetry_flags & (uint32_t)flag) != 0;
}

void telemetry_reset_all(void) {
    telemetry_flags = 0;

    pause_recorded = pause_total = 0;
    frag_recorded  = frag_total  = 0;
    alloc_recorded = alloc_total = 0;
    fullscan_recorded = fullscan_total = 0;

    pause_pending = 0;
    frag_pending  = 0;
    pause_start_us = 0;

    bench_params.ran = 0;
}

/* ---------------- Output (post-run only) ---------------- */

/* Incremental CRC32 (IEEE 802.3, reflected, poly 0xEDB88320) over the
 * emitted text, so the host can verify it received every byte intact.
 * Same polynomial as grr_crc32() in program_transfer.c, but that one is
 * one-shot; this needs to survive across lines. */
static const uint32_t crc32_nibble_table[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

static uint32_t emission_crc;

static void crc_update(const char* s, size_t n) {
    uint32_t crc = emission_crc ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        crc ^= (uint8_t)s[i];
        crc = (crc >> 4) ^ crc32_nibble_table[crc & 0x0Fu];
        crc = (crc >> 4) ^ crc32_nibble_table[crc & 0x0Fu];
    }
    emission_crc = crc ^ 0xFFFFFFFFu;
}

/* Every measurement row goes through here: formatted into a line first so
 * the CRC covers exactly the bytes written to the console. */
static void telemetry_emit(const char* fmt, ...) {
    char line[128];

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);

    if (n <= 0) return;
    if ((size_t)n > sizeof line - 1) n = (int)sizeof line - 1;

    crc_update(line, (size_t)n);
    fputs(line, stdout);
}

/* Trailer and banners: printed but not covered by the payload CRC. */
static void telemetry_emit_raw(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

static void emit_pause_rows(void) {
    for (uint32_t i = 0; i < pause_recorded; i++) {
        telemetry_emit("PAUSE,%u,%u\n", (unsigned)(i + 1),
                       (unsigned)pause_samples[i].delta_us);
    }
}

static void emit_frag_rows(void) {
    for (uint32_t i = 0; i < frag_recorded; i++) {
        const telemetry_frag* f = &frag_samples[i];
        telemetry_emit("FRAG,%u,%u,%u,%u,%u,%u,%u\n", (unsigned)(i + 1),
                       (unsigned)f->pre_buddy_free,
                       (unsigned)f->pre_slab_free,
                       (unsigned)f->pre_longest,
                       (unsigned)f->post_buddy_free,
                       (unsigned)f->post_slab_free,
                       (unsigned)f->post_longest);
    }
}

static void emit_alloc_rows(void) {
    for (uint32_t i = 0; i < alloc_recorded; i++) {
        telemetry_emit("ALLOC,%u,%u,%u\n", (unsigned)(i + 1),
                       (unsigned)alloc_samples[i].allocs,
                       (unsigned)alloc_samples[i].elapsed_us);
    }
}

static void emit_fullscan_rows(void) {
    for (uint32_t i = 0; i < fullscan_recorded; i++) {
        const telemetry_fullscan* s = &fullscan_samples[i];
        telemetry_emit("FULLSCAN,%u,%u,%u,%u,%u,%u\n", (unsigned)(i + 1),
                       (unsigned)s->total_slots,
                       (unsigned)s->allocated_slots,
                       (unsigned)s->free_slots,
                       (unsigned)s->free_regions,
                       (unsigned)s->longest_free_region);
    }
}

void telemetry_flush(void) {
    /* Ports that need the console attached (USB CDC enumeration) do it
     * here. The guest program has finished, so waiting is free. */
    telemetry_pre_flush_hook();

    emission_crc = 0;

    telemetry_emit("#GRRTELEM,3,heap_slots,%u,pause_max,%u,frag_max,%u,"
                   "alloc_max,%u,fullscan_max,%u\n",
                   (unsigned)VM_HEAP_SLOTS, (unsigned)VM_TELEM_MAX_PAUSE,
                   (unsigned)VM_TELEM_MAX_FRAG, (unsigned)VM_TELEM_MAX_ALLOC,
                   (unsigned)VM_TELEM_MAX_FULLSCAN);

    if (bench_params.ran) {
        telemetry_emit("#GRRBENCH,batches,%u,per_batch,%u,min_slots,%u,"
                       "max_slots,%u,seed,%u\n",
                       (unsigned)bench_params.batches,
                       (unsigned)bench_params.per_batch,
                       (unsigned)bench_params.min_slots,
                       (unsigned)bench_params.max_slots,
                       (unsigned)bench_params.seed);
    }

    emit_pause_rows();
    emit_frag_rows();
    emit_alloc_rows();
    emit_fullscan_rows();

    /* Raw counts (recorded, attempted) so dropped samples are visible. */
    telemetry_emit_raw("#GRRTELEM,END,pause,%u,%u,frag,%u,%u,alloc,%u,%u,"
                       "fullscan,%u,%u,crc,0x%08X\n",
                       (unsigned)pause_recorded, (unsigned)pause_total,
                       (unsigned)frag_recorded, (unsigned)frag_total,
                       (unsigned)alloc_recorded, (unsigned)alloc_total,
                       (unsigned)fullscan_recorded, (unsigned)fullscan_total,
                       (unsigned)emission_crc);
}

/* Deprecated per-stream dumps: rows only, no header/CRC. Kept so existing
 * guest programs still work; new programs should call telemetry_flush(). */
void dump_gc_runtime_telemetry(void) {
    emission_crc = 0;
    emit_pause_rows();
}

void dump_ext_frag_telemetry(void) {
    emission_crc = 0;
    emit_frag_rows();
}

/* ---------------- Sampling ---------------- */

void telemetry_record_full_heap_scan(const VM_FullHeapStats* heap_stats) {
    if (heap_stats == NULL) return;

    if (fullscan_recorded < VM_TELEM_MAX_FULLSCAN) {
        telemetry_fullscan* s = &fullscan_samples[fullscan_recorded];
        s->total_slots          = heap_stats->total_slots;
        s->allocated_slots      = heap_stats->allocated_slots;
        s->free_slots           = heap_stats->free_slots;
        s->free_regions         = heap_stats->free_regions;
        s->longest_free_region  = heap_stats->longest_free_region;
        fullscan_recorded++;
    }
    fullscan_total++;
}

float measure_ext_frag(VM_HeapStats heap_stats) {
    word total_free = heap_stats.buddy_free_slots + heap_stats.slab_free_slots;

    /* A full heap has no fragmentation figure at all. Returning 0.0 here
     * (as the previous version did) reported the worst possible moment as
     * "perfectly unfragmented". */
    if (total_free == 0) return (float)NAN;

    word longest = heap_stats.longest_buddy_block > heap_stats.longest_slab_block
                 ? heap_stats.longest_buddy_block
                 : heap_stats.longest_slab_block;

    return 1.0f - ((float)longest / (float)total_free);
}

float measure_full_heap_frag(VM_FullHeapStats heap_stats) {
    /* Same NAN guard as measure_ext_frag(): a heap with nothing free has no
     * fragmentation figure, and 0.0 would read as "perfectly packed". */
    if (heap_stats.free_slots == 0) return (float)NAN;

    return 1.0f - ((float)heap_stats.longest_free_region
                   / (float)heap_stats.free_slots);
}

void pre_gc_telemetry(VM* vm) {
    /* Ef first: vm_heap_stats() walks the forest heads and slab caches and
     * must stay OUTSIDE the timed window opened below. */
    if (telemetry_flag_is_set(TELEMETRY_EXT_FRAG)
        && frag_recorded < VM_TELEM_MAX_FRAG) {

        VM_HeapStats stats;
        vm_heap_stats(vm, &stats);

        telemetry_frag* f = &frag_samples[frag_recorded];
        f->pre_buddy_free = stats.buddy_free_slots;
        f->pre_slab_free  = stats.slab_free_slots;
        f->pre_longest    = stats.longest_buddy_block > stats.longest_slab_block
                          ? stats.longest_buddy_block : stats.longest_slab_block;
        frag_pending = 1;
    }

    if (telemetry_flag_is_set(TELEMETRY_GC_RUNTIME)) {
        pause_start_us = hal_clock_us();
        pause_pending = 1;
    }
}

void post_gc_telemetry(VM* vm) {
    /* Close the timed window before touching heap statistics. */
    if (telemetry_flag_is_set(TELEMETRY_GC_RUNTIME) && pause_pending) {
        uint32_t now = hal_clock_us();
        uint32_t delta = now - pause_start_us; /* wrap-safe */
        pause_pending = 0;

        if (pause_recorded < VM_TELEM_MAX_PAUSE) {
            pause_samples[pause_recorded].delta_us = delta;
            pause_recorded++;
        }
        pause_total++;
    }

    if (telemetry_flag_is_set(TELEMETRY_EXT_FRAG)) {
        VM_HeapStats stats;
        vm_heap_stats(vm, &stats);

        if (frag_pending && frag_recorded < VM_TELEM_MAX_FRAG) {
            telemetry_frag* f = &frag_samples[frag_recorded];
            f->post_buddy_free = stats.buddy_free_slots;
            f->post_slab_free  = stats.slab_free_slots;
            f->post_longest    = stats.longest_buddy_block > stats.longest_slab_block
                               ? stats.longest_buddy_block : stats.longest_slab_block;
            frag_recorded++;
        }
        frag_pending = 0;
        frag_total++;
    }
}

/* ---------------- Allocation benchmark ---------------- */

uint32_t telemetry_bench_alloc(VM* vm, word batches, word per_batch,
                               word min_slots, word max_slots, g_u32 seed) {
    /* References held live during a batch. Static and bounded so the
     * benchmark cannot itself grow the heap it is measuring. */
    static word live_refs[VM_TELEM_BENCH_MAX_LIVE];

    if (vm == NULL || batches == 0 || per_batch == 0) return 0;

    if (per_batch > VM_TELEM_BENCH_MAX_LIVE) per_batch = VM_TELEM_BENCH_MAX_LIVE;
    if (max_slots < min_slots) max_slots = min_slots;
    if (min_slots == 0) min_slots = 1;

    bench_params.batches   = batches;
    bench_params.per_batch = per_batch;
    bench_params.min_slots = min_slots;
    bench_params.max_slots = max_slots;
    bench_params.seed      = seed ? seed : 1u;
    bench_params.ran       = 1;

    g_u32 state = bench_params.seed;
    word span = max_slots - min_slots + 1;

    for (word b = 0; b < batches; b++) {
        word live = 0;

        uint32_t t0 = hal_clock_us();

        for (word i = 0; i < per_batch; i++) {
            /* Same LCG as the replay script: state = a*state + c. */
            state = state * 1664525u + 1013904223u;
            word n_slots = min_slots + (word)(state % span);

            /* Fast path on purpose: this measures allocator latency, not
             * collection. Runs that hit GC are the pause stream's job. */
            prim_val ref = g_malloc_direct(vm, n_slots, 0);
            if (get_prim_type(ref) != TYPE_REFERENCE) break;

            live_refs[live++] = ref.data;
        }

        uint32_t elapsed = hal_clock_us() - t0;

        /* Frees happen after the timer stops - outside the measured
         * region - so the batch length is pure allocation cost. */
        for (word i = 0; i < live; i++) {
            g_free_direct(vm, live_refs[i]);
        }

        if (alloc_recorded < VM_TELEM_MAX_ALLOC) {
            alloc_samples[alloc_recorded].allocs     = live;
            alloc_samples[alloc_recorded].elapsed_us = elapsed;
            alloc_recorded++;
        }
        alloc_total++;
    }

    return alloc_recorded;
}

/* ---------------- Port hook ---------------- */

#if defined(__GNUC__) || defined(__clang__)
#define GRR_WEAK __attribute__((weak))
#else
#define GRR_WEAK
#endif

GRR_WEAK void telemetry_pre_flush_hook(void) {
    /* Default: nothing to wait for. Ports override with a strong
     * definition (see src/ports/pico/init.c). */
}
