# GRRVM - Gamat-Ruyeras-Robinson Virtual Machine

GRRVM is a 32-bit, stack-based bytecode virtual machine written in C11. It
targets bare-metal microcontrollers (Raspberry Pi Pico / RP2040) and POSIX
systems, with the POSIX port used for development, testing and measurement.

The VM is deliberately small and self-contained: a fixed-size heap, fixed-size
thread stacks, a registerless stack machine, and a native-function interface
for anything that needs the host (GPIO, timers, RNG, telemetry). Guest
programs are assembled by a separate Java tool into a raw stream of 32-bit
words (`.grr.o`) and loaded by the same platform-independent loader on every
port.

---

## 1. System overview

```
  .grr source
      |
      |  assembler/bin/grrasm  (Java, separate artifact)
      v
  .grr.o  = 32-bit little-endian word stream
      |
      |  load_vm_from_bytes()  (src/common/load_vm_bytes.c)
      v
 +------------------------------ VM ------------------------------+
 |  program[]        decoded instruction words                    |
 |  ram[]            prim_val heap slots (globals + dynamic heap) |
 |  vm_threads[]     pc / stacks / call frames                    |
 |  global_var_table table-slot -> heap reference                 |
 |  allocator        Buddy Forest + Slab                          |
 |  native registry  FNV-1a symbol -> C function                  |
 |  telemetry        static sample buffers, flushed once          |
 +----------------------------------------------------------------+
      |                                  |
      | HAL: timer, GPIO, math, RNG       | stdio
      v                                  v
  POSIX port                        Pico port
  clock_gettime, /dev/urandom       RP2040 timers, USB CDC + UART0,
  file loader, stdout               flash program store
```

Host-side tools:

| Tool | Purpose |
|---|---|
| `assembler/bin/grrasm` | Assembles `.grr` into `.grr.o` (binary on stdout) and writes `.grr.o.dump.txt`. |
| `tools/upload_grr.py` | Uploads a `.grr.o` to a Pico over the grr-xfer v1 serial protocol. |
| `tools/analyze_telemetry.py` | Verifies the telemetry CRC and computes Ef, Ef_full, pause statistics and allocation speed. |

---

## 2. Repository layout

| Path | Contents |
|---|---|
| `include/grrvm/` | Core public headers: VM, values, allocator, GC, threads, loader, telemetry, opcodes. |
| `include/grrvm/hal/` | Platform abstraction: init, loader, timer, GPIO, math/RNG, print, system. |
| `include/parser/` | `bytecode_parse.h` - the hardened `.grr.o` parser. |
| `include/native_wrappers/` | Declarations for the C functions exposed to guest code. |
| `src/core/evaluation/` | One evaluator per opcode class (stack, arithmetic, branching, memory, native, ...). |
| `src/core/memory/` | Allocator (buddy + slab), mark-sweep GC, telemetry implementation. |
| `src/core/native_wrappers/` | Native registry and the C wrappers (math, GPIO, GC, debug, telemetry, heap dump). |
| `src/common/` | Platform-independent VM construction, image loading, SHA-256, upload protocol. |
| `src/ports/posix/` | POSIX startup/CLI, file loader, clock, RNG, GPIO stubs, stdout printing. |
| `src/ports/pico/` | Pico startup, stdio, persistent flash program store, upload transport, HAL. |
| `src/debug/` | Debug helpers (stack inspection, test runner integration). |
| `tests/` | Shared/per-port suites, linked into the target rather than separate binaries. |
| `guest_programs/` | Example `.grr` sources plus checked-in `.grr.o` images and dumps. |
| `documentation/` | Specification and design notes (this file, opcodes, upload protocol, thesis notes). |
| `assembler/` | Java assembler sources and the prebuilt `bin/grrasm` launcher. |

---

## 3. Layering and build model

Header dependencies are strictly one-way (documented at the top of
`include/grrvm/vm.h`):

```
types.h, default_config.h, vm_log.h, vm_math.h     leaves
        |
      prim_val.h                                   value model
        |
   vm_global_table.h, vm_alloc.h                   need values only
        |
       vm.h                                        full VM
        |
   vm_mem.h, vm_thread.h, vm_gc.h, ...             need the full VM
```

`vm.h` must not include anything from the last row. The allocator and the
global table are embedded by value in `struct VM`, so they have to be defined
before it; direct memory accessors need the complete VM and therefore sit
above it.

CMake builds exactly one port per build tree:

* `grrvm_core` - `src/core/**`, `src/common/**`, `src/debug/**`, shared
  `src/*.c`, and the selected `src/ports/<port>/**`. The selected port
  directory is a public include path because `default_config.h` includes
  `grr_port_config.h`.
* `grrvm` - `src/main.c` plus the test sources when tests are enabled.
  POSIX and Pico builds use separate directories (`build/posix`,
  `build/pico`).

