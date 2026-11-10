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
:=_const_data
  uint8 lvl 42
  char first_name "ethan", 0
  char last_name 'g','a','m','a','t'

```

For mutable variables, this is how we declare and initialize:
```
# Sample for Mutable Variables
:=_data
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

## H_SET_TYPE 
Implement H_SET_TYPE <type> - so that we can set the type of a memory address
Example: H_SET_TYPE uint32

Functionality:
Pops a value from the stack (TYPE_REFERENCE)
Then, it will target the TYPE_LENGTH it points to where it will set the type for that array.
# GRRVM Runtime Guest-Program Upload (Pico)

Persistent runtime loading of `.grr.o` guest programs on the Raspberry Pi
Pico: programs are received over USB serial, stored in a reserved flash
region, SHA-256 verified, structurally validated, executed, and retained
across resets and power cycles — **without rebuilding or reflashing the
firmware**.

---

## 1. Revised program-loading architecture

```
                 shared VM core (no Pico SDK, no flash, no serial)
   +------------------------------------------------------------------+
   | parser/bytecode_parse.h     hardened .grr.o validation           |
   | src/common/load_vm_bytes.c  load_vm_from_bytes(image, size)      |
   | src/common/sha256.c         SHA-256 (whole-image verification)   |
   | src/common/program_transfer.c  grr-xfer v1 protocol state machine|
   +------------------------------------------------------------------+
             ^                                  ^
   POSIX port|                    Pico port     |
   src/ports/posix/load_vm.c      src/ports/pico/program_store.c   (flash)
   (reads a file into memory,     src/ports/pico/program_transfer.c (USB stdio)
    calls the shared loader)      src/ports/pico/init.c            (startup)
```

* **One loader, one parser.** `load_vm_from_bytes()` validates the image
  and builds the VM; POSIX wraps it with `load_vm_from_file()` (read file
  → pass bytes), the Pico passes flash-mapped bytes directly. Parsing
  logic is never duplicated.
* **Buffer ownership:** the loader only *reads* the buffer during the
  call — the parser copies program words and global data into VM-owned
  memory — so callers may free the buffer immediately afterwards
  (`load_vm_from_file` frees its read buffer; the Pico borrows XIP-mapped
  flash, which needs no copy at all). The buffer must be 4-byte aligned
  and its length a multiple of 4.
* `src/ports/pico/load_vm.c` was removed; `init.c` no longer includes
  `main_grr.h`. The generated header still exists in `include/` only as a
  legacy artifact and is not part of the Pico build.
* The POSIX `-r/--run <file>.grr.o` workflow is unchanged.

### Pico startup flow

1. `stdio_init_all()`, native registry, transfer engine init.
2. `program_store_init()` — validates the reserved flash region against
   the firmware image (`__flash_binary_end`) and loads the stored-image
   metadata.
3. If a **valid** persisted image exists and no upload header is already
   pending → `load_vm_from_bytes()` → execute it.
4. Otherwise → recoverable **no-program / upload state**: the device
   prints `Ready to receive a guest program (protocol v1, USB serial)`
   and services uploads.
5. While a guest program runs, `vm_loop()` polls `vm_host_poll()` about
   every 256 instructions; the Pico implementation feeds incoming serial
   bytes into the protocol engine **without touching flash** and stops
   the program only when a complete, valid HEADER arrives. Line noise or
   partial frames do not stop a running program.
6. After a verified upload, the new program is executed immediately.

---

## 2. Flash layout

Derived from the board header (`PICO_FLASH_SIZE_BYTES`, 2 MB on a stock
Pico) — no capacity is hardcoded:

```
0x000000  +----------------------------+
          | firmware (boot2 + code)    |  ~105 KB today (grrvm.bin)
          | ... free space ...         |
0x1E0000  +----------------------------+ <- region base
          | metadata sector   (4 KiB)  |  magic, version, image_length,
          |                            |  SHA-256, crc32
0x1E1000  +----------------------------+
          | image sectors     (124 KiB)|  4 KiB erase units, 256 B program
          |                            |  pages, erased lazily during
          |                            |  upload
0x200000  +----------------------------+ <- end of flash
```

