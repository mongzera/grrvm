/* Negative tests for the ram[]-store invariant.
 *
 * GRRVM's GC scans only op_stack / call_stack_frame per thread plus the
 * global heap block header itself. A TYPE_REFERENCE stored inside vm->ram[]
 * would therefore be untraced: its target gets swept while still reachable
 * from the holder. The ISA has no instruction for that store, but before
 * the guard, H_ALLOC + H_STORE could construct it on an uninitialized
 * (TYPE_NULL) slot. These tests pin the enforcement.
 *
 * NDEBUG is defined here on purpose: in a Debug build vm_mem.h carries a
 * tripwire assert() that aborts the process on the first violation, which
 * would kill the test instead of letting it observe STATE_ERROR. This TU
 * therefore verifies the release-build (graceful) enforcement path.
 */
#define NDEBUG
#include <stdio.h>
#include <string.h>
#include "grrvm/prim_val.h"
#include "grrvm/vm.h"
#include "grrvm/vm_alloc.h"
#include "grrvm/vm_mem.h"

/* VM is too big for the stack (ram[VM_HEAP_SLOTS]) - one static instance.
 * vm_alloc_init() clears vm->ram but does not allocate it (vm_create does),
 * so the test owns the allocation here. */
static VM vm;
static prim_val ram_storage[VM_HEAP_SLOTS];

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); g_pass++; } \
    else      { printf("  [FAIL] %s  (line %d)\n", msg, __LINE__); g_fail++; } \
} while (0)

static void reset_vm(void) {
    memset(&vm, 0, sizeof(vm));
    memset(ram_storage, 0, sizeof(ram_storage));
    vm.ram = ram_storage;
    vm_alloc_init(&vm);
}

/* ------------------------------------------------------------------
 * 1. The exact reproducer from the review: H_ALLOC A, H_ALLOC B, then
 *    H_STORE A's reference into B. B's slots are TYPE_NULL (raw block,
 *    no H_SET_TYPE), so before the guard set_vm_mem's identity path
 *    stored the reference intact. Now: STATE_ERROR, slot untouched.
 * ------------------------------------------------------------------ */
static void test_ref_store_rejected_on_raw_block(void) {
    printf("test_ref_store_rejected_on_raw_block\n");
    reset_vm();

    prim_val block = g_malloc_direct(&vm, 4, 0);
    CHECK(get_prim_type(block) == TYPE_REFERENCE, "setup: g_malloc_direct returns a reference");

    prim_val ref_value = make_prim_val(1234, STATE_OPEN, TYPE_REFERENCE);
    prim_state r = set_vm_mem(&vm, block.data, 0, ref_value);
    CHECK(r == STATE_ERROR, "storing a reference into a raw heap slot is rejected");

    prim_val out;
    int ok = get_vm_mem(&vm, block.data, &out);
    CHECK(ok && get_prim_type(out) == TYPE_NULL, "slot content unchanged after rejected store");
}

/* ------------------------------------------------------------------
 * 2. Typed slots reject references too (previously via the rank check
 *    TYPE_REFERENCE=7 > TYPE_U32=6; now the guard rejects any reference
 *    before the slot is even inspected). Pin the behavior either way.
 * ------------------------------------------------------------------ */
static void test_ref_store_rejected_on_typed_block(void) {
    printf("test_ref_store_rejected_on_typed_block\n");
    reset_vm();

    prim_val block = g_malloc_direct(&vm, 4, 0);
    CHECK(set_heap_block_type(&vm, block.data, TYPE_U32) == STATE_OPEN,
          "setup: H_SET_TYPE uint32 succeeds on a raw block");

    prim_val ref_value = make_prim_val(1234, STATE_OPEN, TYPE_REFERENCE);
    prim_state r = set_vm_mem(&vm, block.data, 1, ref_value);
    CHECK(r == STATE_ERROR, "storing a reference into a typed heap slot is rejected");

    prim_val out;
    get_vm_mem(&vm, block.data + 1, &out);
    CHECK(get_prim_type(out) == TYPE_U32, "typed slot remains uint32 after rejected store");
}

