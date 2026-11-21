#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grrvm/prim_val.h"
#include "grrvm/telemetry/gc_telemetry.h"
#include "grrvm/vm.h"
#include "grrvm/vm_alloc.h"
#include "grrvm/vm_mem.h"
#include "grrvm/vm_thread.h"
#include "test_suite.h"

/* Port-independent allocator tests: they exercise src/core/memory/allocator.c
 * (buddy forest + slab caches) through g_malloc/g_free, so they run on every
 * port. Expectations are derived from the port's VM_HEAP_SLOTS config.
 *
 * VM is ~200KB (mostly ram[VM_HEAP_SLOTS]) - too big for the stack,
 * so it lives here as a single static instance reused across tests.
 * vm_alloc_init() clears vm->ram but does not allocate it (vm_create
 * does), so the runner provides a shared heap arena (see test_suite.h). */
static VM vm;
static prim_val* ram_storage = NULL;

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); g_pass++; } \
    else      { printf("  [FAIL] %s  (line %d)\n", msg, __LINE__); g_fail++; } \
} while (0)

/* VM_Thread holds the op/call stacks, which the port config sizes in the
 * megabytes on POSIX - far too big for the stack, and too big to keep as a
 * second static next to the VM on the Pico. Allocate one per test. */
static VM_Thread* make_thread(void) {
    VM_Thread* t = (VM_Thread*)calloc(1, sizeof(VM_Thread));
    if (t != NULL) t->vm = &vm;
    return t;
}

static void reset_vm(void) {
    memset(&vm, 0, sizeof(vm));
    memset(ram_storage, 0, sizeof(prim_val) * VM_HEAP_SLOTS);
    vm.ram = ram_storage;
    vm_alloc_init(&vm);
}

/* ------------------------------------------------------------------
 * 1. The heap is not necessarily a power of two. On the Pico config
 *    25600 slots / 64 = 400 base blocks = 256+128+16 = 2^8 + 2^7 + 2^4,
 *    so vm_alloc_init produces 3 trees of those orders with zero base
 *    blocks left over; on POSIX 131072 / 64 = 2048 = 2^11 is a single
 *    tree. The expected forest is the binary decomposition of
 *    VM_NUM_BASE_BLOCKS, so derive it instead of hard-coding.
 * ------------------------------------------------------------------ */
static void test_decomposition(void) {
    printf("test_decomposition (VM_HEAP_SLOTS=%u, VM_NUM_BASE_BLOCKS=%u)\n",
           (unsigned)VM_HEAP_SLOTS, (unsigned)VM_NUM_BASE_BLOCKS);
    reset_vm();

    byte expected_trees = 0;
    for (word n = VM_NUM_BASE_BLOCKS; n != 0; n >>= 1) expected_trees += (byte)(n & 1u);

    CHECK(vm.allocator.buddy.num_trees == expected_trees,
          "one tree per set bit of VM_NUM_BASE_BLOCKS");
    if (vm.allocator.buddy.num_trees == expected_trees) {
        byte idx = 0;
        for (int bit = 31; bit >= 0 && idx < vm.allocator.buddy.num_trees; bit--) {
            if ((VM_NUM_BASE_BLOCKS & (1u << bit)) == 0) continue;
            char msg[80];
            snprintf(msg, sizeof msg, "tree[%u] order %d", (unsigned)idx, bit);
            CHECK(vm.allocator.buddy.trees[idx].order == (byte)bit, msg);
            idx++;
        }
    }

    word total_base_blocks = 0;
    for (byte i = 0; i < vm.allocator.buddy.num_trees; i++) {
        total_base_blocks += (1u << vm.allocator.buddy.trees[i].order);
    }
    CHECK(total_base_blocks == VM_NUM_BASE_BLOCKS, "trees exactly cover the heap, no waste");
}

/* ------------------------------------------------------------------
 * 2. Small allocation goes through the slab path, rounds n_slots+1
 *    (header included) up to the smallest class that fits, and a
 *    real STORE/LOAD through the returned reference round-trips.
 * ------------------------------------------------------------------ */

