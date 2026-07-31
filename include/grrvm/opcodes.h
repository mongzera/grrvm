#ifndef OPCODES_H
#define OPCODES_H

#include "grrvm/types.h"
#include <stdint.h>
typedef enum OPCODE_CATEGORY{
    OPC_STACK_OPERAND   = 0x00,
    OPC_ARITHMETIC      = 0x10,
    OPC_CONDITIONAL     = 0x20,
    OPC_BRANCHING       = 0x30,
    OPC_BITWISE         = 0x40,
    OPC_MEMORY_LOCAL    = 0x50,
    OPC_MEMORY_GLOBAL   = 0x60,
    OPC_MEMORY_HEAP     = 0x70

} OPCODE_CATEGORY;

typedef enum OPCODE{
    /** STACK OPERAND */
    HALT        = OPC_STACK_OPERAND | 0x00,
    PUSH        = OPC_STACK_OPERAND | 0x01,
    POP         = OPC_STACK_OPERAND | 0x02,
    DUP         = OPC_STACK_OPERAND | 0x03,
    ROT         = OPC_STACK_OPERAND | 0x04,
    SWAP        = OPC_STACK_OPERAND | 0x05,

    /** ARITHMETIC OPERAND */
    ADD         = OPC_ARITHMETIC | 0x00,
    SUB         = OPC_ARITHMETIC | 0x01,
    MUL         = OPC_ARITHMETIC | 0x02,
    DIV         = OPC_ARITHMETIC | 0x03,
    MOD         = OPC_ARITHMETIC | 0x04,
    INC         = OPC_ARITHMETIC | 0x05,
    DEC         = OPC_ARITHMETIC | 0x06,

    /** CONDITIONAL OPERAND */
    CMPEQ       = OPC_CONDITIONAL | 0x00,
    CMPNEQ      = OPC_CONDITIONAL | 0x01,
    CMPLT       = OPC_CONDITIONAL | 0x02,
    CMPLTE      = OPC_CONDITIONAL | 0x03,
    CMPGT       = OPC_CONDITIONAL | 0x04,
    CMPGTE      = OPC_CONDITIONAL | 0x05,

    /** BRANCHING OPERAND */
    JUMP        = OPC_BRANCHING | 0x00,
    JZ          = OPC_BRANCHING | 0x01,
    JNZ         = OPC_BRANCHING | 0x02,
    CALL        = OPC_BRANCHING | 0x03,
    RET         = OPC_BRANCHING | 0x04,

    /** BITWISE OPERAND */
    AND         = OPC_BITWISE | 0x00,
    OR          = OPC_BITWISE | 0x01,
    XOR         = OPC_BITWISE | 0x02,
    NOT         = OPC_BITWISE | 0x03,
    LSHIFT      = OPC_BITWISE | 0x04,
    RSHIFT      = OPC_BITWISE | 0x05,

    /** MEMORY(LOCAL) OPERAND */
    STORE_L     = OPC_MEMORY_LOCAL | 0x00,
    LOAD_L      = OPC_MEMORY_LOCAL | 0x01,

    /** MEMORY(GLOBAL) OPERAND */
    STORE       = OPC_MEMORY_GLOBAL | 0x00,
    LOAD        = OPC_MEMORY_GLOBAL | 0x01,
    LOCK        = OPC_MEMORY_GLOBAL | 0x02,
    UNLOCK      = OPC_MEMORY_GLOBAL | 0x03,

    /** MEMORY(HEAP) OPERAND */
    H_ALLOC     = OPC_MEMORY_HEAP | 0x00,
    H_FREE      = OPC_MEMORY_HEAP | 0x01,

} OPCODE;

static inline uint16_t get_opcode_cat(word opcode){
    return opcode & 0xF0;
}


#endif
