#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <sys/types.h>
#include "grr_port_config.h"

#ifndef VM_MAX_THREADS
#define VM_MAX_THREADS 2
#endif

#ifndef VM_PROGRAM_MAX_SIZE
#define VM_PROGRAM_MAX_SIZE 256
#endif


// memory
#ifndef VM_MAX_RAM
#define VM_MAX_RAM 1024
#endif

#ifndef VM_RAM_GLOBAL_REGION_SIZE
#define VM_RAM_GLOBAL_REGION_SIZE 64
#endif

#ifndef VM_RAM_SLAB_REGION_SIZE
#define VM_RAM_SLAB_REGION_SIZE 600
#endif

#ifndef VM_RAM_BUDDY_REGION_SIZE
#define VM_RAM_BUDDY_REGION_SIZE 360
#endif


#ifndef VM_OP_STACK_MAX
#define VM_OP_STACK_MAX 127
#endif

#ifndef VM_CALL_STACK_MAX
#define VM_CALL_STACK_MAX 16
#endif

#ifndef VM_CALL_STACK_FRAME_MAX
#define VM_CALL_STACK_FRAME_MAX 512
#endif

#if (VM_RAM_GLOBAL_REGION_SIZE + VM_RAM_SLAB_REGION_SIZE + VM_RAM_BUDDY_REGION_SIZE) != VM_MAX_RAM
#error "Global + Slab + Buddy memory allocation does not match total VM_MAX_RAM!"
#endif

#endif