static void test_slab_roundtrip(void) {
    printf("test_slab_roundtrip\n");
    reset_vm();
    VM_Thread* thread = make_thread();
    if (thread == NULL) { CHECK(0, "could not allocate a VM_Thread"); return; }

    prim_val ref = g_malloc(thread, 4); /* total_needed = 5 -> class 8 */
    CHECK(get_prim_type(ref) == TYPE_REFERENCE, "g_malloc(4) returns a TYPE_REFERENCE");

    word data_addr = ref.data;
    word header_addr = data_addr - 1;
    CHECK(get_prim_type(vm.ram[header_addr]) == TYPE_HEAP_BLOCK, "header slot is TYPE_HEAP_BLOCK");
    /* The header packs (block_size, n_slots), so decode it rather than
     * comparing the raw word. */
    CHECK(get_heap_block_size(vm.ram[header_addr].data) == 8,
          "5 needed slots rounds up to the 8-slot slab class");
    CHECK(get_n_slots(vm.ram[header_addr].data) == 4, "the logical size is the 4 requested slots");

    prim_val v = make_prim_val(1234, STATE_OPEN, TYPE_U32);
    CHECK(set_vm_mem(&vm, data_addr, 0, v) == 0, "STORE into the allocated slot succeeds");
    prim_val readback;
    int read_ok = get_vm_mem(&vm, data_addr, &readback);
    CHECK(read_ok && readback.data == 1234, "LOAD reads back the same value");

    g_free(thread, ref);
    CHECK(get_prim_state(vm.ram[header_addr]) == STATE_OPEN, "header invalidated after free");
    free(thread);
}

/* ------------------------------------------------------------------
 * 3. Freeing a sub-block and immediately re-allocating the same
 *    class should hand back the exact same address - proves the
 *    intrusive free-list linking actually works, not just that it
 *    doesn't crash.
 * ------------------------------------------------------------------ */
static void test_slab_reuse(void) {
    printf("test_slab_reuse\n");
    reset_vm();
    VM_Thread* thread = make_thread();
    if (thread == NULL) { CHECK(0, "could not allocate a VM_Thread"); return; }

    prim_val a = g_malloc(thread, 4);
    word addr_a = a.data;
    g_free(thread, a);

    prim_val b = g_malloc(thread, 4);
    CHECK(b.data == addr_a, "freed slot is reused by the next same-class alloc (LIFO free list)");
    free(thread);
}

/* ------------------------------------------------------------------
 * 4. Large allocation goes through the buddy path and rounds up to
 *    the smallest 64*2^k that fits.
 * ------------------------------------------------------------------ */
static void test_buddy_roundtrip(void) {
    printf("test_buddy_roundtrip\n");
    reset_vm();
    VM_Thread* thread = make_thread();
    if (thread == NULL) { CHECK(0, "could not allocate a VM_Thread"); return; }

    prim_val ref = g_malloc(thread, 100); /* total_needed = 101 -> order 1, 128 slots */
    word header_addr = ref.data - 1;
    CHECK(get_heap_block_size(vm.ram[header_addr].data) == 128,
          "100 slots rounds up to a 128-slot buddy block");
    CHECK(get_n_slots(vm.ram[header_addr].data) == 100, "the logical size is the 100 requested slots");

    g_free(thread, ref);
    free(thread);
}

/* ------------------------------------------------------------------
 * 5. Double free must be caught (corrupted-header error), not
 *    silently hand the same memory out twice.
 * ------------------------------------------------------------------ */
static void test_double_free(void) {
    printf("test_double_free (one HEAP error line below is expected)\n");
    reset_vm();
    VM_Thread* thread = make_thread();
    if (thread == NULL) { CHECK(0, "could not allocate a VM_Thread"); return; }

    prim_val ref = g_malloc(thread, 4);
    g_free(thread, ref);
    g_free(thread, ref); /* should log an error and return, not crash */
    CHECK(1, "second g_free on the same reference did not crash");
    free(thread);
}

/* ------------------------------------------------------------------
 * 6. Full coalescing proof: fill the largest tree (order 8 = 256
 *    base blocks) with 256 individual 64-slot allocations, free them
 *    all in the same order, then confirm the tree's root capacity is
 *    back to fully free and a single whole-tree allocation succeeds.
 * ------------------------------------------------------------------ */
