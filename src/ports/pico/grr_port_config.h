#ifndef GRR_PORT_CONFIG_H
#define GRR_PORT_CONFIG_H

// virtual threads
#define VM_MAX_THREADS 2

// guest program
#define VM_PROGRAM_MAX_SIZE 1024 // this is probably not being followed in the vm_load.c

// persistent guest-program storage: flash region reserved at the TOP of
// flash (metadata sector + image). See src/ports/pico/program_store.h.
// program_store_init() refuses to run if the firmware overlaps it.
#define GRRVM_PICO_PROGRAM_REGION_SIZE (128u * 1024u)

// memory
#define VM_HEAP_SLOTS (200 * 1024 / 8) // 200KB * 1024 bytes / 8 bytes (sizeof prim_val) = 25600 SLOTS

// stack frames
#define VM_OP_STACK_MAX 64
#define VM_CALL_STACK_MAX 16
#define VM_CALL_STACK_FRAME_MAX 16

#endif
