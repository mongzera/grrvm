#ifndef GRR_PORT_CONFIG_H
#define GRR_PORT_CONFIG_H

// virtual threads
#define VM_MAX_THREADS 24

// guest program
#define VM_PROGRAM_MAX_SIZE (256 * 1024)

// memory
#define VM_MAX_RAM (5000 * 1024)
#define VM_RAM_GLOBAL_REGION_SIZE (512 * 1024)
#define VM_RAM_SLAB_REGION_SIZE (3400 * 1024)
#define VM_RAM_BUDDY_REGION_SIZE (1088 * 1024)

// stack frames
#define VM_OP_STACK_MAX (127 * 1024)
#define VM_CALL_STACK_MAX (16 * 1024)
#define VM_CALL_STACK_FRAME_MAX (512 * 1024)

#endif
