#include "../../../include/parser/bytecode_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../../../include/grrvm/hal/load_vm.h"

VM* load_vm(const void* data, size_t size) {
    // Pass word_count to the parser instead of byte count

    long word_count = size / sizeof(uint32_t);
    uint32_t* buffer = (uint32_t*)data;

    VM *vm = (VM*)malloc(sizeof(VM));
    if (vm == NULL) {
        perror("Memory allocation failed for VM");
        return NULL;
    }

    if (bytecode_parser(buffer, word_count, vm) != 0) {
        free(vm); // Free vm on parser failure to avoid leaking memory
        return NULL;
    }

    return vm;
}