The core never includes Pico SDK headers. Only `src/ports/pico/` does, and
the Pico SDK usage requirements are attached to `grrvm_core` in CMake.

| Port | Characteristics |
|---|---|
| POSIX | Development/measurement. Filesystem loader, CLI (`-r/--run`), high-resolution monotonic clock, `/dev/urandom` seeding, GPIO stubs. |
| Pico | RP2040. No filesystem; guest images live in a reserved flash region, loaded directly from XIP memory. USB CDC and UART0 stdio. |

---

## 4. Execution model

### 4.1 VM and threads

`struct VM` owns:

| Field | Meaning |
|---|---|
| `program` | Heap-allocated array of `VM_PROGRAM_MAX_SIZE` instruction words. |
| `program_size` | Number of valid code words in `program[]`. |
| `_program_start` | Entry point, a code-relative index into `program[]` (the `_global` subroutine). |
| `ram` | Heap-allocated array of `VM_HEAP_SLOTS` `prim_val` slots. This is both global memory and the dynamic heap. |
| `global_var_table` | Array of table rows, each holding a `TYPE_REFERENCE` to a global variable's first data slot. |
| `allocator` | Buddy forest + slab descriptors (embedded by value). |
| `vm_threads` | `VM_MAX_THREADS` thread contexts (heap-allocated array). |
| `gc_disabled` | Inverted flag: zero (the zeroed default) means collection enabled. |

`struct VM_Thread` contains:

```
status              INACTIVE / ACTIVE / WAIT
pc                  next instruction index in program[]
pc_checkpoint       debug/restart checkpoint
sp                  operand-stack top (-1 = empty)
csp                 call-stack top (-1 = empty)
sfp                 frame base into call_stack_frame[]
n_local_vars        locals declared in the current frame
op_stack[]          VM_OP_STACK_MAX prim_val operands
call_stack_frame[]  VM_CALL_STACK_FRAME_MAX prim_val locals, all frames
call_stack[]        VM_CALL_STACK_MAX saved-frame records
```

There is one operand stack per thread, not per call frame. `CALL` saves the
caller's `pc`, `sfp`, `sp` and `n_local_vars`, advances `sfp` past the
caller's locals, and starts the callee with `n_local_vars = 0`. `RET`
restores `pc`, `sfp` and `n_local_vars`; it intentionally does not restore
`sp`, so return values left on the operand stack survive. Arguments are
consumed by the callee with `STORE_L`; the convention is visible in
`guest_programs/`.

Frame area exhaustion is a call-stack error, not a heap allocation: local
storage is a fixed array. `VM_CALL_STACK_MAX` bounds recursion depth.

### 4.2 Dispatch loop

`vm_start()` creates thread 0 at `_program_start` and enters `vm_loop()`.
The loop is cooperative and round-robin: on each pass it executes at most
one instruction for each active thread, in thread-index order. There is no
preemption and no time slice.

```
while (active threads remain) {
    poll host every 256 loop iterations; if it requests stop, deactivate all
    for each thread:
        if inactive: continue
        opcode = program[pc++]
        switch (opcode & 0xF0):
            0x00 stack, 0x10 arithmetic, 0x20 conditional,
            0x30 branching, 0x40 bitwise, 0x50 local memory,
            0x60 global memory, 0x70 heap memory, 0xA0 native
}
```

The high nibble selects an evaluator (`src/core/evaluation/*.c`); the low
nibble selects the operation. Unknown categories (`OPC_SYS = 0x80`,
`OPC_UART = 0x90`) and unknown opcodes are execution errors. Errors normally
log via `vm_error()` and deactivate the offending thread; other threads can
continue.

`HALT` deactivates the current thread. The loop exits once all threads have
been inactive for more than `VM_MAX_THREADS` consecutive passes. Only thread 0
is created by the runtime; `vm_new_thread()` exists for ports and tests but no
guest-visible spawn instruction exists.

`vm_host_poll()` is a weak hook called approximately every 256 loop passes.
The Pico port overrides it to feed serial bytes into the upload engine and to
stop a running program when a valid upload header arrives. The default hook
never interrupts.

`vm_start()` also prints a `PROGRAM EXECUTION ... Delta` timing line to
stdout. It is outside the telemetry payload and must not be treated as a
measurement sample.

### 4.3 Opcode classes

The instruction set is defined in `include/grrvm/opcodes.h` and documented in
`OPCODES.MD`. In summary:

