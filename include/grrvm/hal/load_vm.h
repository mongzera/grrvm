#ifndef GRRVM_HAL_LOAD_VM_H
#define GRRVM_HAL_LOAD_VM_H
#include <stddef.h>
#include <stdint.h>
#include "grrvm/vm.h"

/*
 * Shared byte-buffer loader (implemented in src/common/load_vm_bytes.c).
 *
 * Validates a complete .grr.o image and builds the VM from it. The image
 * buffer is only READ during the call: the parser copies the program words
 * and the global-data rows into VM-owned memory, so the caller may free or
 * reuse `image` as soon as this returns. The buffer must be 4-byte aligned
 * and its length must be an exact multiple of 4.
 *
 * Returns NULL when the image is invalid or memory is insufficient.
 * On success the caller owns the VM and releases it with vm_terminate().
 */
VM* load_vm_from_bytes(const void* image, size_t image_size);

/* Result codes reported through `error` by load_vm_from_bytes_ex(). */
#define GRR_LOAD_OK          0
#define GRR_LOAD_ERR_IMAGE   1  /* malformed / unsupported image */
#define GRR_LOAD_ERR_MEMORY  2  /* not enough memory for the VM */

/* Same as load_vm_from_bytes(), but distinguishes an invalid image from
 * an allocation failure so platforms can report a precise error (e.g.
 * the Pico upload path answers ERR_VM_MEMORY vs ERR_STRUCTURE).
 * `error` and `out` must be non-NULL. */
int load_vm_from_bytes_ex(const void* image, size_t image_size,
                          VM** out, int* error);

/*
 * POSIX port only (src/ports/posix/load_vm.c): reads the file at `path`
 * into memory and delegates to load_vm_from_bytes(). Not implemented on
 * embedded ports, which have no filesystem.
 */
VM* load_vm_from_file(const char* path);

#endif /* GRRVM_HAL_LOAD_VM_H */