* Region size: `GRRVM_PICO_PROGRAM_REGION_SIZE` = 128 KiB, defined in
  `src/ports/pico/grr_port_config.h`.
* **Maximum image size:** 131072 − 4096 = **126 976 bytes**. Headers
  declaring larger images are rejected before any erase.
* `program_store_init()` refuses to start if `__flash_binary_end` > region
  base — a growing firmware can never silently overlap stored programs.
* All erase/program operations run through the Pico SDK's
  `flash_safe_execute()` (interrupts disabled, RAM-resident
  `flash_range_*` routines). Operations are one sector/page at a time
  (~45 ms), with normal execution — and USB servicing — in between.
* **Single slot.** Starting an upload erases the metadata sector first,
  immediately invalidating the old program. There is no dual-slot
  rollback. The firmware area is never written or erased.

### Metadata (first 48 bytes of the metadata sector)

| offset | size | field                                              |
|--------|------|----------------------------------------------------|
| 0      | 4    | magic `GRRP` (0x50525247 LE)                       |
| 4      | 4    | metadata version (1)                               |
| 8      | 4    | image length in bytes                              |
| 12     | 32   | SHA-256 of the image                               |
| 44     | 4    | crc32 of bytes 0..43                               |

Validity = magic + version + length sanity + crc + (implicitly) the
SHA-256 recorded only after verification. An erased sector (all 0xFF)
means **no valid program**.

---

## 3. Upload protocol (grr-xfer v1)

Authoritative spec: `include/grrvm/program_transfer.h`.

*All multi-byte integers are little-endian. No C structs cross the wire.*

```
frame := type:u8  length:u32le  payload[length]
```

| type | direction | payload |
|------|-----------|---------|
| `0x01` HEADER  | host → device | `version:u8, flags:u8, format_version:u16, image_length:u32, chunk_size:u32, sha256[32], crc32:u32` over the previous 44 bytes (48 total) |
| `0x02` CHUNK   | host → device | `index:u32, data_len:u32, data[data_len], crc32:u32` over `index‖data_len‖data` |
| `0x03` ABORT   | host → device | empty |
| `0x80` RESPONSE | device → host | `status:u8, ack_type:u8, reserved:u16, aux:u32` (8 bytes) |

Sequence:

1. Host sends HEADER. The device validates **before erasing anything**:
   protocol version, image format version, word-aligned length in
   `[12, 126976]`, `chunk_size == 256`, header crc32.
2. Device answers `READY` (`status=0`, `ack=0x01`, `aux=chunk_size`) —
   or a rejection. Receiving a HEADER at *any* time (including mid-
   transfer) restarts: the storage is re-erased and `READY` re-sent.
3. Host sends chunks 0,1,2,… strictly in order; device verifies framing
   and crc32, writes to flash, answers `OK (ack=0x02, aux=index)`.
   `ERR_SEQ (aux=expected)` tells the host exactly which chunk to
   (re)send; `ERR_CRC (aux=index)` asks for a resend of that chunk.
4. When the declared length is reached the device: flushes the last
   partial page → re-reads the stored image → SHA-256 → compares →
   structurally validates the `.grr.o` (parser + VM fit) → only then
   marks it valid → answers `VERIFIED (status=1, ack=0x00)`.

Status codes (`include/grrvm/program_transfer.h`):

| code | name | meaning |
|------|------|---------|
| 0x00 | OK | READY / chunk accepted |
| 0x01 | VERIFIED | image installed (final success) |
| 0x02 | ERR_BAD_VERSION | unsupported protocol version |
| 0x03 | ERR_BAD_LENGTH | invalid length/chunk size/too large |
| 0x04 | ERR_FORMAT | unsupported image format version |
| 0x05 | ERR_SEQ | wrong chunk index (`aux` = expected) |
| 0x06 | ERR_CRC | chunk checksum mismatch (`aux` = index) |
| 0x07 | ERR_FLASH | flash erase/program failed |
| 0x08 | ERR_HASH | final SHA-256 mismatch |
| 0x09 | ERR_STRUCTURE | not a structurally valid `.grr.o` |
| 0x0A | ERR_VM_MEMORY | image needs more VM memory than exists |
| 0x0B | ERR_BUSY | frame not valid in this state |
| 0x0C | ERR_FRAME | malformed frame / header CRC error |
| 0x0D | ERR_CANCELLED | ABORT acknowledged |

