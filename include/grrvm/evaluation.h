#ifndef GRRVM_EVALUATION_H
#define GRRVM_EVALUATION_H

#include "grrvm/vm_thread.h"

void eval_stack_operand         (VM_Thread* thread, word opcode);
void eval_arithmetic_operand    (VM_Thread* thread, word opcode);
void eval_conditional_operand   (VM_Thread* thread, word opcode);
void eval_branching_operand     (VM_Thread* thread, word opcode);
void eval_bitwise_operand       (VM_Thread* thread, word opcode);
void eval_memory_local_operand  (VM_Thread* thread, word opcode);
void eval_memory_global_operand (VM_Thread* thread, word opcode);
void eval_memory_heap_operand   (VM_Thread* thread, word opcode);
void eval_native_operand        (VM_Thread* thread, word opcode);

#endif /* GRRVM_EVALUATION_H */
