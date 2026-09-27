#ifndef GRR_PORT_CONFIG_H
#define GRR_PORT_CONFIG_H

// virtual threads
#define VM_MAX_THREADS 2

// guest program
#define VM_PROGRAM_MAX_SIZE 256

// memory
#define VM_HEAP_SLOTS (200 * 1024 / 8) // 200KB * 1024 bytes / 8 bytes (sizeof prim_val) = 25600 SLOTS

// stack frames
#define VM_OP_STACK_MAX 64
#define VM_CALL_STACK_MAX 16
#define VM_CALL_STACK_FRAME_MAX 16

#endif