| Class | Operations |
|---|---|
| `0x00` stack | `HALT`, `PUSH`, `POP`, `DUP`, `ROT`, `SWAP`, `PUSH_ADDR`, `PUSH_T` |
| `0x10` arithmetic | `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `INC`, `DEC` |
| `0x20` conditional | `CMPEQ`, `CMPNEQ`, `CMPLT`, `CMPLTE`, `CMPGT`, `CMPGTE` |
| `0x30` branching | `JUMP`, `JZ`, `JNZ`, `CALL`, `RET` (relative offsets) |
| `0x40` bitwise | `AND`, `OR`, `XOR`, `NOT`, `LSHIFT`, `RSHIFT` |
| `0x50` local memory | `STORE_L`, `LOAD_L` (frame-relative indices) |
| `0x60` global memory | `STORE`, `LOAD` (global table indices) |
| `0x70` heap memory | `H_ALLOC`, `H_FREE`, `H_STORE`, `H_LOAD`, `H_STORE_OFF`, `H_LOAD_OFF`, `H_SET_TYPE` |
| `0xA0` native | `INVOKE_NATIVE` (FNV-1a hash of the symbol) |

`LOCK` and `UNLOCK` are declared in the opcode enum and accepted by the
assembler, but their evaluator cases are not implemented. They currently fall
through without effect; do not rely on them.

---

## 5. Value and memory model

### 5.1 `prim_val`

Every guest value and every RAM slot is an 8-byte `prim_val`:

```c
typedef struct prim_val {
    union { word data; float float_data; };  // 4 bytes
    byte metadata;                           // state << 4 | type
    byte gc_mark;                            // mark-sweep bit
} prim_val;
```

Metadata packs a 4-bit state in the high nibble and a 4-bit type in the low
nibble:

| State | Meaning |
|---|---|
| `STATE_ERROR` | Invalid value. |
| `STATE_OPEN` | Writable slot/variable. |
| `STATE_LOCKED` | Header or otherwise non-writable. |
| `STATE_CONSTANT` | Immutable global. |

| Type | Notes |
|---|---|
| `TYPE_NULL` | Uninitialized. |
| `TYPE_U8`, `U16`, `U32` | Unsigned integers. |
| `TYPE_I8`, `I16`, `I32` | Signed integers. |
| `TYPE_FLOAT` | IEEE 754 single precision. |
| `TYPE_U64`, `TYPE_I64` | Present in the enum but unsupported by the parser/VM. |
| `TYPE_HEAP_BLOCK` | Allocation header; payload is packed block metadata. |
| `TYPE_REFERENCE` | Points at the first data slot of a heap block. |

`word` and `half` are 32- and 16-bit unsigned integers. There is no 64-bit
guest integer or double; arithmetic is 32-bit or float.

### 5.2 RAM and heap-block headers

`ram[]` is the entire data address space: globals are materialized as heap
blocks at load time, and `H_ALLOC` allocations come from the same array.

Every allocation is preceded by one header slot:

```
slot:    [ header ][ data 0 ][ data 1 ] ... [ data n-1 ][ physical padding ]
type:    HEAP_BLOCK   <--------------- n_slots ---------------->
locked               <---------------- block_size ---------------->
```

`header.data = (block_size << 16) | n_slots`, where:

* `block_size` is the physical number of slots reserved (header included) and
  is what the GC and the full-heap scan use to skip an allocation.
* `n_slots` is the logical usable size returned to the guest.
* The 16-bit fields cap a single allocation at 65,536 slots.

A `TYPE_REFERENCE` value stores the absolute slot index of the first data
slot, so `reference.data - 1` is the header address.

### 5.3 Reference invariant

`ram[]` must never contain a `TYPE_REFERENCE`. References live only in thread
stacks (`op_stack`, `call_stack_frame`) and in the global heap block's header.
`set_vm_mem()` enforces this with an assertion plus a runtime `STATE_ERROR`.

This is what makes the collector a root scan rather than a graph traversal:
there are no edges inside RAM to follow, cycles are unrepresentable, and no
cycle collector is needed. It also means a C static holding a reference is not
a GC root and will be swept.

### 5.4 Memory access and types

`get_vm_mem()` copies a slot out. `set_vm_mem()` checks the heap range, the
block's logical `n_slots` bound, the slot state, and type compatibility:

* a `TYPE_NULL` slot accepts a converted value;
* otherwise the value's type rank must not exceed the slot's rank;
* `promote_types()` determines the stored type, and `convert_prim()`
  re-encodes the payload.

Bounds checks are per flat block, not per array dimension. `H_SET_TYPE`
rewrites the type of every logical slot in a block.

Global variables are one outer heap block containing a `TYPE_HEAP_BLOCK`
header per variable followed by its data. The global variable table stores a
reference to each variable's first data slot. `LOAD`/`STORE` take a table
index; `PUSH_ADDR` pushes the row's reference so guest code can compute
`base + index` for `H_LOAD`/`H_STORE`.

---

## 6. Guest image format (`.grr.o`)

### 6.1 Binary layout

The image is a sequence of 32-bit little-endian words:

| Word | Field | Meaning |
|---|---|---|
| 0 | `program_offset` | Absolute word index where the code begins (3 + all global data words). |
| 1 | `program_size` | Number of code words (excludes global data). |
| 2 | `global_start` | Entry point, relative to the start of the code array. |
| 3 .. `program_offset-1` | Global data records | See below. |
| `program_offset` .. | Program | `program_size` instruction words. |

Each global data record is:

```
[ metadata ][ length ][ value 0 ] ... [ value length-1 ]
```

`metadata` is the assembler type id, with bit 7 set for constants
(`_const*` sections). The parser maps `0x01`..`0x03` (u8/u16/u32),
`0x05`..`0x07` (i8/i16/i32) and `0x09` (float). It rejects `0x04`, `0x08`,
`0x0A` and `0x0B` (u64/i64/double/char), any metadata above bit 7, and any
length above `0xFFFF`.

The assembler emits data sections first, then code. `_global` is the required
entry subroutine; `global_start` is its word offset in the code stream.

### 6.2 Loading and validation

`load_vm_from_bytes_ex()` in `src/common/load_vm_bytes.c` is the single
loader used by both ports. It requires:

* a non-NULL, 4-byte-aligned buffer;
* at least 12 bytes;
* a length that is an exact multiple of 4.

It creates the VM, then `bytecode_parser()` validates:

* `program_offset >= 3` and within the image;
* `program_size` fits in the remaining words and in `VM_PROGRAM_MAX_SIZE`;
* a nonzero program with an entry point inside `[0, program_size)`;
* every global record is complete, has a supported type, fits before
  `program_offset`, and does not overflow the VM heap.

The parser copies the code and global data into VM-owned memory; the caller
may free or reuse the image immediately afterwards. On failure the
half-built VM is destroyed, and `load_vm_from_bytes_ex()` distinguishes
`GRR_LOAD_ERR_IMAGE` from `GRR_LOAD_ERR_MEMORY`.

`include/grrvm/vm_program_loader.h` is a legacy streaming loader that is not
used by the build; the shared byte loader above is authoritative.

### 6.3 Assembling and running

```bash
assembler/bin/grrasm guest_programs/prog.grr > guest_programs/prog.grr.o
./build/posix/grrvm -r guest_programs/prog.grr.o
```

`grrasm` writes the binary image to stdout (shell redirection is required)
and writes `prog.grr.dump.txt` next to the source. The POSIX port's
`-c/--compile` command is a stub that only logs; the Java assembler is the
real compiler. Pico images are uploaded instead of read from a filesystem
(see §10).

---

## 7. Heap, allocator and collector

### 7.1 Configuration

| Setting | POSIX | Pico |
|---|---|---|
| `VM_HEAP_SLOTS` | 131,072 slots (1 MiB) | 25,600 slots (200 KiB) |
| `VM_PROGRAM_MAX_SIZE` | 262,144 words | 1,024 words |
| `VM_MAX_THREADS` | 24 | 2 |
| `VM_OP_STACK_MAX` | 130,048 | 64 |
| `VM_CALL_STACK_MAX` | 16,384 | 16 |
| `VM_CALL_STACK_FRAME_MAX` | 524,288 | 16 |

The heap is divided into 64-slot base blocks (512 bytes each). A request for
`n` usable slots reserves `n + 1` slots (header included).

### 7.2 Slab allocator

Small allocations (physical need `<= 32` slots) use one of three slab classes:

```
VM_SLAB_CLASS_SIZES = { 8, 16, 32 }   // slots per sub-block
```

A slab cache is exactly one 64-slot buddy base block split into
`64 / class_size` sub-blocks. Free sub-blocks are threaded as an intrusive
free list: a free sub-block's own `data` field stores the next free slot
index, so no side table is needed.

* `available_head[class]` links caches of that class that still have a free
  sub-block.
* `base_block_owner[base]` routes a free address back to its cache in O(1).
* Each cache has a `used_count`; when it reaches zero, the cache is removed
  from the available list, the base block is returned to the buddy forest,
  and the descriptor is released.
* There are at most `VM_MAX_SLAB_CACHES` (64) live caches. This is a hard
  resource limit: a small allocation can fail even when the buddy forest has
  free space if all descriptors are claimed.

The class is selected by first fit over `n_slots + 1`; for example 7 usable
slots need 8 physical slots and use the 8-slot class, while 31 usable slots
need 32 and use the 32-slot class.

### 7.3 Buddy forest

Larger allocations (and slab backing blocks) come from a buddy forest. The
heap is partitioned into `VM_HEAP_SLOTS / 64` base blocks; `vm_alloc_init()`
decomposes that count into one tree per set bit, so the heap does not have to
be a power of two.

* Pico: 400 base blocks = 256 + 128 + 16 → trees of order 8, 7 and 4.
* POSIX: 2,048 base blocks = 2^11 → one tree of order 11.

Each tree uses the classic single-array `buddy2` layout: `longest[i]` stores
`order + 1` of the largest free block reachable under node `i`, with 0
meaning fully allocated. Allocation descends toward a child that can satisfy
the request and then recomputes every ancestor. Freeing promotes a parent to
full capacity only when both children are at full capacity (a real merge),
otherwise it takes the maximum child value. No per-block free-list pointers
or separate bitmap are required.

`want_order` is the smallest order with `64 << order >= n_slots + 1`. The
block's physical `block_size` is `64 << order`.

### 7.4 Allocation and free paths

```
g_malloc(thread, n)
  -> g_malloc_direct(vm, n, 0)
       n + 1 <= 32 ? slab class (8/16/32)
                   : buddy forest at computed order
  -> on failure: run_gc() unless gc_disabled
  -> retry g_malloc_direct(vm, n, 1)
