#ifndef GRR_PORT_CONFIG_H
#define GRR_PORT_CONFIG_H

// virtual threads
#define VM_MAX_THREADS 2

// guest program
#define VM_PROGRAM_MAX_SIZE 256

// memory
#define VM_MAX_RAM 1024
#define VM_RAM_GLOBAL_REGION_SIZE 64
#define VM_RAM_SLAB_REGION_SIZE 600
#define VM_RAM_BUDDY_REGION_SIZE 360

// stack frames
#define VM_OP_STACK_MAX 127
#define VM_CALL_STACK_MAX 16
#define VM_CALL_STACK_FRAME_MAX 512

#endif
