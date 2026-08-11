#ifndef VM_PROGRAM_LOADER_H
#define VM_PROGRAM_LOADER_H

#include "grrvm/types.h"
#include "grrvm/vm.h"
#include "grrvm/vm_log.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static inline void vm_program_loader(char* src, VM* vm_instance){
    FILE *file = fopen(src, "rb");
    if (!file) {
        vm_error("[ERROR] Could not open binary file: %s\n", src);
        exit(-1);
    }

    uint32_t header[3]; // [_global_start, program_size, data_size]
    if (fread(header, sizeof(uint32_t), 3, file) != 3) {
        vm_error("[ERROR] Corrupt file header in %s\n", src);
        fclose(file);
        exit(-1);
    }

    vm_instance = malloc(sizeof(VM));

    vm_instance->_program_start = header[0];
    vm_instance->program_size = header[1];
    uint32_t data_size = header[2];

    if (fread(vm_instance->program, sizeof(word), vm_instance->program_size, file) != vm_instance->program_size) {
        printf("[ERROR] Failed to read instruction stream.\n");
        fclose(file);
        exit(-1);
    }

    prim_val raw_primitive;
    for (uint32_t i = 0; i < data_size; i++) {
        if (fread(&raw_primitive, sizeof(prim_val), 1, file) != 1) {
            printf("[ERROR] Corrupt data payload at RAM index %d\n", i); // TODO:: Change this to vm_error next time
            fclose(file);
            exit(-1);
        }

        vm_instance->ram[i].metadata    = raw_primitive.metadata;
        vm_instance->ram[i].data        = raw_primitive.data;
    }

}

#endif