/* ------------------------------------------------------------------
 * 3. The guard must not disturb ordinary traffic: a numeric store into
 *    a raw slot still takes the TYPE_NULL identity path, and the
 *    promotion path (int8 into a typed uint32 slot) still works.
 * ------------------------------------------------------------------ */
static void test_numeric_stores_still_work(void) {
    printf("test_numeric_stores_still_work\n");
    reset_vm();

    prim_val raw = g_malloc_direct(&vm, 4, 0);
    prim_val v = make_prim_val(42, STATE_OPEN, TYPE_U32);
    CHECK(set_vm_mem(&vm, raw.data, 0, v) == STATE_OPEN, "u32 store into raw slot still succeeds");
    prim_val out;
    int ok = get_vm_mem(&vm, raw.data, &out);
    CHECK(ok && out.data == 42 && get_prim_type(out) == TYPE_U32, "value round-trips");

    prim_val typed = g_malloc_direct(&vm, 4, 0);
    CHECK(set_heap_block_type(&vm, typed.data, TYPE_U32) == STATE_OPEN, "H_SET_TYPE uint32 succeeds");
    prim_val small = make_prim_val(5, STATE_OPEN, TYPE_I8);
    CHECK(set_vm_mem(&vm, typed.data, 0, small) == STATE_OPEN, "int8 store into u32 slot promoted");
    get_vm_mem(&vm, typed.data, &out);
    CHECK(out.data == 5 && get_prim_type(out) == TYPE_U32, "promoted value stored as uint32");
}

/* ------------------------------------------------------------------
 * 4. Regression for the set_heap_block_type bounds fix: the old loop
 *    bounded on memslot->data - the PACKED metadata word (block_size<<16
 *    | n_slots, e.g. 0x0008_0004) - so it rewrote tens of thousands of
 *    slots past the allocation, corrupting the neighboring block (and
 *    writing past ram[] entirely). It must convert exactly the logical
 *    n_slots data slots and leave the neighbor's header alone.
 * ------------------------------------------------------------------ */
static void test_set_type_converts_exactly_block(void) {
    printf("test_set_type_converts_exactly_block\n");
    reset_vm();

    prim_val block = g_malloc_direct(&vm, 4, 0);  /* 4 + 1 header -> 8-slot slab class */
    prim_val guard = g_malloc_direct(&vm, 4, 0);  /* next 8-slot sub-block */

    word header = block.data - 1;
    word block_size = get_heap_block_size(vm.ram[header].data);
    CHECK(block_size == 8, "setup: 4-slot request + header fits an 8-slot slab class");
    CHECK(guard.data == block.data + block_size, "setup: guard block sits right after block");

    CHECK(set_heap_block_type(&vm, block.data, TYPE_U32) == STATE_OPEN, "H_SET_TYPE succeeds");

    for (word i = 0; i < 4; i++) {
        if (get_prim_type(vm.ram[block.data + i]) != TYPE_U32) {
            CHECK(0, "data slot converted to uint32");
            break;
        }
        if (i == 3) CHECK(1, "all 4 data slots converted to uint32");
    }
    /* padding inside this sub-block: header(+0) + 4 data(+1..4) leaves
     * +5..+7; block.data+7 would already be the neighbor's header slot */
    for (word i = 4; i < block_size - 1; i++) {
        if (get_prim_type(vm.ram[block.data + i]) != TYPE_NULL) {
            CHECK(0, "padding slot left untouched");
            break;
        }
        if (i == block_size - 2) CHECK(1, "padding slots (4..6) left untouched");
    }

    CHECK(get_prim_type(vm.ram[guard.data - 1]) == TYPE_HEAP_BLOCK,
          "neighboring block header intact after H_SET_TYPE");
    CHECK(get_n_slots(vm.ram[guard.data - 1].data) == 4,
          "neighboring block metadata intact after H_SET_TYPE");
}

int main(void) {
    test_ref_store_rejected_on_raw_block();
    test_ref_store_rejected_on_typed_block();
    test_numeric_stores_still_work();
    test_set_type_converts_exactly_block();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
