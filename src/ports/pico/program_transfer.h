#ifndef GRRVM_PORT_PICO_PROGRAM_TRANSFER_H
#define GRRVM_PORT_PICO_PROGRAM_TRANSFER_H

#include "grrvm/vm.h"

/* USB-serial (stdio) transport driver for the guest-program upload
 * protocol. Framing/state live in the shared grr_xfer_* engine; this
 * file only moves bytes between stdio and the engine and binds the
 * program_store storage backend.
 *
 * Transport/protocol separation: replacing stdio with another byte
 * transport (UART, BLE, ...) only requires changing this file. */

/* Initialize the transfer engine (call once, before any poll/service). */
void pico_transfer_init(void);

/* Drains whatever bytes are available into the engine WITHOUT touching
 * storage (feeding in IDLE / up to a complete HEADER never calls
 * storage ops), so it is safe to call while a guest program runs.
 *
 * Returns nonzero when a valid HEADER has been fully received and the
 * program must stop so the upload can proceed. Partial frames and line
 * noise do NOT stop a running program. */
int pico_transfer_poll(void);

/* Accepts a parked HEADER (erase + READY) and runs the transfer loop
 * until a new image is verified, validated and installed - or forever
 * while waiting for an upload. Call only when no VM is executing. */
void pico_transfer_service(void);

/* True once a valid upload HEADER has been received and is waiting to
 * be accepted (i.e. the stored program must not be started). */
int pico_transfer_header_pending(void);

/* True once pico_transfer_service() installed a new image; reading it
 * clears the flag. */
int pico_transfer_take_ready(void);

/* VM constructed during final validation of the newly installed image
 * (NULL if the platform loaded it itself instead). Ownership passes to
 * the caller; free with vm_terminate(). */
VM* pico_transfer_take_vm(void);

#endif /* GRRVM_PORT_PICO_PROGRAM_TRANSFER_H */
