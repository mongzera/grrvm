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
#ifndef VM_HEAP_SLOTS
#define VM_HEAP_SLOTS 1024
#endif

#ifndef VM_OP_STACK_MAX
#define VM_OP_STACK_MAX 127
#endif

#ifndef VM_CALL_STACK_MAX
#define VM_CALL_STACK_MAX 16
#endif

#ifndef VM_CALL_STACK_FRAME_MAX
#define VM_CALL_STACK_FRAME_MAX 128
#endif

#endif
