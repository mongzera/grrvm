#include "grrvm/hal/load_vm.h"
#include "grrvm/vm_log.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * POSIX wrapper: reads a .grr.o file into memory and hands the bytes to
 * the shared loader (load_vm_from_bytes). All parsing/validation happens
 * there, so POSIX and Pico accept exactly the same images.
 */
VM* load_vm_from_file(const char* path) {
    if (path == NULL) {
        vm_error("LOAD VM", "File path is NULL!");
        return NULL;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        perror("Error opening file");
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        perror("Error seeking file");
        fclose(file);
        return NULL;
    }

    long file_size = ftell(file);
    rewind(file);

    if (file_size <= 0) {
        fprintf(stderr, "Error: file is empty or unreadable: %s\n", path);
        fclose(file);
        return NULL;
    }

    // Validate that the file size is aligned to 4-byte boundaries
    if (file_size % sizeof(uint32_t) != 0) {
        fprintf(stderr, "Error: File size (%ld bytes) is not aligned to 32-bit words\n", file_size);
        fclose(file);
        return NULL;
    }

    uint8_t *buffer = (uint8_t*)malloc((size_t)file_size);
    if (buffer == NULL) {
        perror("Memory allocation failed");
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, (size_t)file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size) {
        perror("Error reading binary image");
        free(buffer);
        return NULL;
    }

    VM *vm = load_vm_from_bytes(buffer, (size_t)file_size);

    // The parser copies everything it needs, so the file buffer can go now.
    free(buffer);

    return vm;
}