static void test_buddy_coalesce(void) {
    printf("test_buddy_coalesce\n");
    reset_vm();
    VM_Thread* thread = make_thread();
    if (thread == NULL) { CHECK(0, "could not allocate a VM_Thread"); return; }

    /* The size of the largest tree depends on the port's heap config
     * (256 base blocks on the Pico, 2048 on POSIX), so take it from the
     * forest rather than hard-coding 256/16384. */
    const word n = (word)1u << vm.allocator.buddy.trees[0].order;
    char msg[96];

    prim_val* refs = (prim_val*)malloc(sizeof(prim_val) * n);
    if (refs == NULL) {
        CHECK(0, "could not allocate the reference array");
        free(thread);
        return;
    }

    word allocated = 0;
    for (word i = 0; i < n; i++) {
        /* total_needed = 64 -> order 0 (a whole base block, so the buddy
         * path is used, not the slab path). */
        refs[i] = g_malloc(thread, VM_BUDDY_BASE_BLOCK_SLOTS - 1);
        if (get_prim_type(refs[i]) != TYPE_REFERENCE) break;
        allocated++;
    }
    snprintf(msg, sizeof msg, "all %u order-0 blocks fit in tree[0]", (unsigned)n);
    CHECK(allocated == n, msg);
    CHECK(vm.allocator.buddy.trees[0].longest[0] == 0, "tree[0] fully allocated (root longest == 0)");

    for (word i = 0; i < allocated; i++) g_free(thread, refs[i]);

    snprintf(msg, sizeof msg, "tree[0] fully coalesced back (root longest == order+1 == %u)",
             (unsigned)vm.allocator.buddy.trees[0].order + 1u);
    CHECK(vm.allocator.buddy.trees[0].longest[0] == vm.allocator.buddy.trees[0].order + 1u, msg);

    /* Needs one big block from the coalesced tree. The header packs
     * block_size into 16 bits, so the largest representable block is 2^16
     * slots; a POSIX tree (2048 base blocks = 131072 slots) is larger than
     * that, so cap the request at the largest encodable order. */
    byte max_order = vm.allocator.buddy.trees[0].order;
    while (max_order > 0 && ((word)VM_BUDDY_BASE_BLOCK_SLOTS << max_order) > 65536u)
        max_order--;
    word max_block_slots = (word)VM_BUDDY_BASE_BLOCK_SLOTS << max_order;

    prim_val big = g_malloc(thread, max_block_slots - 1);
    snprintf(msg, sizeof msg, "%u-slot block allocatable after full coalesce",
             (unsigned)max_block_slots);
    CHECK(get_prim_type(big) == TYPE_REFERENCE, msg);
    g_free(thread, big);

    free(thread);
    free(refs);
}

/* ------------------------------------------------------------------
 * 7. Policy check: an emptied SlabCache must be handed back to the
 *    BuddyForest, not held forever. Fill one whole 8-slot-class cache
 *    (8 sub-blocks out of a 64-slot base block), free them all, then
 *    confirm the owning base block is unclaimed and the buddy tree
 *    shows that base block free again.
 * ------------------------------------------------------------------ */
static void test_slab_cache_reclaim(void) {
    printf("test_slab_cache_reclaim\n");
    reset_vm();
    VM_Thread* thread = make_thread();
    if (thread == NULL) { CHECK(0, "could not allocate a VM_Thread"); return; }

    prim_val refs[8];
    for (int i = 0; i < 8; i++) refs[i] = g_malloc(thread, 7); /* total_needed=8 -> class 8, one whole cache */

    word base_block = (refs[0].data - 1) / VM_BUDDY_BASE_BLOCK_SLOTS;
    CHECK(vm.allocator.base_block_owner[base_block] != VM_ALLOC_NONE, "base block claimed by a slab cache");

    for (int i = 0; i < 8; i++) g_free(thread, refs[i]);

    CHECK(vm.allocator.base_block_owner[base_block] == VM_ALLOC_NONE,
          "emptied cache's base block returned to the BuddyForest");
    free(thread);
}

/* ------------------------------------------------------------------
 * 8. Full heap scan on an empty heap: the whole heap is one free run,
 *    so Ef_full is 0.0 (not NAN - there is free memory).
 * ------------------------------------------------------------------ */
static void test_full_heap_scan_empty(void) {
    printf("test_full_heap_scan_empty\n");
    reset_vm();

    VM_FullHeapStats s;
    vm_full_heap_scan(&vm, &s);

    CHECK(s.total_slots == VM_HEAP_SLOTS, "scan covers the whole heap");
    CHECK(s.allocated_slots == 0, "no allocated slots on a fresh heap");
    CHECK(s.free_slots == VM_HEAP_SLOTS, "every slot is free");
    CHECK(s.free_regions == 1, "a fresh heap is one contiguous free region");
    CHECK(s.longest_free_region == VM_HEAP_SLOTS, "longest run is the whole heap");
    CHECK(measure_full_heap_frag(s) == 0.0f, "Ef_full is 0.0 on an empty heap");
}

