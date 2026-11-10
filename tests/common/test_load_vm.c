/* Shared loader (load_vm_from_bytes) validation tests.
 *
 * Every case here runs identically on POSIX and Pico because the loader
 * and parser are shared: valid images (synthetic + real assembler
 * output), empty and truncated input, bad offsets/lengths, malformed
 * global records, misalignment, oversized programs, bad entry points and
 * overflow-shaped headers. Allocation-failure injection is not practical
 * in-process, so that case is covered by inspection instead (vm_create /
 * parser return paths are exercised indirectly by the error cases).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grrvm/hal/load_vm.h"
#include "grr_port_config.h"

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); g_pass++; } \
    else      { printf("  [FAIL] %s  (line %d)\n", msg, __LINE__); g_fail++; } \
} while (0)

#define CHECK_REJECT(image, words, msg) do { \
    VM* vm = load_vm_from_bytes((image), (words) * sizeof(uint32_t)); \
    CHECK(vm == NULL, msg); \
    if (vm != NULL) vm_terminate(vm); \
} while (0)

#define CHECK_ACCEPT(image, bytes, msg) do { \
    VM* vm = load_vm_from_bytes((image), (bytes)); \
    CHECK(vm != NULL, msg); \
    if (vm != NULL) vm_terminate(vm); \
} while (0)

/* --- Real assembler artifacts ------------------------------------- */

/* guest_programs/sample_fib_iter.grr.o (148 bytes, no global data) */
static const uint32_t fib_image[] = {
    0x00000003, 0x00000022, 0x00000000, 0x00000001,
    0x00000005, 0x00000033, 0x00000006, 0x00000002,
    0x00000000, 0x00000050, 0x00000000, 0x00000051,
    0x00000000, 0x00000001, 0x00000002, 0x00000022,
    0x00000032, 0x00000010, 0x00000051, 0x00000000,
    0x00000001, 0x00000001, 0x00000011, 0x00000033,
    0x00000006, 0x00000051, 0x00000000, 0x00000001,
    0x00000002, 0x00000011, 0x00000033, 0x00000006,
    0x00000010, 0x00000034, 0x00000051, 0x00000000,
    0x00000034
};

/* guest_programs/global_var_test.grr.o (228 bytes, two global rows) */
static const uint32_t gvt_image[] = {
    0x0000000c, 0x0000002d, 0x00000000, 0x00000003,
    0x00000004, 0x00000001, 0x00000002, 0x00000003,
    0x00000004, 0x00000005, 0x00000001, 0x00000003,
    0x00000061, 0x00000000, 0x00000002, 0x00000007,
    0x00000005, 0x00000005, 0x00000060, 0x00000001,
    0x00000061, 0x00000001, 0x00000002, 0x000000a0,
    0x18966ad5, 0x00000001, 0x00000001, 0x000000a0,
    0x6242059b, 0x00000001, 0x00000000, 0x00000050,
    0x00000000, 0x00000001, 0x00000001, 0x00000001,
    0x00001f40, 0x000000a0, 0xdee321a3, 0x00000070,
    0x00000002, 0x00000051, 0x00000000, 0x00000015,
    0x00000001, 0x00000050, 0x00000000, 0x00000051,
    0x00000000, 0x00000001, 0x00000004, 0x00000024,
    0x00000031, 0xffffffeb, 0x000000a0, 0x8178710d,
    0x00000000
};

/* --- Synthetic images --------------------------------------------- */

static void test_valid_synthetic(void) {
    printf("test_valid_synthetic\n");

    /* Minimal: header {program_offset=3, program_size=3, entry=0},
     * program = PUSH 42; HALT ... (3 words) */
    static const uint32_t minimal[] = {
        3, 3, 0,
        0x00000001, 42, 0x00000000
    };
    VM* vm = load_vm_from_bytes(minimal, sizeof(minimal));
    CHECK(vm != NULL, "minimal image loads");
    if (vm != NULL) {
        CHECK(vm->program_size == 3, "program_size parsed");
        CHECK(vm->_program_start == 0, "entry point parsed");
        CHECK(vm->program[0] == 0x00000001 && vm->program[1] == 42,
              "program words copied");
        vm_terminate(vm);
    }

    /* With one global row: [0x03, len=2, 7, 8] before the program */
    static const uint32_t with_globals[] = {
        7, 3, 0,
        0x00000003, 2, 7, 8,
        0x00000000, 0, 0
    };
    vm = load_vm_from_bytes(with_globals, sizeof(with_globals));
    CHECK(vm != NULL, "image with global data loads");
    if (vm != NULL) {
        CHECK(vm->global_var_table.row_count == 1, "global row count");
        vm_terminate(vm);
    }
}

