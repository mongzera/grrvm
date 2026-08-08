#ifndef EVALUATION_H
#define EVALUATION_H

#include "grrvm/vm.h"

void eval_stack_operand         (VM_Thread* thread, word opcode);
void eval_arithmetic_operand    (VM_Thread* thread, word opcode);
void eval_conditional_operand   (VM_Thread* thread, word opcode);
void eval_branching_operand     (VM_Thread* thread, word opcode);
void eval_bitwise_operand       (VM_Thread* thread, word opcode);
void eval_memory_local_operand  (VM_Thread* thread, word opcode);
void eval_memory_global_operand (VM_Thread* thread, word opcode);
void eval_memory_heap_operand   (VM_Thread* thread, word opcode);

#endif