**Retrying / restarting:** resend the same chunk on timeout or CRC error,
jump to `aux` on `ERR_SEQ`, or simply send a fresh HEADER — a new HEADER
always restarts (re-erasing) the single slot. The device has no internal
timeout; it stays in the current state until the host retries, aborts, or
restarts.

---

## 4. Host-side upload

```bash
# 1. Build and flash the firmware ONCE
./build.sh --pico
# hold BOOTSEL while plugging in, then:
cp build/pico/grrvm.uf2 /media/$USER/RPI-RP2/

# 2. Replace the guest program as often as you like (no reflash)
python3 tools/upload_grr.py /dev/ttyACM0 guest_programs/blink_led.grr.o
```

* The uploader hashes the **exact `.grr.o` bytes on disk** (Python
  `hashlib`), never a hex dump or regenerated header.
* Success is printed **only after the device's `VERIFIED` response**,
  which the device sends after re-reading its own flash, recomputing
  SHA-256 and validating the binary. Finishing the byte transfer is not
  reported as success.
* Progress, per-chunk retries, and diagnostics (the SHA-256) are printed;
  exit code is non-zero on any failure.
* Transport: pyserial when installed (all platforms, `pip install
  pyserial`; required on Windows/macOS recommended), otherwise a stdlib
  termios fallback on Linux.
* Useful flags: `--retries N`, `--timeout S`, `--header-timeout S`,
  `--verify-timeout S`.
* The protocol is transport-independent (framing is defined on a raw
  byte stream), so Web Serial or any other byte transport can implement
  it; `tools/upload_grr.py` and `tests/common/test_program_transfer.c`
  pin the byte format (golden-vector test).

---

## 4. Host-side upload (WSL - Windows Subsystem for Linux)
1. Install usbipd-win in Powershell (Administrator)
```bash
winget install --interactive --exact dorssel.usbipd-win
```
2. Restart Powershel (Administrator)
```bash
usbipd list
```
You should see something like this
```bash

BUSID  VID:PID    DEVICE
1-4    2e8a:0005  Raspberry Pi Pico
```
3. Bind, then attach Powershell (Administrator)
```bash
usbipd bind --busid 1-4
usbipd attach --wsl --busid 1-4
```

4. Check from WSL
Back in WSL:
```bash
lsusb
```
Then:

```bash
ls /dev/ttyACM*
```
You should hopefully get:

```bash
/dev/ttyACM0
```
Then:

```bash
dmesg | tail -30
```
Now with some minor changes...
`Note: sudo is important here, it might fail when referencing /dev/ttyACM0 without root.`

```bash
# 1. Build and flash the firmware ONCE
./build.sh --pico
# hold BOOTSEL while plugging in, then:
cp build/pico/grrvm.uf2 /media/$USER/RPI-RP2/

# 2. Replace the guest program as often as you like (no reflash) 
sudo python3 tools/upload_grr.py /dev/ttyACM0 guest_programs/blink_led.grr.o
```

* The uploader hashes the **exact `.grr.o` bytes on disk** (Python
  `hashlib`), never a hex dump or regenerated header.
* Success is printed **only after the device's `VERIFIED` response**,
  which the device sends after re-reading its own flash, recomputing
  SHA-256 and validating the binary. Finishing the byte transfer is not
  reported as success.
* Progress, per-chunk retries, and diagnostics (the SHA-256) are printed;
  exit code is non-zero on any failure.
* Transport: pyserial when installed (all platforms, `pip install
  pyserial`; required on Windows/macOS recommended), otherwise a stdlib
  termios fallback on Linux.
* Useful flags: `--retries N`, `--timeout S`, `--header-timeout S`,
  `--verify-timeout S`.
* The protocol is transport-independent (framing is defined on a raw
  byte stream), so Web Serial or any other byte transport can implement
  it; `tools/upload_grr.py` and `tests/common/test_program_transfer.c`
  pin the byte format (golden-vector test).

---

## 5. Behavior after failures