```

`finish_alloc()` writes the header and initializes every logical data slot to
`TYPE_NULL`. The returned reference points to `header + 1`.

`g_free()` validates the reference, reads the block size from the header, and
dispatches to `slab_free()` (block size <= 32) or `buddy_free()`. Freeing a
pointer twice, or a pointer whose header is no longer a heap block, is
reported as a heap error.

There is no `REALLOC` opcode and no compaction. Growth is allocate-copy-free,
which is itself a source of fragmentation.

### 7.5 Mark-sweep collector

`run_gc()` is a synchronous stop-the-world collection. It runs only when
`g_malloc()` cannot satisfy a request (or when the guest invokes the
`gc_run` native), and the allocation is retried afterwards.

Roots:

1. the global heap block header (`global_heap_block_address`), if present;
2. for each active thread, every `call_stack_frame` slot from 0 through
   `sfp + n_local_vars`;
3. every `op_stack` slot from `sp` down to 0.

Every `TYPE_REFERENCE` found in a root is turned into a mark on its block
header (`set_tagged()`). Because references cannot live in `ram[]`, there is
no transitive marking phase.

Sweep walks `ram[]` linearly from slot 0. Each `TYPE_HEAP_BLOCK` header gives
its physical `block_size`; unmarked blocks are freed, marked blocks have
their mark cleared for the next collection, and the cursor skips the entire
block. A corrupted block size is a hard GC error.

Consequences:

* Pause is statically bounded: marking is bounded by
  `VM_MAX_THREADS * (op_stack + call_stack_frame)` and sweeping by
  `VM_HEAP_SLOTS`, independent of the live-object count.
* Inactive threads are not roots. A reference parked only in an inactive
  thread's stack can be collected.
* A C static inside a native is not a root; any block it points to can be
  swept. Workloads that must retain objects across collections have to keep
  the references in guest locals (see `gc_jitter_test.grr`).
* Collection does not compact, so allocation can still fail after a
  successful sweep when no single contiguous block is large enough.
* `set_gc_enabled(0)` disables automatic collection so the allocator can be
  measured in isolation; the failed allocation still retries and reports out
  of memory rather than collecting.

### 7.6 Heap statistics and fragmentation

`vm_heap_stats()` reports the allocator's own view in O(trees + `VM_MAX_SLAB_CACHES`):

| Field | Meaning |
|---|---|
| `buddy_free_slots` | Free slots owned by the buddy forest. |
| `longest_buddy_block` | Largest block (header included) the forest can return now. |
| `slab_free_slots` | Free slots in partially used slab caches. |
| `longest_slab_block` | Largest free sub-block (0, 8, 16 or 32). |

The allocator-aware fragmentation metric is:

```
Ef = 1 - max(longest_buddy_block, longest_slab_block)
         / (buddy_free_slots + slab_free_slots)
