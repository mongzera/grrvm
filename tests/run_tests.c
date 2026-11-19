/* In-process test runner shared by every port.
 *
 * Tests are never standalone executables (they used to each carry a main(),
 * which collided with the runtime's main). Instead each suite exposes
 * test_<name>() returning its failure count, and this runner executes the
 * suites compiled into the current build:
 *
 *   - POSIX: invoked by `grrvm --test` before normal argument handling.
 *   - Pico:  invoked at boot when the firmware is built with
 *            -DGRRVM_BUILD_TESTS=ON.
 */
#include <stdio.h>
#include <stdlib.h>

#include "grr_port_config.h"
#include "grrvm/debug/run_tests.h"
#include "test_suite.h"

#if defined(GRRVM_TEST_PICO)
#include <unistd.h>

extern char __StackLimit;

/* The Pico has no MMU and a single C heap that ends at __StackLimit, so a
 * full VM (200 KB ram + program + threads) uses nearly all of it. Print
 * what is left at each suite so an out-of-memory report is diagnosable
 * from the serial log. */
static void report_heap(const char* phase) {
    char* brk = (char*)sbrk(0);
    printf("[GRRVM] heap before %s: %u B free\n",
           phase, (unsigned)(&__StackLimit - brk));
}
#else
#define report_heap(phase) ((void)(phase))
#endif

static prim_val* arena = NULL;

prim_val* test_heap_arena(void) {
    if (arena == NULL) {
        arena = (prim_val*)malloc(sizeof(prim_val) * VM_HEAP_SLOTS);
    }
    return arena;
}

void test_heap_arena_release(void) {
    free(arena);
    arena = NULL;
}

static int run_suite(const char* name, int (*suite)(void)) {
    printf("\n--- %s ---\n", name);
    report_heap(name);
    int failures = suite();
    if (failures == 0) {
        printf("%s: OK\n", name);
    } else {
        printf("%s: %d check(s) FAILED\n", name, failures);
    }
    return failures;
}

int run_all_tests(void) {
    int failures = 0;

    printf("\n===== GRRVM system checks =====\n");

    failures += run_suite("load_vm", test_load_vm);
    failures += run_suite("sha256", test_sha256);
    failures += run_suite("program_transfer", test_program_transfer);
    failures += run_suite("ref_guard", test_ref_guard);
    failures += run_suite("heap", test_heap);

#if defined(GRRVM_TEST_POSIX)
    failures += run_suite("load_vm_file", test_load_vm_file);
#endif

    if (failures == 0) {
        printf("\n===== all checks passed =====\n");
    } else {
        printf("\n===== %d check(s) FAILED =====\n", failures);
    }

    /* Released last so the 200 KB block is not split by later allocations:
     * the runtime's vm_create() must be able to reuse it whole. */
    test_heap_arena_release();

    return failures;
}
