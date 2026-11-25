/* Unit tests for the H_SET_TYPE opcode (src/core/evaluation/memory_heap.c).
 *
 * These drive the evaluator itself - pop the reference, fetch the datatype
 * immediate from the program, decode it, retag the block - rather than
 * calling set_heap_block_type() directly. The helper's slot conversion is
 * already pinned by test_ref_guard; what is covered here is the opcode
 * around it:
 *
 *   - every datatype code the VM can represent retags the logical slots
 *   - codes without a prim_type are rejected before any slot is touched
 *   - an empty operand stack and a non-heap-block reference both
 *     deactivate the thread instead of corrupting the heap
 *
 * The VM is too big for the stack (ram[VM_HEAP_SLOTS]) - one static
 * instance, with the runner's shared heap arena (see test_suite.h).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grrvm/evaluation.h"
#include "grrvm/opcodes.h"
#include "grrvm/prim_val.h"
#include "grrvm/vm.h"
#include "grrvm/vm_alloc.h"
#include "grrvm/vm_mem.h"
#include "grrvm/vm_thread.h"
#include "test_suite.h"

static VM vm;
static prim_val* ram_storage = NULL;
static word program[8];

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); g_pass++; } \
    else      { printf("  [FAIL] %s  (line %d)\n", msg, __LINE__); g_fail++; } \
} while (0)

/* VM_Thread is stack-sized by the port config, so allocate one per test.
 * The field values are what vm_new_thread() sets: an empty operand stack
 * is sp == -1 (pop_stack() only reports underflow below zero). */
static VM_Thread* make_thread(void) {
    VM_Thread* t = (VM_Thread*)calloc(1, sizeof(VM_Thread));
    if (t == NULL) return NULL;
    t->vm = &vm;
    t->sp = -1;
    t->csp = -1;
    set_thread_active(t);
    return t;
}

static void reset_vm(void) {
    memset(&vm, 0, sizeof(vm));
    memset(ram_storage, 0, sizeof(prim_val) * VM_HEAP_SLOTS);
    memset(program, 0, sizeof(program));
    vm.ram = ram_storage;
    vm.program = program;
    vm.program_size = (word)(sizeof(program) / sizeof(program[0]));
    vm_alloc_init(&vm);
}

/* Runs H_SET_TYPE <datatype> against a freshly allocated block and returns
 * the reference. The datatype immediate is fetched from program[0]. */
static prim_val run_set_type(VM_Thread* thread, prim_val reference, word datatype) {
    program[0] = datatype;
    thread->pc = 0;
    push_stack(thread, reference);
    eval_memory_heap_operand(thread, H_SET_TYPE);
    return reference;
}

/* ------------------------------------------------------------------
 * 1. Each datatype code the assembler is allowed to emit must retag
 *    every logical slot of the block. The block header stays intact so
 *    the allocator can still free it later.
 * ------------------------------------------------------------------ */
static void test_supported_datatypes_retag_block(void) {
    static const struct { word code; prim_type type; const char* name; } cases[] = {
        { 0x01, TYPE_U8,    "uint8"  },
        { 0x02, TYPE_U16,   "uint16" },
        { 0x03, TYPE_U32,   "uint32" },
        { 0x05, TYPE_I8,    "int8"   },
        { 0x06, TYPE_I16,   "int16"  },
        { 0x07, TYPE_I32,   "int32"  },
        { 0x09, TYPE_FLOAT, "float"  },
    };

    printf("test_supported_datatypes_retag_block\n");

    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        char msg[80];
        reset_vm();
        VM_Thread* thread = make_thread();
        if (thread == NULL) { CHECK(0, "could not allocate a VM_Thread"); return; }

        prim_val block = g_malloc_direct(&vm, 4, 0);
        prim_val ref = run_set_type(thread, block, cases[c].code);

        snprintf(msg, sizeof msg, "H_SET_TYPE %s leaves the thread active", cases[c].name);
        CHECK(is_thread_active(thread), msg);

        int all_tagged = 1;
        for (word i = 0; i < 4; i++) {
            if (get_prim_type(vm.ram[block.data + i]) != cases[c].type) all_tagged = 0;
        }
        snprintf(msg, sizeof msg, "all 4 slots retagged as %s", cases[c].name);
        CHECK(all_tagged, msg);

        snprintf(msg, sizeof msg, "block header intact after H_SET_TYPE %s", cases[c].name);
        CHECK(get_prim_type(vm.ram[block.data - 1]) == TYPE_HEAP_BLOCK &&
              get_n_slots(vm.ram[block.data - 1].data) == 4, msg);

        /* the retag must not run past the logical slots into the slab padding */
        snprintf(msg, sizeof msg, "padding slot untouched by H_SET_TYPE %s", cases[c].name);
        CHECK(get_prim_type(vm.ram[block.data + 4]) == TYPE_NULL, msg);

        /* pc consumed exactly the datatype immediate */
        CHECK(thread->pc == 1, "pc advanced past the datatype immediate");
        CHECK(thread->sp == -1, "reference was popped off the operand stack");

        (void)ref;
        g_free(thread, block);
        free(thread);
    }
}