```

Both free pools must be in the denominator; omitting slab free slots reports
a heap packed entirely into slab caches as perfectly unfragmented. `Ef` is
`NAN` when total free is zero.

`vm_full_heap_scan()` provides the algorithm-independent counterpart. It
walks `ram[]` linearly from slot 0, exactly like `sweep_objects()`, skips
every `TYPE_HEAP_BLOCK` by its physical `block_size`, and treats everything
else as free. The result is one physical free-region map over buddy and slab
allocations alike. A corrupt header is treated as one allocated slot so the
scan can continue.

```
Ef_full = 1 - longest_free_region / free_slots       (NAN if free_slots == 0)
```

`Ef_full` measures contiguity, not allocatability: a free run may cross slab
sub-block or cache boundaries and therefore need not be returnable as one
request. That is intentional - it is the definition a linear block heap such
as MicroPython's can compute with the same walk, so it is the fair
cross-runtime comparison. Report it alongside `Ef`, not instead of it.

---

## 8. Native function interface

Guest code calls a host function with:

```
INVOKE_NATIVE <symbol name>
```

The assembler replaces the name with its 32-bit FNV-1a hash (offset basis
`0x811C9DC5`, prime `0x01000193`). The VM executes `INVOKE_NATIVE` by reading
the next word as a hash and calling `native_lookup()`; an unresolved hash
deactivates the thread.

`native_registry_init()` resets a fixed 128-entry registry and calls the
registration functions for each wrapper group. Duplicate or colliding hashes
are rejected. `REGISTER_NATIVE()` (a constructor macro) exists in
`vm_native.h`, but the current wrappers register explicitly at boot.

A `NativeFn` receives the calling `VM_Thread*` and communicates through its
operand stack: arguments are pushed left-to-right, so the wrapper pops them
in reverse order; results are pushed back. Type checks and stack failures
normally deactivate the thread.

| Group | Symbols |
|---|---|
| Math/RNG | `math_rng_range`, `rand_seed` |
| GPIO | `gpio_init`, `gpio_write`, `gpio_read`, `gpio_set`, `gpio_clear`, `gpio_toggle` |
| GC | `gc_run` |
| Debug | `vm_stack_debug` |
| Heap dump | `dump_heap_stdout`, `dump_heap_file` |
| Telemetry | `telemetry_reset`, `telemetry_flush`, `set_gc_runtime_telemetry_flag`, `set_ext_frag_telemetry_flag`, `bench_alloc`, `set_gc_enabled`, `full_heap_scan`, `timer_us`, `dump_gc_runtime_telemetry`, `dump_ext_frag_telemetry` |

`full_heap_scan` runs the O(`VM_HEAP_SLOTS`) layout scan immediately, records
one `FULLSCAN` sample, and pushes `Ef_full` as a `TYPE_FLOAT`. It allocates
nothing and prints nothing.

---

## 9. Telemetry

### 9.1 Contract

1. Sampling never prints. It only writes into static arrays. All output
   happens in `telemetry_flush()`, which the guest calls once after its
   workload (via `INVOKE_NATIVE`). USB/UART blocking therefore cannot enter a
   sample.
2. Buffers are static and sized at compile time; a full buffer drops samples
   and increments a total counter. The dump reports `(recorded, total)` for
   every stream, so truncation is visible. Nothing wraps silently.
3. Heap statistics are sampled outside the timed GC window: `pre_gc_telemetry()`
   records the pre-GC heap components first, then opens the pause timer;
   `post_gc_telemetry()` closes the timer before sampling the post-GC heap.

### 9.2 Streams

| Stream | Buffer | Content |
|---|---|---|
| `PAUSE` | `VM_TELEM_MAX_PAUSE` (512) | One stop-the-world GC pause in microseconds. |
| `FRAG` | `VM_TELEM_MAX_FRAG` (128) | Pre/post buddy free, slab free and longest block around one collection. |
| `ALLOC` | `VM_TELEM_MAX_ALLOC` (64) | One timed allocation batch: successful allocations and elapsed microseconds. |
| `FULLSCAN` | `VM_TELEM_MAX_FULLSCAN` (64) | One full heap layout scan. |

`telemetry_bench_alloc()` runs `batches` timed batches of `per_batch`
allocations, drawing sizes from `[min_slots, max_slots]` with a seeded LCG
(`state = 1664525 * state + 1013904223`). It uses `g_malloc_direct()` (the
no-GC fast path), records the elapsed time, then frees the batch after the
timer stops. The seed and parameters are echoed in `#GRRBENCH` so another
runtime can replay the same sequence.

