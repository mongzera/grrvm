#include "../../../include/parser/bytecode_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../../../include/grrvm/hal/load_vm.h"

VM* load_vm(const void* data, size_t size) {

    const char* dir = (const char*)data;

    FILE *file = fopen(dir, "rb");
    if (file == NULL) {
        perror("Error opening file");
        return NULL;
    }


    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Validate that the file size is aligned to 4-byte boundaries
    if (file_size % sizeof(uint32_t) != 0) {
        fprintf(stderr, "Error: File size (%ld bytes) is not aligned to 32-bit words\n", file_size);
        fclose(file);
        return NULL;
    }

    long word_count = file_size / sizeof(uint32_t);
    uint32_t *buffer = (uint32_t*)malloc(file_size); // file_size bytes = word_count * sizeof(uint32_t)

    if (buffer == NULL) {
        perror("Memory allocation failed");
        fclose(file);
        return NULL;
    }

    // Read word_count elements of size uint32_t
    size_t words_read = fread(buffer, sizeof(uint32_t), word_count, file);
    fclose(file);

    if (words_read < word_count) {
        perror("Error reading full binary word sequence");
        free(buffer);
        return NULL;
    }

    // Pass word_count to the parser instead of byte count
    VM *vm = (VM*)malloc(sizeof(VM));
    if (vm == NULL) {
        perror("Memory allocation failed for VM");
        free(buffer);
        return NULL;
    }

    if (bytecode_parser(buffer, word_count, vm) != 0) {
            free(buffer);
            free(vm); // Free vm on parser failure to avoid leaking memory
            return NULL;
        }

    // Clean up temporary buffer if the parser copies instructions elsewhere inside `vm`
    free(buffer);

    return vm;
}
