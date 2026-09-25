# GRRVM - Gamat-Ruyeras-Robinson Virtual Machine

GRRVM is a Stack-Based Virtual Machine targeting Microcontrollers and Posix Systems

# GRR ByteCode Specifications

## Data Types
GRR Bytecode Features a standard primitive datatype for variables.

* uint8 | int8
* uint16 | int16
* uint32 | int32
* float
* char

## Variables

Variables can be assigned in the ::_const_data and ::_data subroutine, which is a reserved subroutine for assigning globally available variables.

For immutable variables / constants, this is how we declare and initialize:
```
# Sample for Immutable Variables
::_const_data
  uint8 lvl 42
  char first_name "ethan", 0
  char last_name 'g','a','m','a','t'

```

For mutable variables, this is how we declare and initialize:
```
# Sample for Mutable Variables
::_data
  uint8 lvl 42
  char first_name "ethan", 0
  char last_name 'g','a','m','a','t'

```

## Subroutines
Subroutines are used to define reusable blocks of code that can be called from other parts of the program.

```
# Sample Subroutine
::_global ; this is the main subroutine that gets called first
  PUSH 5
  STORE_L 0 ; store the value 5 at local variable index 0
  PUSH 6
  STORE_L 1 ; store the value 6 at local variable index 1

  LOAD_L 0 ; load the value at local variable index 0
  LOAD_L 1 ; load the value at local variable index 1
  CALL add

  LOAD_L 0 ; load the value at local variable index 0
  LOAD_L 1 ; load the value at local variable index 1
  CALL sub
  
  HALT ; the main subroutine ends with HALT

::add ; this is a user-defined subroutine that adds two values by popping them from the stack and pushing the result back
  ADD
  RET  ; all subroutines end with RET

::sub ; this is a user-defined subroutine that subtracts two values by popping them from the stack and pushing the result back
  SUB
  RET  ; all subroutines end with RET
```

### 'CALL'
CALL is used to call a subroutine by its name.
It pushes its own Call Stack Frame so that you can declare local variables without interfering with previous call frames.

### 'RET'
RET is used to pop the top most Call Stack Frame and return from a subroutine.
It returns to the calling subroutine.

### 'HALT'
HALT is used to terminate the program.
It stops the program from running further.

## Labels
Labels are used to mark a specific location within a subroutine.
They are used with the JUMP, JZ, JNZ instruction to jump to a specific label.

```

::_global
    CALL label_sample
    HALT


::label_sample
    PUSH 5
    PUSH 6
    CMPNEQ   ; pops 5 and 6 to compare for inequality COMPARE_NOT_EQUAL | CMPNEQ which pushes 1 if not equal, 0 if equal
    JNZ .equal
    PUSH 0
    POP
    RET
    .equal:  ; this is a label called 'equal' where JNZ calls if 5 is not equal to 6.
        PUSH 1
        POP
    RET
    
```

# GRR Opcode and Instruction Set

Here is the breakdown of the **OPC (Opcode Classes/Categories)** and all defined **OPCODES** based on your Instruction Set Architecture (ISA) implementation.

---

## **OPC (Opcode Classes)**

The high-order nibble (or base byte offset) defines the instruction category:

| Class Constant | Hex Value | Category Description |
| --- | --- | --- |
| `OPC_STACK_OPERAND` | `0x00` | Basic stack manipulation instructions |
| `OPC_ARITHMETIC` | `0x10` | Mathematical calculations |
| `OPC_CONDITIONAL` | `0x20` | Comparison operations |
| `OPC_BRANCHING` | `0x30` | Control flow and subroutines |
| `OPC_BITWISE` | `0x40` | Bitwise logical operations |
| `OPC_MEMORY_LOCAL` | `0x50` | Stack frame/local variable memory operations |
| `OPC_MEMORY_GLOBAL` | `0x60` | Main memory/global address space operations |
| `OPC_MEMORY_HEAP` | `0x70` | Dynamic heap memory management |
| `OPC_SYS` | `0x80` | System-level operations *(Reserved)* |
| `OPC_UART` | `0x90` | Serial I/O communication operations *(Reserved)* |

---

## **OPCODES**

