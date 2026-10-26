/* POSIX file-loader tests (load_vm_from_file).
 *
 * Covers the file-reading wrapper: valid artifacts, missing files,
 * empty/misaligned/truncated files - and loads EVERY checked-in
 * guest_programs/*.grr.o through the shared loader, proving the
 * hardened parser still accepts the real assembler output.
 *
 * GRRVM_TEST_DATA_DIR is provided by CMake and points at the repository's
 * guest_programs/ directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grrvm/hal/load_vm.h"

#ifndef GRRVM_TEST_DATA_DIR
#define GRRVM_TEST_DATA_DIR "../../guest_programs"
#endif

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); g_pass++; } \
    else      { printf("  [FAIL] %s  (line %d)\n", msg, __LINE__); g_fail++; } \
} while (0)

static const char* k_artifacts[] = {
    "7segment.grr.o",
    "allocation_speed.grr.o",
    "blink_led.grr.o",
    "gc_heap_test.grr.o",
    "gc_jitter_test.grr.o",
    "gcc_ext_frag_pre_post_gc.grr.o",
    "global_var_test.grr.o",
    "sample_fib_iter.grr.o",
    "sample_mem_alloc.grr.o",
    "tensor_horner.grr.o",
};

static char* path_of(const char* name) {
    static char buf[512];
    snprintf(buf, sizeof(buf), "%s/%s", GRRVM_TEST_DATA_DIR, name);
    return buf;
}

static void write_bytes(const char* path, const void* data, size_t len) {
    FILE* f = fopen(path, "wb");
    if (f == NULL) return;
    fwrite(data, 1, len, f);
    fclose(f);
}

static void test_valid_artifacts(void) {
    printf("test_valid_artifacts\n");

    size_t count = sizeof(k_artifacts) / sizeof(k_artifacts[0]);
    for (size_t i = 0; i < count; i++) {
        VM* vm = load_vm_from_file(path_of(k_artifacts[i]));
        char msg[256];
        snprintf(msg, sizeof(msg), "%s loads", k_artifacts[i]);
        CHECK(vm != NULL, msg);
        if (vm != NULL) vm_terminate(vm);
    }

    VM* vm = load_vm_from_file(path_of("sample_fib_iter.grr.o"));
    CHECK(vm != NULL, "sample_fib_iter loads");
    if (vm != NULL) {
        CHECK(vm->program_size == 0x22, "sample_fib_iter program_size == 34");
        vm_terminate(vm);
    }
}

static void test_missing_and_bad_files(void) {
    printf("test_missing_and_bad_files\n");

    VM* vm = load_vm_from_file(path_of("does_not_exist.grr.o"));
    CHECK(vm == NULL, "missing file rejected");
    CHECK(load_vm_from_file(NULL) == NULL, "NULL path rejected");

    /* Empty file */
    write_bytes("test_empty.bin", "", 0);
    vm = load_vm_from_file("test_empty.bin");
    CHECK(vm == NULL, "empty file rejected");
    remove("test_empty.bin");

    /* Truncated header: 4 bytes = one word */
    uint32_t one = 3;
    write_bytes("test_trunc.bin", &one, sizeof(one));
    vm = load_vm_from_file("test_trunc.bin");
    CHECK(vm == NULL, "single-word file rejected");
    remove("test_trunc.bin");

    /* Non-word-aligned size: 13 bytes */
    uint8_t unaligned[13] = { 3,0,0,0, 3,0,0,0, 0,0,0,0, 0xAA };
    write_bytes("test_unaligned.bin", unaligned, sizeof(unaligned));
    vm = load_vm_from_file("test_unaligned.bin");
    CHECK(vm == NULL, "13-byte file rejected");
    remove("test_unaligned.bin");

    /* Structurally invalid image (bad program offset) with aligned size */
    uint32_t bad[] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0, 1, 2, 3 };
    write_bytes("test_bad.bin", bad, sizeof(bad));
    vm = load_vm_from_file("test_bad.bin");
    CHECK(vm == NULL, "aligned but malformed file rejected");
    remove("test_bad.bin");
}

int main(void) {
    test_valid_artifacts();
    test_missing_and_bad_files();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