### 9.3 Output format (version 3)

```
#GRRTELEM,3,heap_slots,<n>,pause_max,<n>,frag_max,<n>,alloc_max,<n>,fullscan_max,<n>
#GRRBENCH,batches,<n>,per_batch,<n>,min_slots,<n>,max_slots,<n>,seed,<n>
PAUSE,<seq>,<delta_us>
FRAG,<seq>,<pre_buddy_free>,<pre_slab_free>,<pre_longest>,<post_buddy_free>,<post_slab_free>,<post_longest>
ALLOC,<seq>,<allocs>,<elapsed_us>
FULLSCAN,<seq>,<total_slots>,<allocated_slots>,<free_slots>,<free_regions>,<longest_free_region>
#GRRTELEM,END,pause,<recorded>,<total>,frag,<recorded>,<total>,alloc,<recorded>,<total>,fullscan,<recorded>,<total>,crc,0x<CRC32>
```

The CRC covers every byte from the first `#GRRTELEM` line up to, but not
including, the `END` line. `END` itself is printed raw. `telemetry_flush()`
is the only function that writes to stdio; calling it more than once re-emits
the recorded rows with a fresh CRC.

Host-side definitions:

```
Ef       = 1 - longest / (buddy_free + slab_free)      per FRAG row
Ef_full  = 1 - longest_free_region / free_slots        per FULLSCAN row
speed_us = elapsed_us / allocs                         per ALLOC row (allocs > 0)
```

