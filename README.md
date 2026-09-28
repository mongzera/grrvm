# GRRVM - Gamat-Ruyeras-Robinson Virtual Machine

GRRVM is a Stack-Based Virtual Machine targeting Microcontrollers and Posix Systems

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
| `OPC_NATIVE` | `0xA0` | Native function calls *(Reserved)* |

---

## **OPCODES**

| OPC Class | Sub-Code | Assembly Mnemonic | Arg Count | Description / Pass Handler |
| --- | --- | --- | --- | --- |
| **`0x00` Stack** | `0x00` | `HALT` | `0` | Stops execution. |
|  | `0x01` | `PUSH <value>` | `1` | Pushes a raw value onto the stack. |
|  | `0x02` | `POP` | `0` | Removes top item from the stack. |
|  | `0x03` | `DUP` | `0` | Duplicates top item on the stack. |
|  | `0x04` | `ROT` | `0` | Rotates stack elements. |
|  | `0x05` | `SWAP` | `0` | Swaps top two items on the stack. |
|  | `0x06` | `PUSH_ADDR <var_name>` | `1` | Resolves variable name to global memory address and pushes it. |
|  | `0x07` | `PUSH_T <type> <value>` | `2` | Pushes a typed value onto the stack |
| **`0x10` Arithmetic** | `0x00` | `ADD` | `0` | Adds top two items on the stack. |
|  | `0x01` | `SUB` | `0` | Subtracts top two items on the stack. |
|  | `0x02` | `MUL` | `0` | Multiplies top two items on the stack. |
|  | `0x03` | `DIV` | `0` | Divides top two items on the stack. |
|  | `0x04` | `MOD` | `0` | Modulo operation on top two items. |
|  | `0x05` | `INC <value>` | `1` | Increments top item on the stack. |
|  | `0x06` | `DEC <value>` | `1` | Decrements top item on the stack. |
| **`0x20` Conditional** | `0x00` | `CMPEQ` | `0` | Compare: Equal (`==`) |
|  | `0x01` | `CMPNEQ` | `0` | Compare: Not Equal (`!=`) |
|  | `0x02` | `CMPLT` | `0` | Compare: Less Than (`<`) |
|  | `0x03` | `CMPLTE` | `0` | Compare: Less Than or Equal (`<=`) |
|  | `0x04` | `CMPGT` | `0` | Compare: Greater Than (`>`) |
|  | `0x05` | `CMPGTE` | `0` | Compare: Greater Than or Equal (`>=`) |
| **`0x30` Branching** | `0x00` | `JUMP <label>` | `1` | Unconditional jump to target address. |
|  | `0x01` | `JZ <label>` | `1` | Jump if Zero (conditional branch). |
|  | `0x02` | `JNZ <label>` | `1` | Jump if Not Zero (conditional branch). |
|  | `0x03` | `CALL <subroutine>` | `1` | Calls subroutine and pushes return address; resolves label offset. |
|  | `0x04` | `RET` | `0` | Returns from subroutine. |
| **`0x40` Bitwise** | `0x00` | `AND` | `0` | Bitwise AND |
|  | `0x01` | `OR` | `0` | Bitwise OR |
|  | `0x02` | `XOR` | `0` | Bitwise XOR |
|  | `0x03` | `NOT` | `0` | Bitwise NOT |
|  | `0x04` | `LSHIFT` | `0` | Logical Shift Left |
|  | `0x05` | `RSHIFT` | `0` | Logical Shift Right |
| **`0x50` Local Mem** | `0x00` | `STORE_L <index>` | `1` | Stores top stack value to local frame index. |
|  | `0x01` | `LOAD_L <index>` | `1` | Loads local frame index onto the stack. |
| **`0x60` Global Mem** | `0x02` | `LOCK <reference>` | `1` | Memory synchronization lock. |
|  | `0x03` | `UNLOCK <reference>` | `1` | Memory synchronization unlock. |
| **`0x70` Heap Mem** | `0x00` | `H_ALLOC` | `0` | Allocates dynamic memory block on heap. |
|  | `0x01` | `H_FREE` | `0` | Deallocates dynamic memory block on heap. |
| **`0x80` System Calls** | `0x00` | | | Not implemented yet! |
| **`0x90` UART Interface** | `0x00` | | | Not implemented yet! |
| **`0xA0` Native Functions** | `0x00` | `INVOKE_NATIVE <native_function_name>` | `1` | Invokes a native function. |
# Data Types

* g_float - standard IEEE 754 float
* g_int32   - signed int (16-bit / 32-bit)
* g_uint32  - unsigned int (16-bit / 32-bit)
* g_  - signed byte (8-bit)
* G_CHAR  - unsigned int (8-bit)

# Bit Length
* word  - 16-bit / 32-bit
* half  - 16-bit / 8-bit
* byte  - 8-bit

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

## POP_ADDR 
Implement POP_ADDR <mem_address> - so that we can store the popped value back into a memory address
Example: POP_ADDR 2

[Program]
0|POP_ADDR
1|2

Also, Since RAM is now considered the HEAP, maybe we can separate the GLOBAL and HEAP memory

## H_SET_TYPE [DONE]
Implement H_SET_TYPE <type> - so that we can set the type of a memory address
Example: H_SET_TYPE uint32

Functionality:
Pops a value from the stack (TYPE_REFERENCE)
Then, it will target the TYPE_LENGTH it points to where it will set the type for that array.