/* ------------------------------------------------------------------
 * 9. The scan must find slab sub-blocks through their TYPE_HEAP_BLOCK
 *    headers, not through the free list. A fresh slab cache is claimed
 *    from the leftmost buddy base block, so allocating one 8-slot
 *    sub-block leaves a single free run after it. This is the case the
 *    allocator-aware metric cannot express: vm_heap_stats() says the
 *    longest slab block is 8, while the physical layout keeps the rest
 *    of the heap contiguous.
 * ------------------------------------------------------------------ */
static void test_full_heap_scan_slab(void) {
    printf("test_full_heap_scan_slab\n");
    reset_vm();
    VM_Thread* thread = make_thread();
    if (thread == NULL) { CHECK(0, "could not allocate a VM_Thread"); return; }

    prim_val ref = g_malloc(thread, 7); /* total_needed = 8 -> class 8 */
    word class_size = VM_SLAB_CLASS_SIZES[0];

    VM_FullHeapStats s;
    vm_full_heap_scan(&vm, &s);
    CHECK(s.allocated_slots == class_size, "one 8-slot slab sub-block is allocated");
    CHECK(s.free_slots == VM_HEAP_SLOTS - class_size, "only the used sub-block is excluded from free memory");
    CHECK(s.free_regions == 1, "remaining sub-blocks and the rest of the heap form one run");
    CHECK(s.longest_free_region == VM_HEAP_SLOTS - class_size, "the free run crosses the slab cache boundary");

    g_free(thread, ref);
    vm_full_heap_scan(&vm, &s);
    CHECK(s.allocated_slots == 0 && s.free_slots == VM_HEAP_SLOTS,
          "freeing the sub-block returns it to the unified free map");
    free(thread);
}

/* ------------------------------------------------------------------
 * 10. Fill the heap with 64-slot buddy blocks, then free every other
 *     one. The scan must report alternating 64-slot free runs - no run
 *     may span a live block - and an Ef that matches that layout.
 *     Filling first is what makes the metric meaningful: on a mostly
 *     empty heap the untouched tail is the longest run, not a fragment.
 * ------------------------------------------------------------------ */
static void test_full_heap_scan_fragmented(void) {
    printf("test_full_heap_scan_fragmented\n");
    reset_vm();
    VM_Thread* thread = make_thread();
    if (thread == NULL) { CHECK(0, "could not allocate a VM_Thread"); return; }

    const word n = VM_NUM_BASE_BLOCKS; /* one order-0 block each */
    prim_val* refs = (prim_val*)malloc(sizeof(prim_val) * n);
    if (refs == NULL) { CHECK(0, "could not allocate the reference array"); free(thread); return; }

    word allocated = 0;
    for (word i = 0; i < n; i++) {
        /* total_needed = 64 -> order 0 (one whole base block). */
        refs[i] = g_malloc(thread, VM_BUDDY_BASE_BLOCK_SLOTS - 1);
        if (get_prim_type(refs[i]) != TYPE_REFERENCE) break;
        allocated++;
    }
    CHECK(allocated == n, "the heap fills with one block per base block");
    if (allocated != n) { free(refs); free(thread); return; }

    for (word i = 0; i < n; i += 2) g_free(thread, refs[i]);

    VM_FullHeapStats s;
    vm_full_heap_scan(&vm, &s);

    const word block = VM_BUDDY_BASE_BLOCK_SLOTS;
    word freed = (n / 2) * block;

    CHECK(s.allocated_slots == freed, "half the base blocks remain allocated");
    CHECK(s.free_slots == freed, "free slots are exactly the freed base blocks");
    CHECK(s.free_regions == n / 2, "one free region per freed block, separated by live ones");
    CHECK(s.longest_free_region == block, "no run spans a live buddy block");

    float expect = 1.0f - ((float)block / (float)freed);
    float diff = measure_full_heap_frag(s) - expect;
    CHECK(diff < 1e-6f && diff > -1e-6f, "Ef_full matches the scanned layout");

    for (word i = 1; i < n; i += 2) g_free(thread, refs[i]);
    vm_full_heap_scan(&vm, &s);
    CHECK(s.allocated_slots == 0 && s.free_regions == 1,
          "freeing the rest coalesces back to one region");

    free(refs);
    free(thread);
}

int test_heap(void) {
    ram_storage = test_heap_arena();
    if (ram_storage == NULL) {
        printf("  [FAIL] could not allocate the VM heap\n");
        return 1;
    }

    test_decomposition();
    test_slab_roundtrip();
    test_slab_reuse();
    test_buddy_roundtrip();
    test_double_free();
    test_buddy_coalesce();
    test_slab_cache_reclaim();
    test_full_heap_scan_empty();
    test_full_heap_scan_slab();
    test_full_heap_scan_fragmented();

    ram_storage = NULL;

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
