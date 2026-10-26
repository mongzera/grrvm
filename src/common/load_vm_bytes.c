#include "grrvm/hal/load_vm.h"
#include "grrvm/vm_log.h"
#include "parser/bytecode_parse.h"
#include <stdint.h>
#include <stdlib.h>

/* Releases a VM together with every heap block vm_create() allocated.
 * Used on load failure, where vm_terminate() is not reachable yet. */
static void destroy_vm(VM* vm) {
    if (vm == NULL) return;
    free(vm->program);
    free(vm->ram);
    free(vm->vm_threads);
    free(vm->global_var_table.rows);
    free(vm);
}

/*
 * Shared loader: takes the complete .grr.o image as bytes, validates it
 * and constructs the VM state. Storage-agnostic on purpose - POSIX passes
 * a buffer read from a file, the Pico passes flash-mapped bytes.
 *
 * Ownership: the parser copies everything it needs into VM-owned buffers,
 * so `image` is borrowed only for the duration of this call.
 */
VM* load_vm_from_bytes(const void* image, size_t image_size) {
    int error = GRR_LOAD_OK;
    VM* vm = NULL;
    (void)load_vm_from_bytes_ex(image, image_size, &vm, &error);
    return vm;
}

int load_vm_from_bytes_ex(const void* image, size_t image_size,
                          VM** out, int* error) {
    *out = NULL;
    *error = GRR_LOAD_OK;

    if (image == NULL) {
        vm_error("LOAD VM", "Image buffer is NULL!");
        *error = GRR_LOAD_ERR_IMAGE;
        return -1;
    }

    if (image_size < 3 * sizeof(uint32_t)) {
        vm_error("LOAD VM", "Image too small: %u bytes (minimum header is 12)!",
                 (unsigned) image_size);
        *error = GRR_LOAD_ERR_IMAGE;
        return -1;
    }

    if (image_size % sizeof(uint32_t) != 0) {
        vm_error("LOAD VM", "Image size (%u bytes) is not aligned to 32-bit words!",
                 (unsigned) image_size);
        *error = GRR_LOAD_ERR_IMAGE;
        return -1;
    }

    if (((uintptr_t) image % sizeof(uint32_t)) != 0) {
        vm_error("LOAD VM", "Image buffer is not 4-byte aligned!");
        *error = GRR_LOAD_ERR_IMAGE;
        return -1;
    }

    const uint32_t* buffer = (const uint32_t*) image;
    size_t word_count = image_size / sizeof(uint32_t);

    VM* vm_create_result = vm_create();
    if (vm_create_result == NULL) {
        vm_error("LOAD VM", "Memory allocation failed for VM!");
        *error = GRR_LOAD_ERR_MEMORY;
        return -1;
    }

    int parse_result = bytecode_parser(buffer, word_count, vm_create_result);
    if (parse_result != 0) {
        destroy_vm(vm_create_result);
        *error = (parse_result == -2) ? GRR_LOAD_ERR_MEMORY : GRR_LOAD_ERR_IMAGE;
        return -1;
    }

    *out = vm_create_result;
    return 0;
}
