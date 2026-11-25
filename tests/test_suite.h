#ifndef GRRVM_TEST_SUITE_H
#define GRRVM_TEST_SUITE_H

#include "grrvm/prim_val.h"

/* Test entry points. Each one prints its own results and returns the
 * number of failed checks (0 == success).
 *
 * Tests are never standalone executables: they are linked into the target
 * (see tests/run_tests.c) and executed through the --test flag / the
 * GRRVM_BUILD_TESTS firmware.
 */

/* tests/common */
int test_load_vm(void);
int test_program_transfer(void);
int test_ref_guard(void);
int test_sha256(void);
int test_heap(void);
int test_h_set_type(void);

/* tests/posix */
int test_load_vm_file(void);

/* One shared VM heap for the suites that need one (ref_guard, heap).
 * Allocated on first use and released by the runner afterwards, so the
 * firmware allocates a single 200 KB block instead of one per suite -
 * repeated large alloc/free cycles fragment the small Pico C heap and
 * can make the runtime's own vm_create() fail. */
prim_val* test_heap_arena(void);
void test_heap_arena_release(void);

#endif /* GRRVM_TEST_SUITE_H */