| OPC Class | Sub-Code | Assembly Mnemonic | Arg Count | Description / Pass Handler |
| --- | --- | --- | --- | --- |
| **`0x00` Stack** | `0x00` | `HALT` | `0` | Stops execution. |
|  | `0x01` | `PUSH` | `1` | Pushes a raw value onto the stack. |
|  | `0x02` | `POP` | `0` | Removes top item from the stack. |
|  | `0x03` | `DUP` | `0` | Duplicates top item on the stack. |
|  | `0x04` | `ROT` | `0` | Rotates stack elements. |
|  | `0x05` | `SWAP` | `0` | Swaps top two items on the stack. |
|  | `0x06` | `PUSH_ADDR` | `1` | Resolves variable name to absolute memory address and pushes it. |
|  | `0x07` | `PUSH_T` | `2` | Pushes a typed value onto the stack |
| **`0x10` Arithmetic** | `0x00` | `ADD` | `0` | Adds top two items on the stack. |
|  | `0x01` | `SUB` | `0` | Subtracts top two items on the stack. |
|  | `0x02` | `MUL` | `0` | Multiplies top two items on the stack. |
|  | `0x03` | `DIV` | `0` | Divides top two items on the stack. |
|  | `0x04` | `MOD` | `0` | Modulo operation on top two items. |
|  | `0x05` | `INC` | `1` | Increments top item on the stack. |
|  | `0x06` | `DEC` | `1` | Decrements top item on the stack. |
| **`0x20` Conditional** | `0x00` | `CMPEQ` | `0` | Compare: Equal (`==`) |
|  | `0x01` | `CMPNEQ` | `0` | Compare: Not Equal (`!=`) |
|  | `0x02` | `CMPLT` | `0` | Compare: Less Than (`<`) |
|  | `0x03` | `CMPLTE` | `0` | Compare: Less Than or Equal (`<=`) |
|  | `0x04` | `CMPGT` | `0` | Compare: Greater Than (`>`) |
|  | `0x05` | `CMPGTE` | `0` | Compare: Greater Than or Equal (`>=`) |
| **`0x30` Branching** | `0x00` | `JUMP` | `1` | Unconditional jump to target address. |
|  | `0x01` | `JZ` | `1` | Jump if Zero (conditional branch). |
|  | `0x02` | `JNZ` | `1` | Jump if Not Zero (conditional branch). |
|  | `0x03` | `CALL` | `1` | Calls subroutine and pushes return address; resolves label offset. |
|  | `0x04` | `RET` | `0` | Returns from subroutine. |
| **`0x40` Bitwise** | `0x00` | `AND` | `0` | Bitwise AND |
|  | `0x01` | `OR` | `0` | Bitwise OR |
|  | `0x02` | `XOR` | `0` | Bitwise XOR |
|  | `0x03` | `NOT` | `0` | Bitwise NOT |
|  | `0x04` | `LSHIFT` | `0` | Logical Shift Left |
|  | `0x05` | `RSHIFT` | `0` | Logical Shift Right |
| **`0x50` Local Mem** | `0x00` | `STORE_L` | `1` | Stores top stack value to local frame index. |
|  | `0x01` | `LOAD_L` | `1` | Loads local frame index onto the stack. |
| **`0x60` Global Mem** | `0x00` | `STORE` | `0` | Stores value into global address space. |
|  | `0x01` | `LOAD` | `0` | Loads value from global address space. |
|  | `0x02` | `LOCK` | `0` | Memory synchronization lock. |
|  | `0x03` | `UNLOCK` | `0` | Memory synchronization unlock. |
| **`0x70` Heap Mem** | `0x00` | `H_ALLOC` | `0` | Allocates dynamic memory block on heap. |
|  | `0x01` | `H_FREE` | `0` | Deallocates dynamic memory block on heap. |

# Assembler Documentation
This will provide you an idea how to assembler works under the hood

## Datatype and Mutability

How datatype ids

|datatype|id|
|-----|-----|
|TYPE_UINT8  |0x01|
|TYPE_UINT16 |0x02|
|TYPE_UINT32 |0x03|
|TYPE_UINT64 |0x04|
|TYPE_INT8   |0x05|
|TYPE_INT16  |0x06|
|TYPE_INT32  |0x07|
|TYPE_INT64  |0x08|
|TYPE_FLOAT  |0x09|
|TYPE_DOUBLE |0x0A|
|TYPE_CHAR   |0x0B|

Mutable tag
|mutable|immutable|
|---|---|
|0|1|

When combined with the datatype, it becomes a 32bit word
|isConstant|datatype|Binary|
|----|----|----|
|yes|uint8|0b1000000000000001|
|no|uint8|0b0000000000000001|
|yes|uint16|0b1000000000000010|
|no|uint16|0b1000000000000010|

## Binary Layout
The binary layout of the executable looks like this <br>
This is a binary stream of size 32bit
|offset|segment|
|---|---|
|0|program_offset|
|1|program_size|
|2|global_start|
|3|memory_decl|
|program_start|program|

# Ideas & Todos

## PUSH_T [DONE]
Implement PUSH_T <type> <value> - because using PUSH only is lacking, cannot specify the type of data, meaning, treatment will be wrong
Example: PUSH_T uint32 123423

[Program]
0|PUSH_T
1|uint32
2|123423

## PUSH_ADDR [DONE]
Implement PUSH_ADDR <mem_address> - so that we can access global variables and load it into the stack
Example: PUSH_ADDR 2

[Program]
0|PUSH_ADDR
1|2

[Stack]
[push data from address = 2, copied into the stack]
