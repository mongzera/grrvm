#include "native_wrappers/internal_telemetry_wrapper.h"
#include "grrvm/hal/vm_hal_timer.h"
#include "grrvm/telemetry/gc_telemetry.h"
#include "grrvm/vm_thread.h"
#include "grrvm/vm_log.h"
#include "grrvm/vm_native.h"
#include "grrvm/vm_thread.h"

void wrap_set_gc_runtime_telemetry_flag(VM_Thread* thread){
    prim_val flag;
    if(!pop_stack(thread, &flag)){
        vm_error("NATIVE", "Failed to pop stack for native gc runtime telemetry flag");
        return;
    }

    if(flag.data) {
        telemetry_flag_set(TELEMETRY_GC_RUNTIME);
    }else{
        telemetry_flag_clear(TELEMETRY_GC_RUNTIME);
    }

    vm_info("RUNTIME TELEMETRY", "Attempting to set GC runtime telemetry flag: %d", flag.data);
}

void wrap_dump_gc_runtime_telemetry(VM_Thread* thread){
    dump_gc_runtime_telemetry();
}

void wrap_set_ext_frag_telemetry_flag(VM_Thread* thread){
    prim_val flag;
    if(!pop_stack(thread, &flag)){
        vm_error("NATIVE", "Failed to pop stack for native external fragmentation telemetry flag");
        return;
    }

    if(flag.data) {
        telemetry_flag_set(TELEMETRY_EXT_FRAG);
    }else{
        telemetry_flag_clear(TELEMETRY_EXT_FRAG);
    }

    vm_info("EXTERNAL FRAG TELEMETRY", "Attempting to set external fragmentation telemetry flag: %d", flag.data);
}

void __wrap__timer_us(VM_Thread* thread){

    uint32_t us = hal_clock_us();
    if(!push_stack(thread, make_prim_val(us, STATE_OPEN, TYPE_U32))){
        vm_error("NATIVE", "Failed to pop stack for native timer_us");
        return;
    }

}

void wrap_dump_ext_frag_telemetry(VM_Thread* thread){
    dump_ext_frag_telemetry();
}

/* -- Evaluation control surface ----------------------------------------
 * None of these log while the guest runs: a vm_info() inside a timed
 * region would show up in the very samples it is meant to observe.
 * ------------------------------------------------------------------- */

void wrap_telemetry_reset(VM_Thread* thread){
    (void)thread;
    telemetry_reset_all();
}

void wrap_telemetry_flush(VM_Thread* thread){
    (void)thread;
    telemetry_flush();
}

/* full_heap_scan() - algorithm-independent Ef checkpoint.
 *
 * Runs vm_full_heap_scan() immediately (O(VM_HEAP_SLOTS)), stores a
 * FULLSCAN row for the next telemetry_flush(), and pushes the resulting
 * Ef_full as TYPE_FLOAT so the guest can branch on it right away. It reads
 * the block headers in ram[], so it measures a buddy allocation, a slab
 * sub-block and a linear block heap the same way. Nothing is printed here. */
void wrap_full_heap_scan(VM_Thread* thread){
    if(thread->vm == NULL){
        vm_error("NATIVE", "full_heap_scan called without a VM");
        return;
    }

    VM_FullHeapStats stats;
    vm_full_heap_scan(thread->vm, &stats);
    telemetry_record_full_heap_scan(&stats);

    prim_val result = make_prim_val(0, STATE_OPEN, TYPE_FLOAT);
    result.float_data = measure_full_heap_frag(stats);

    if(!push_stack(thread, result)){
        vm_error("NATIVE", "Failed to push stack for native full_heap_scan");
    }
}

/* set_gc_enabled(0) stops g_malloc from collecting, so external
 * fragmentation and fast-path latency can be measured on the allocator
 * alone. The flag is inverted in the VM struct (gc_disabled) so a zeroed
 * VM keeps collection on. */
void wrap_set_gc_enabled(VM_Thread* thread){
    prim_val flag;
    if(!pop_stack(thread, &flag)){
        vm_error("NATIVE", "Failed to pop stack for native set_gc_enabled");
        return;
    }

    if(thread->vm != NULL){
        thread->vm->gc_disabled = flag.data ? 0 : 1;
    }
}

/* bench_alloc(batches, per_batch, min_slots, max_slots, seed)
 * Arguments are pushed left-to-right, so they pop in reverse. */
void wrap_bench_alloc(VM_Thread* thread){
    prim_val batches, per_batch, min_slots, max_slots, seed;

    if(!pop_stack(thread, &seed)
       || !pop_stack(thread, &max_slots)
       || !pop_stack(thread, &min_slots)
       || !pop_stack(thread, &per_batch)
       || !pop_stack(thread, &batches)){
        vm_error("NATIVE", "bench_alloc needs 5 arguments: batches, "
                 "per_batch, min_slots, max_slots, seed");
        return;
    }

    /* prim_val.data is an unsigned word, so a negative guest argument
     * would already have wrapped to a huge positive value. Reinterpret as
     * signed and clamp: telemetry_bench_alloc() treats zero as "record
     * nothing", which is better than walking a 4-billion-iteration loop. */
    g_int  b   = (g_int)(int32_t)batches.data;
    g_int  per = (g_int)(int32_t)per_batch.data;
    g_int  lo  = (g_int)(int32_t)min_slots.data;
    g_int  hi  = (g_int)(int32_t)max_slots.data;

    telemetry_bench_alloc(thread->vm,
                          (word)(b   > 0 ? b   : 0),
                          (word)(per > 0 ? per : 0),
                          (word)(lo  > 0 ? lo  : 0),
                          (word)(hi  > 0 ? hi  : 0),
                          (g_u32)seed.data);
}

void register_internal_telemetry_wrappers(void) {
    native_register("set_gc_runtime_telemetry_flag", wrap_set_gc_runtime_telemetry_flag);
    native_register("dump_gc_runtime_telemetry", wrap_dump_gc_runtime_telemetry);

    native_register("set_ext_frag_telemetry_flag", wrap_set_ext_frag_telemetry_flag);
    native_register("dump_ext_frag_telemetry", wrap_dump_ext_frag_telemetry);

    native_register("timer_us", __wrap__timer_us);

    native_register("telemetry_reset", wrap_telemetry_reset);
    native_register("telemetry_flush", wrap_telemetry_flush);
    native_register("bench_alloc", wrap_bench_alloc);
    native_register("set_gc_enabled", wrap_set_gc_enabled);
    native_register("full_heap_scan", wrap_full_heap_scan);
}