`tools/analyze_telemetry.py` parses the stream, verifies the CRC, reports
dropped samples, computes pause statistics (n, mean, sigma, min, p50, p99,
max), Ef before/after each collection, Ef_full and allocation speed. `--json`
emits machine-readable results; `--strict` fails on a CRC mismatch. Only the
lines between the `#GRRTELEM` header and `END` are payload; boot banners and
the `PROGRAM EXECUTION ... Delta` line are ignored.

`dump_gc_runtime_telemetry` and `dump_ext_frag_telemetry` are deprecated
per-stream dumps without a header or CRC; new guests should call
`telemetry_flush`.

---

## 10. Ports

### 10.1 POSIX

`init()` in `src/ports/posix/init.c`:

1. initializes the HAL RNG (`/dev/urandom` or Xorshift32 fallback) and the
   native registry;
2. handles `-t/--test` by running the in-process suites before normal
   argument handling;
3. `-r/--run <file>.grr.o` reads the file into memory, calls the shared
   loader, runs the VM and terminates it;
4. `-c/--compile` is a stub; use `grrasm`;
5. with no arguments it prints help.

The clock is `CLOCK_MONOTONIC` (`hal_clock_ns()` 64-bit, `hal_clock_us()`
32-bit and wrap-safe for deltas). GPIO HAL functions are logging stubs. Heap
is 1 MiB / 131,072 slots.

### 10.2 Pico

Startup (`src/ports/pico/init.c`):

1. `stdio_init_all()`; USB CDC CRLF translation is disabled, UART0 keeps its
   default translation;
2. RNG and native registry initialization;
3. optional boot-time test suite when built with `GRRVM_BUILD_TESTS`;
4. a 2-second delay so the host can enumerate the CDC device;
5. `pico_transfer_init()` and `program_store_init()`, which checks that the
   reserved flash region does not overlap the firmware (`__flash_binary_end`);
6. if a valid persisted image exists and no upload header is already pending,
   load it from XIP-mapped flash and run it;
7. otherwise print the upload banner and service the serial protocol.

While a guest runs, `vm_loop()` polls `vm_host_poll()` about every 256
instructions. The Pico implementation feeds incoming bytes into the upload
engine without touching flash and stops the program only after a complete,
valid HEADER has arrived. Noise and partial frames do not stop a run. After a
verified upload, the VM built during validation is executed immediately; on
the following loop pass the persisted image is loaded from flash and run
again before the device settles back into the upload/idle state.

Serial output is broadcast to both enabled stdio drivers: USB CDC
(`/dev/ttyACM0`) and UART0 (`GP0` TX, `GP1` RX, 115200 8N1 by default). The
telemetry dump is therefore visible on either transport. `telemetry_flush()`
calls a strong `telemetry_pre_flush_hook()` that waits up to ~5 s for USB CDC
to connect; this runs after the guest program, so it does not affect
measurements. USB CDC has no flow control, so open the capture before a run
or rely on the CRC trailer to detect truncation.

#### Persistent program storage

The top 128 KiB of flash is reserved as a single-slot program store:

```
0x000000  firmware (boot2 + code)
   ...
0x1E0000  metadata sector   (4 KiB)  magic "GRRP", version, length,
                                     SHA-256, crc32
0x1E1000  image sectors     (124 KiB) 4 KiB erase units, 256 B pages
0x200000  end of flash
```

Maximum image size is `131072 - 4096 = 126,976` bytes. `program_store_init()`
refuses to run if the firmware overlaps the region. The firmware area is
never erased or written. Beginning an upload erases the metadata sector
first, invalidating any previous image; there is no dual-slot rollback. An
interrupted upload therefore leaves no valid program, and the device simply
returns to the upload state.

#### Upload protocol (grr-xfer v1)

The transport-neutral engine is `src/common/program_transfer.c`; the Pico
binds it to stdio and the flash store in
`src/ports/pico/program_transfer.c`. All multi-byte integers are
little-endian.

```
frame            := type:u8  length:u32le  payload[length]
HEADER  (0x01)   version, flags, format_version, image_length, chunk_size,
                 sha256[32], crc32 over the previous 44 bytes   (48 bytes)
CHUNK   (0x02)   index:u32, data_len:u32, data[data_len], crc32
ABORT   (0x03)   empty
RESPONSE(0x80)   status:u8, ack_type:u8, reserved:u16, aux:u32
```