/* ------------------------------------------------------------------
 * 2. Codes with no prim_type (uint64/int64/double/char and anything
 *    unknown) must be rejected: thread inactive, block still raw.
 *    This is the VM-side half of the assembler's immediate validation.
 * ------------------------------------------------------------------ */
static void test_unsupported_datatypes_rejected(void) {
    static const word codes[] = { 0x00, 0x04, 0x08, 0x0A, 0x0B, 0x7F, 0xFF };
    char msg[96];

    printf("test_unsupported_datatypes_rejected (one HEAP error line per code is expected)\n");

    for (size_t c = 0; c < sizeof(codes) / sizeof(codes[0]); c++) {
        reset_vm();
        VM_Thread* thread = make_thread();
        if (thread == NULL) { CHECK(0, "could not allocate a VM_Thread"); return; }

        prim_val block = g_malloc_direct(&vm, 4, 0);
        run_set_type(thread, block, codes[c]);

        snprintf(msg, sizeof msg, "code 0x%02X deactivates the thread",
                 (unsigned)codes[c]);
        CHECK(!is_thread_active(thread), msg);

        int untouched = 1;
        for (word i = 0; i < 4; i++) {
            if (get_prim_type(vm.ram[block.data + i]) != TYPE_NULL) untouched = 0;
        }
        snprintf(msg, sizeof msg, "code 0x%02X leaves the block untagged",
                 (unsigned)codes[c]);
        CHECK(untouched, msg);

        g_free(thread, block);
        free(thread);
    }
}

/* ------------------------------------------------------------------
 * 3. H_SET_TYPE with nothing on the operand stack must fail cleanly.
 * ------------------------------------------------------------------ */
static void test_stack_underflow_rejected(void) {
    printf("test_stack_underflow_rejected (one HEAP error line is expected)\n");
    reset_vm();
    VM_Thread* thread = make_thread();
    if (thread == NULL) { CHECK(0, "could not allocate a VM_Thread"); return; }

    prim_val block = g_malloc_direct(&vm, 4, 0);
    program[0] = 0x03;
    thread->pc = 0;

    eval_memory_heap_operand(thread, H_SET_TYPE);

    CHECK(!is_thread_active(thread), "empty operand stack deactivates the thread");
    CHECK(get_prim_type(vm.ram[block.data - 1]) == TYPE_HEAP_BLOCK,
          "target block header untouched after underflow");
    CHECK(get_prim_type(vm.ram[block.data]) == TYPE_NULL,
          "target block slots untouched after underflow");

    g_free(thread, block);
    free(thread);
}

/* ------------------------------------------------------------------
 * 4. A reference that does not point at a heap block (an interior
 *    slot, or address 0) must be rejected by set_heap_block_type and
 *    surface as a deactivated thread.
 * ------------------------------------------------------------------ */
static void test_non_heap_reference_rejected(void) {
    printf("test_non_heap_reference_rejected (two HEAP error lines are expected)\n");

    for (int pass = 0; pass < 2; pass++) {
        reset_vm();
        VM_Thread* thread = make_thread();
        if (thread == NULL) { CHECK(0, "could not allocate a VM_Thread"); return; }

        prim_val block = g_malloc_direct(&vm, 4, 0);
        prim_val bad = (pass == 0)
            ? make_prim_val(block.data + 1, STATE_OPEN, TYPE_REFERENCE) /* interior slot */
            : make_prim_val(0, STATE_OPEN, TYPE_REFERENCE);             /* address 0 */

        run_set_type(thread, bad, 0x03);

        CHECK(!is_thread_active(thread),
              pass == 0 ? "interior reference deactivates the thread"
                        : "address 0 deactivates the thread");

        int untouched = 1;
        for (word i = 0; i < 4; i++) {
            if (get_prim_type(vm.ram[block.data + i]) != TYPE_NULL) untouched = 0;
        }
        CHECK(untouched,
              pass == 0 ? "interior reference retags nothing"
                        : "address 0 retags nothing");

        g_free(thread, block);
        free(thread);
    }
}

int test_h_set_type(void) {
    ram_storage = test_heap_arena();
    if (ram_storage == NULL) {
        printf("  [FAIL] could not allocate the VM heap\n");
        return 1;
    }

    test_supported_datatypes_retag_block();
    test_unsupported_datatypes_rejected();
    test_stack_underflow_rejected();
    test_non_heap_reference_rejected();

    ram_storage = NULL;

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
