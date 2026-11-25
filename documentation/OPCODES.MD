
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
|  | `0x07` | `PUSH_T <type> <value>` | `2` | Pushes a typed value onto the stack (`uint8`, `uint16`, `uint32`, `int8`, `int16`, `int32`, `float`). |
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
| **`0x60` Global Mem** | `0x00` | `STORE <var_name>` | `1` | Pops the top stack value and writes it to the named global variable's memory row. |
|  | `0x01` | `LOAD <var_name>` | `1` | Reads the named global variable and pushes its value onto the stack. |
|  | `0x02` | `LOCK <reference>` | `1` | Memory synchronization lock. |
|  | `0x03` | `UNLOCK <reference>` | `1` | Memory synchronization unlock. |
| **`0x70` Heap Mem** | `0x00` | `H_ALLOC` | `0` | Allocates dynamic memory block on heap. |
|  | `0x01` | `H_FREE` | `0` | Deallocates dynamic memory block on heap. |
|  | `0x02` | `H_STORE` | `0` | Pops a heap reference, then a value, and writes the value at that heap address. |
|  | `0x03` | `H_LOAD` | `0` | Pops a heap reference and pushes the value stored at that heap address. |
|  | `0x04` | `H_STORE_OFF` | `0` | Pops a heap reference, an offset, then a value; writes the value at `reference + offset`. |
|  | `0x05` | `H_LOAD_OFF` | `0` | Pops a heap reference and an offset; pushes the value at `reference + offset`. |
|  | `0x06` | `H_SET_TYPE <type>` | `1` | Pops a heap reference and retags that heap block with the given type (same type set as `PUSH_T`). |
| **`0x80` System Calls** | `0x00` | | | Not implemented yet! |
| **`0x90` UART Interface** | `0x00` | | | Not implemented yet! |
| **`0xA0` Native Functions** | `0x00` | `INVOKE_NATIVE <native_function_name>` | `1` | Invokes a native function. |

---

## **Heap Memory Stack Order**

The heap opcodes take their operands from the operand stack, not from instruction arguments. `H_STORE` requires the value below the reference (`value`, `offset` where applicable, then `reference` pushed last), because the reference is popped first.

| Opcode | Popped (in order) | Pushed |
| --- | --- | --- |
| `H_STORE` | `reference`, `value` | — |
| `H_LOAD` | `reference` | `value` |
| `H_STORE_OFF` | `reference`, `offset`, `value` | — |
| `H_LOAD_OFF` | `reference`, `offset` | `value` |

## **Implementation Notes**

- `LOCK` and `UNLOCK` are declared in `include/grrvm/opcodes.h` and encoded by the assembler, but `src/core/evaluation/memory_global.c` has no handler for them (and no `default` case), so they currently execute as no-ops. Their arg count is also inconsistent: the assembler declares `0`, this document lists `1`.
- `PUSH_T` and `H_SET_TYPE` accept only the types the VM can represent: `uint8`, `uint16`, `uint32`, `int8`, `int16`, `int32`, `float`. The assembler rejects `uint64`, `int64`, `double` and `char` for these immediates because the VM has no matching `prim_type`; those names are still accepted in data declarations, where the loader rejects unsupported headers.
- `OPC_SYS` (`0x80`) and `OPC_UART` (`0x90`) are reserved and have no evaluator.