Sequence: HEADER is validated before anything is erased; the device answers
READY (or a rejection), then accepts numbered 256-byte CHUNKs in order, each
with a CRC32. When the declared length is reached it flushes, re-reads the
stored bytes, compares SHA-256, structurally validates the `.grr.o` through
the shared loader, and only then marks the image valid and answers VERIFIED.
Errors use the `GRR_XFER_ERR_*` status codes in
`include/grrvm/program_transfer.h`. A new HEADER at any time restarts the
transfer.

SHA-256 provides integrity, not authentication: anyone with serial access can
install a validly hashed image. The protocol is a robustness mechanism, not a
security boundary.

Host side:

```bash
./build.sh --pico
# flash build/pico/grrvm.uf2 in BOOTSEL mode
python3 tools/upload_grr.py /dev/ttyACM0 guest_programs/full_heap_scan.grr.o
```

---

## 11. Tests and tooling

Tests are never standalone executables. Each suite returns a failure count;
the shared runner (`tests/run_tests.c`) runs the suites compiled into the
current target:

* POSIX: `grrvm --test` (also registered with CTest) before argument
  handling;
* Pico: at boot when configured with `-DGRRVM_BUILD_TESTS=ON`.

| Suite | Covers |
|---|---|
| `load_vm` | Shared loader: valid synthetic/real images, truncation, misalignment, bad offsets/lengths, malformed globals, entry point, size limits. |
| `sha256` | Published SHA-256 vectors, Python reference digests over a real `.grr.o`, CRC32 check values. |
| `program_transfer` | grr-xfer state machine with mock storage: happy path, bad versions/lengths, ordering/duplicate/CRC errors, hash mismatch, structural failure, flash failure, abort, restart, noise resync, golden frames. |
| `ref_guard` | The "no references in `ram[]`" invariant. |
| `heap` | Allocation, free, buddy/slab behavior, statistics, GC-visible headers, full heap scan (empty/slab/fragmented). |
| `load_vm_file` (POSIX) | File wrapper over every checked-in `guest_programs/*.grr.o`, plus missing/empty/truncated/misaligned files. |

Build and verification commands:

```bash
./build.sh                  # POSIX, tests not compiled, prints help
./build.sh --test           # POSIX + CTest + in-process suites
./build.sh --pico           # build/pico/grrvm.uf2
./build.sh --pico --test    # firmware with boot-time system checks
ctest --output-on-failure   # from build/posix
```

Measurement runs should use `-DVM_DEBUG_ENABLE=OFF` (or `./build.sh
--release`) and configure with `-DENABLE_VM_COLORS=OFF`: the collector logs
per object when debug is enabled, which would dominate the pause being
measured. Debug and instruction
logs go to stdout through `printf()`, like the telemetry dump; the analyzer
ingores everything before the `#GRRTELEM` header and after `END`, but the
logging itself still costs execution time.

---

## 12. Invariants and known limitations

**Invariants**

* Images are 32-bit-word aligned, little-endian and at most
  `VM_PROGRAM_MAX_SIZE` code words.
* `ram[]` never contains a `TYPE_REFERENCE`; references live only on thread
  stacks and the global block header.
* Every allocation has a `TYPE_HEAP_BLOCK` header immediately before its
  data; the GC and the full scan trust that header's physical `block_size`.
* The allocator owns all heap slots: buddy forest trees plus slab caches
  partition `[0, VM_HEAP_SLOTS)`.
* `gc_mark` is zero for a newly created value and is cleared on surviving
  blocks after every collection.
* Telemetry sampling never prints; only `telemetry_flush()` writes the CSV.

**Limitations**

* 32-bit values only. `TYPE_U64`, `TYPE_I64` and `TYPE_DOUBLE` are not
  supported end to end.
* A single allocation is capped at 65,536 slots by the packed 16-bit
  `block_size`/`n_slots` metadata.
* At most 64 slab caches can exist at once; small allocations can fail on
  descriptor exhaustion even with free heap.
* No compaction and no `REALLOC`. A collection can free plenty of memory and
  still leave no single block large enough.
* No reference edges in RAM: no linked lists, trees, object graphs or
  closures in guest code; arrays of scalars are the model.
* Bounds checks are per flat block, not per array dimension. Multi-dimensional
  index checks must be inserted by the front end.
* Only thread 0 is started by the runtime, and scheduling is cooperative
  round-robin with no preemption or blocking primitives.
* `LOCK`/`UNLOCK` are declared but not implemented; `OPC_SYS` and `OPC_UART`
  are reserved and rejected by the dispatch loop.
* Pico upload storage is a single slot with no rollback; SHA-256 verifies
  integrity but does not authenticate the sender.
* The POSIX `compile` command is a stub, and POSIX GPIO HAL functions only
  log.
* Debug logging inside mark/sweep changes the timing; measurement firmware
  must be built without it.

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