static void test_valid_real_images(void) {
    printf("test_valid_real_images\n");

    VM* vm = load_vm_from_bytes(fib_image, sizeof(fib_image));
    CHECK(vm != NULL, "sample_fib_iter.grr.o loads");
    if (vm != NULL) {
        CHECK(vm->program_size == 0x22, "fib program_size == 34 words");
        CHECK(vm->_program_start == 0, "fib entry == _global at 0");
        vm_terminate(vm);
    }

    vm = load_vm_from_bytes(gvt_image, sizeof(gvt_image));
    CHECK(vm != NULL, "global_var_test.grr.o loads");
    if (vm != NULL) {
        CHECK(vm->program_size == 0x2d, "gvt program_size == 45 words");
        CHECK(vm->global_var_table.row_count == 2,
              "gvt has two global rows (nums[4], x)");
        vm_terminate(vm);
    }
}

static void test_reject_small_or_misaligned(void) {
    printf("test_reject_small_or_misaligned\n");

    static const uint32_t one_word = 0x12345678u;

    CHECK(load_vm_from_bytes(NULL, 64) == NULL, "NULL image rejected");
    CHECK(load_vm_from_bytes(&one_word, 0) == NULL, "empty image rejected");
    CHECK_REJECT(&one_word, 0, "size 0 rejected (explicit)");

    /* 8 bytes: less than the 12-byte header */
    static const uint32_t two_words[] = { 3, 3 };
    CHECK(load_vm_from_bytes(two_words, sizeof(two_words)) == NULL,
          "truncated header (8 bytes) rejected");

    /* 13 bytes: not a multiple of 4 */
    static const uint8_t unaligned[13] = { 3,0,0,0, 3,0,0,0, 0,0,0,0, 0xAA };
    CHECK(load_vm_from_bytes(unaligned, sizeof(unaligned)) == NULL,
          "non-word-aligned image size rejected");
}

static void test_reject_bad_header(void) {
    printf("test_reject_bad_header\n");

    static const uint32_t program[] = { 0, 0, 0 };

    /* program_offset way past the buffer (overflow-shaped value) */
    static const uint32_t huge_offset[] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0, 1, 2, 3 };
    CHECK_REJECT(huge_offset, 6, "0xFFFFFFFF offset/size rejected without wrap");

    /* program_offset below the 3-word header */
    static const uint32_t tiny_offset[] = { 2, 1, 0, 0 };
    CHECK_REJECT(tiny_offset, 4, "program_offset < 3 rejected");

    /* program_offset inside the buffer but program length runs past it */
    static const uint32_t short_buffer[] = { 3, 100, 0, 0, 0, 0 };
    CHECK_REJECT(short_buffer, 6, "program length exceeding image rejected");

    /* program_size == 0 (or entry beyond it) */
    static const uint32_t empty_program[] = { 3, 0, 0 };
    CHECK_REJECT(empty_program, 3, "zero-length program rejected");

    static const uint32_t entry_past_end[] = { 3, 3, 5, 0, 0, 0 };
    CHECK_REJECT(entry_past_end, 6, "entry point >= program_size rejected");

    static const uint32_t entry_eq_end[] = { 3, 3, 3, 0, 0, 0 };
    CHECK_REJECT(entry_eq_end, 6, "entry point == program_size rejected");
}

