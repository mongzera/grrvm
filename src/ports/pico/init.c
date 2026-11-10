#include "grrvm/hal/init.h"
#include "grrvm/hal/load_vm.h"
#include "grrvm/hal/vm_hal_math.h"
#include "grrvm/program_transfer.h"
#include "grrvm/vm.h"
#include "grrvm/vm_native.h"
#include "program_store.h"
#include "program_transfer.h"

#include "pico/stdlib.h"

/*
 * Pico startup flow - guest programs are loaded from persistent flash
 * storage and can be replaced at runtime over USB serial; no generated
 * guest-program header (main_grr.h) is involved.
 *
 *   1. platform services + storage init (validates the reserved region)
 *   2. if a valid persisted image exists and no upload is already
 *      pending, load it through the shared loader and execute it
 *   3. otherwise enter the recoverable upload/idle state
 *   4. a running program can be interrupted by a valid upload header
 *      (vm_host_poll -> pico_transfer_poll)
 *   5. after a verified upload the new program runs; an interrupted or
 *      failed upload leaves no valid image, so the device simply stays
 *      in (or returns to) the upload state
 */

static void run_guest(VM* vm) {
    vm_start(vm);
    vm_terminate(vm);
}

int init(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    stdio_init_all();
    hal_rng_init(0);

    // Give the host time to enumerate CDC and open the serial port.
    sleep_ms(2000);

    native_registry_init();
    pico_transfer_init();

    printf("--- GrrVM Starting ---\n");

    if (program_store_init() != 0) {
        printf("[GRRVM] FATAL: the reserved guest-program region overlaps "
               "the firmware image or flash bounds!\n");
        printf("[GRRVM] Rebuild with a smaller GRRVM_PICO_PROGRAM_REGION_SIZE "
               "or check the linker map.\n");
        for (;;) {
            sleep_ms(1000);
        }
    }

    if (program_store_has_valid_image()) {
        printf("[GRRVM] Stored guest program: %u bytes (verified).\n",
               (unsigned)program_store_image_size());
    } else {
        printf("[GRRVM] No valid guest program in storage.\n");
    }

    for (;;) {
        // Catch input that arrived before we started listening; a valid
        // upload header takes priority over running the stored program.
        pico_transfer_poll();

        if (program_store_has_valid_image()
            && !pico_transfer_header_pending()) {
            printf("[GRRVM] Loading stored guest program...\n");
            VM* vm = load_vm_from_bytes(program_store_image_ptr(),
                                        program_store_image_size());
            if (vm != NULL) {
                printf("[GRRVM] Executing guest program.\n");
                run_guest(vm);
                printf("[GRRVM] Guest program finished.\n");
            } else {
                printf("[GRRVM] Stored image failed validation; "
                       "invalidating it.\n");
                program_store_invalidate();
            }
        }

        // Recoverable no-program / upload state.
        printf("[GRRVM] Ready to receive a guest program "
               "(protocol v%u, USB serial).\n",
               (unsigned)GRR_XFER_PROTOCOL_VERSION);
        pico_transfer_service();

        if (pico_transfer_take_ready()) {
            printf("[GRRVM] Guest program verified and installed.\n");
        }

        VM* new_vm = pico_transfer_take_vm();
        if (new_vm != NULL) {
            printf("[GRRVM] Executing new guest program.\n");
            run_guest(new_vm);
            printf("[GRRVM] Guest program finished.\n");
        }
    }
}
