#ifndef GRRVM_VM_HOST_POLL_H
#define GRRVM_VM_HOST_POLL_H

/* Periodically called by vm_loop() while a guest program runs.
 *
 * Returns nonzero to request that execution stops (all threads are
 * deactivated and the loop exits). This is how a port interrupts a long
 * -running guest program, e.g. to accept a replacement-program upload.
 *
 * The shared core provides a weak default that always returns 0, so a
 * port that does nothing keeps the previous behavior. Ports override it
 * with a strong definition. Implementations must be cheap and must not
 * block: vm_loop polls roughly once every 256 instructions. */
int vm_host_poll(void);

#endif /* GRRVM_VM_HOST_POLL_H */
