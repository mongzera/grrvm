#ifndef GRRVM_DEBUG_RUN_TESTS_H
#define GRRVM_DEBUG_RUN_TESTS_H

/* Runs every test compiled into this build (common + selected port).
 *
 * Returns the number of failed checks: 0 means all checks passed.
 * Only available when GRRVM_BUILD_TESTS is enabled at configure time. */
int run_all_tests(void);

#endif /* GRRVM_DEBUG_RUN_TESTS_H */