static void test_reject_malformed_globals(void) {
    printf("test_reject_malformed_globals\n");

    /* Record header truncated: region has only 1 word before program */
    static const uint32_t truncated_record[] = { 4, 1, 0, 0x00000003, 0 };
    CHECK_REJECT(truncated_record, 5, "truncated global record rejected");

    /* Unsupported record header (0x33 is an opcode, not a data type) */
    static const uint32_t bad_header[] = { 5, 1, 0, 0x00000033, 0x00000001, 0 };
    CHECK_REJECT(bad_header, 6, "unsupported global record header rejected");

    /* Row length larger than the words remaining before the program */
    static const uint32_t overrun_row[] = { 6, 1, 0, 0x00000003, 5, 0, 0 };
    CHECK_REJECT(overrun_row, 7, "row length overrunning region rejected");

    /* Row length that would corrupt the 16-bit heap-block metadata */
    static const uint32_t huge_row[] = { 5, 1, 0, 0x00000003, 0x00010000u, 0 };
    CHECK_REJECT(huge_row, 6, "row length > 0xFFFF rejected");

    /* Total global payload larger than the VM heap */
    static const uint32_t too_much_data[] = { 6, 1, 0, 0x00000003,
                                              VM_HEAP_SLOTS + 1u, 0, 0 };
    CHECK_REJECT(too_much_data, 7, "row length beyond VM heap rejected");
}

static void test_program_size_limits(void) {
    printf("test_program_size_limits\n");

    /* Exactly VM_PROGRAM_MAX_SIZE words: accepted. */
    size_t max_words = 3u + VM_PROGRAM_MAX_SIZE;
    uint32_t* exact = (uint32_t*)calloc(max_words, sizeof(uint32_t));
    if (exact == NULL) { printf("  [FAIL] calloc\n"); g_fail++; return; }
    exact[0] = 3;
    exact[1] = VM_PROGRAM_MAX_SIZE;
    exact[2] = 0;
    VM* vm = load_vm_from_bytes(exact, max_words * sizeof(uint32_t));
    CHECK(vm != NULL, "program of exactly VM_PROGRAM_MAX_SIZE loads");
    if (vm != NULL) vm_terminate(vm);
    free(exact);

    /* One word more: rejected. */
    size_t over_words = 3u + VM_PROGRAM_MAX_SIZE + 1u;
    uint32_t* over = (uint32_t*)calloc(over_words, sizeof(uint32_t));
    if (over == NULL) { printf("  [FAIL] calloc\n"); g_fail++; return; }
    over[0] = 3;
    over[1] = VM_PROGRAM_MAX_SIZE + 1;
    over[2] = 0;
    vm = load_vm_from_bytes(over, over_words * sizeof(uint32_t));
    CHECK(vm == NULL, "program exceeding VM_PROGRAM_MAX_SIZE rejected");
    if (vm != NULL) vm_terminate(vm);
    free(over);
}

static void test_ex_error_codes(void) {
    printf("test_ex_error_codes\n");

    int error = -99;
    VM* vm = (VM*)0x1;

    int rc = load_vm_from_bytes_ex(NULL, 16, &vm, &error);
    CHECK(rc != 0 && error == GRR_LOAD_ERR_IMAGE && vm == NULL,
          "NULL image reports GRR_LOAD_ERR_IMAGE");

    static const uint32_t good[] = { 3, 3, 0, 1, 42, 0 };
    error = -99;
    vm = NULL;
    rc = load_vm_from_bytes_ex(good, sizeof(good), &vm, &error);
    CHECK(rc == 0 && error == GRR_LOAD_OK && vm != NULL,
          "valid image reports GRR_LOAD_OK");
    if (vm != NULL) vm_terminate(vm);

    static const uint32_t bad[] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0, 1, 2, 3 };
    error = -99;
    vm = NULL;
    rc = load_vm_from_bytes_ex(bad, sizeof(bad), &vm, &error);
    CHECK(rc != 0 && error == GRR_LOAD_ERR_IMAGE && vm == NULL,
          "malformed image reports GRR_LOAD_ERR_IMAGE");
}

int main(void) {
    test_valid_synthetic();
    test_valid_real_images();
    test_reject_small_or_misaligned();
    test_reject_bad_header();
    test_reject_malformed_globals();
    test_program_size_limits();
    test_ex_error_codes();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