| situation | behavior |
|-----------|----------|
| Upload interrupted (unplugged, killed host) | metadata sector was erased at start → next boot reports **no valid program**, device enters upload state, firmware intact |
| Wrong hash / invalid structure | image is **not** marked valid; device reports the error and returns to upload state; a new upload works immediately |
| Flash write failure | `ERR_FLASH`, image not marked valid, device recoverable |
| Device powered off mid-verify | same as interrupted: marker never written → no partial image is ever executed |
| Running program + new upload header | program is stopped (via `vm_host_poll`), then flash is modified |
| Just want the old program back | re-upload it — single slot, no rollback |

**Never execute a partial image:** the validity marker is written only
after SHA-256 + structural validation, and startup only loads marked
images.

---

## 6. Integrity and its limitations

* Per-chunk CRC32 detects line corruption early (it is **not** a
  substitute for the final hash).
* SHA-256 compares host bytes against the exact stored bytes; it proves
  the image arrived intact and was stored faithfully.
* **SHA-256 does not authenticate the sender.** Anyone with access to the
  serial port can install a validly-hashed image. This protocol is a
  integrity/robustness mechanism, not a security boundary; there is no
  signature check and none should be claimed.
* A matching hash is not proof of validity: the device still runs the
  full structural parser before marking the image usable.

---

## 7. Diagnosing failed uploads

1. **`no response to header`** — right port? device booted? the Pico
   prints its banner about 2 s after power-up; check `dmesg`/Device
   Manager for the CDC port.
2. **`ERR_BAD_LENGTH`** — image larger than 126 976 bytes, or not a
   multiple of 4 (truncated copy of the `.grr.o`).
3. **`ERR_HASH` / repeated `ERR_CRC`** — serial line corrupt or another
   process is reading the port (serial monitors steal bytes). Close other
   terminals, retry.
4. **`ERR_STRUCTURE`** — the file passed SHA-256 but is not a valid
   `.grr.o` for this VM (bad offsets, oversized program, bad entry
   point). Test the same file locally: `./grrvm -r <file>.grr.o`.
5. **`ERR_VM_MEMORY`** — global-data region larger than the VM heap
   (`VM_HEAP_SLOTS` in `src/ports/pico/grr_port_config.h`).
6. **Device stuck after a failure** — it is in the upload state; just run
   the uploader again (a fresh HEADER restarts cleanly). Reset the board
   if needed; the firmware is never affected.
7. During development the device logs parse/validation errors over USB
   serial (`vm_error` messages) — keep a terminal attached to see them.

---

## 8. Building and testing

```bash
./build.sh                      # POSIX build + ctest + runs grrvm
./build.sh --no-test            # POSIX build only
./build.sh --pico               # Pico build -> build/pico/grrvm.uf2
ctest --output-on-failure       # from build/posix
```

Tests added for this feature:

| test | covers |
|------|--------|
| `tests/common/test_load_vm.c` | shared loader: valid synthetic + real images, truncation, misalignment, bad offsets/lengths, malformed global records, entry point, size limits, error codes |
| `tests/common/test_program_transfer.c` | protocol state machine with mock storage: happy path, bad versions/lengths, out-of-order/duplicate/wrong-size chunks, corrupt CRC, hash mismatch, invalid binary with matching hash, flash failures, abort, restart/recovery, noise resync, golden wire-format vectors |
| `tests/common/test_sha256.c` | SHA-256 published vectors + Python reference digests over a real `.grr.o`; CRC32 check values |
| `tests/posix/test_load_vm_file.c` | file wrapper: every checked-in `guest_programs/*.grr.o`, missing/empty/truncated/misaligned files |

### Manual hardware test (requires a physical Pico)

1. Flash `build/pico/grrvm.uf2`, open `/dev/ttyACM0`.
2. `python3 tools/upload_grr.py /dev/ttyACM0 guest_programs/blink_led.grr.o`
   → expect `SUCCESS: device verified SHA-256…`.
3. Press reset (or power-cycle): the program runs again — proving the
   image survived in flash without a reflash.
4. Start an upload, unplug mid-transfer, power back on: device reports
   `No valid guest program in storage` and accepts a new upload.
