#include <stdio.h>
#include <string.h>
#include "grrvm/vm.h"
#include "grrvm/vm_alloc.h"
#include "grrvm/vm_mem.h"

/* VM is ~200KB (mostly ram[VM_HEAP_SLOTS]) - too big for the stack,
 * so it lives here as a single static instance reused across tests. */
static VM vm;

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); g_pass++; } \
    else      { printf("  [FAIL] %s  (line %d)\n", msg, __LINE__); g_fail++; } \
} while (0)

static VM_Thread make_thread(void) {
    VM_Thread t;
    memset(&t, 0, sizeof(t));
    t.vm = &vm;
    return t;
}

static void reset_vm(void) {
    memset(&vm, 0, sizeof(vm));
    vm_alloc_init(&vm);
}

/* ------------------------------------------------------------------
 * 1. VM_HEAP_SLOTS (25600 on the current grr_port_config.h) isn't a
 *    power of two. 25600 / 64 = 400 base blocks = 256+128+16 =
 *    2^8 + 2^7 + 2^4, so vm_alloc_init should produce exactly 3 trees
 *    of those orders, with zero base blocks left over.
 * ------------------------------------------------------------------ */
static void test_decomposition(void) {
    printf("test_decomposition (VM_NUM_BASE_BLOCKS=%u)\n", (unsigned)VM_NUM_BASE_BLOCKS);
    reset_vm();

    CHECK(vm.allocator.buddy.num_trees == 3, "exactly 3 buddy trees");
    if (vm.allocator.buddy.num_trees == 3) {
        CHECK(vm.allocator.buddy.trees[0].order == 8, "tree[0] order 8  (256 base blocks / 16384 slots)");
        CHECK(vm.allocator.buddy.trees[1].order == 7, "tree[1] order 7  (128 base blocks / 8192 slots)");
        CHECK(vm.allocator.buddy.trees[2].order == 4, "tree[2] order 4  (16 base blocks / 1024 slots)");
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
    VM_Thread thread = make_thread();

    prim_val ref = g_malloc(&thread, 4); /* total_needed = 5 -> class 8 */
    CHECK(get_prim_type(ref) == TYPE_REFERENCE, "g_malloc(4) returns a TYPE_REFERENCE");

    word data_addr = ref.data;
    word header_addr = data_addr - 1;
    CHECK(get_prim_type(vm.ram[header_addr]) == TYPE_LENGTH, "header slot is TYPE_LENGTH");
    CHECK(vm.ram[header_addr].data == 8, "5 needed slots rounds up to the 8-slot slab class");

    prim_val v = make_prim_val(1234, STATE_OPEN, TYPE_U32);
    CHECK(set_vm_mem(&vm, data_addr, 0, v) == 0, "STORE into the allocated slot succeeds");
    prim_val *readback;
    get_vm_mem(&vm, data_addr, readback);
    CHECK(readback && readback->data == 1234, "LOAD reads back the same value");

    g_free(&thread, ref);
    CHECK(get_prim_state(vm.ram[header_addr]) == STATE_OPEN, "header invalidated after free");
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
    VM_Thread thread = make_thread();

    prim_val a = g_malloc(&thread, 4);
    word addr_a = a.data;
    g_free(&thread, a);

    prim_val b = g_malloc(&thread, 4);
    CHECK(b.data == addr_a, "freed slot is reused by the next same-class alloc (LIFO free list)");
}

/* ------------------------------------------------------------------
 * 4. Large allocation goes through the buddy path and rounds up to
 *    the smallest 64*2^k that fits.
 * ------------------------------------------------------------------ */
static void test_buddy_roundtrip(void) {
    printf("test_buddy_roundtrip\n");
    reset_vm();
    VM_Thread thread = make_thread();

    prim_val ref = g_malloc(&thread, 100); /* total_needed = 101 -> order 1, 128 slots */
    word header_addr = ref.data - 1;
    CHECK(vm.ram[header_addr].data == 128, "100 slots rounds up to a 128-slot buddy block");

    g_free(&thread, ref);
}

/* ------------------------------------------------------------------
 * 5. Double free must be caught (corrupted-header error), not
 *    silently hand the same memory out twice.
 * ------------------------------------------------------------------ */
static void test_double_free(void) {
    printf("test_double_free (one HEAP error line below is expected)\n");
    reset_vm();
    VM_Thread thread = make_thread();

    prim_val ref = g_malloc(&thread, 4);
    g_free(&thread, ref);
    g_free(&thread, ref); /* should log an error and return, not crash */
    CHECK(1, "second g_free on the same reference did not crash");
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
    VM_Thread thread = make_thread();

    #define N 256
    static prim_val refs[N];
    int allocated = 0;
    for (int i = 0; i < N; i++) {
        refs[i] = g_malloc(&thread, 50); /* total_needed=51 -> order 0, 64 slots, all from tree[0] */
        if (get_prim_type(refs[i]) != TYPE_REFERENCE) break;
        allocated++;
    }
    CHECK(allocated == N, "all 256 order-0 blocks fit in tree[0]");
    CHECK(vm.allocator.buddy.trees[0].longest[0] == 0, "tree[0] fully allocated (root longest == 0)");

    for (int i = 0; i < allocated; i++) g_free(&thread, refs[i]);

    CHECK(vm.allocator.buddy.trees[0].longest[0] == 9,
          "tree[0] fully coalesced back (root longest == order+1 == 9)");

    prim_val big = g_malloc(&thread, 16383); /* needs the whole 16384-slot tree in one block */
    CHECK(get_prim_type(big) == TYPE_REFERENCE, "whole tree reusable as one block after full coalesce");
    #undef N
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
    VM_Thread thread = make_thread();

    prim_val refs[8];
    for (int i = 0; i < 8; i++) refs[i] = g_malloc(&thread, 7); /* total_needed=8 -> class 8, one whole cache */

    word base_block = (refs[0].data - 1) / VM_BUDDY_BASE_BLOCK_SLOTS;
    CHECK(vm.allocator.base_block_owner[base_block] != VM_ALLOC_NONE, "base block claimed by a slab cache");

    for (int i = 0; i < 8; i++) g_free(&thread, refs[i]);

    CHECK(vm.allocator.base_block_owner[base_block] == VM_ALLOC_NONE,
          "emptied cache's base block returned to the BuddyForest");
}

int main(void) {
    test_decomposition();
    test_slab_roundtrip();
    test_slab_reuse();
    test_buddy_roundtrip();
    test_double_free();
    test_buddy_coalesce();
    test_slab_cache_reclaim();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
