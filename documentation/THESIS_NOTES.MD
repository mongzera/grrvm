# THESIS NOTES — evaluation design, telemetry, and review findings

State of this document: written during implementation of the measurement
telemetry (see §4). It records *why* the code looks the way it does, plus the
results of a line-by-line review of the thesis proposal against this artifact.
Keep it updated when the measurement path or the manuscript changes.

---

## 1. Project snapshot

GRRVM — a lightweight bytecode VM for microcontrollers (Raspberry Pi Pico,
RP2040), with a POSIX port used for development and testing.

| Item | Value |
|---|---|
| Core + headers + ports + tests | ~9,100 LOC C |
| Assembler (Java, separate artifact) | 21 files, ~1,620 LOC |
| Commits | 76 (Jul 30 – Nov 11 2026) |
| Guest ISA | stack machine, opcodes in `include/grrvm/opcodes.h` |
| Native/FFI | `INVOKE_NATIVE <name>` → FNV-1a hash → `native_lookup()` |

Layering (strictly one-way, no cycles — see `include/grrvm/vm.h`):
`types.h → prim_val.h → vm_alloc.h → vm.h → vm_mem.h / vm_thread.h / vm_gc.h`.

### Ports

| | Pico | POSIX |
|---|---|---|
| `VM_HEAP_SLOTS` | 25,600 (200 KB / 8 B) | 131,072 (1 MB / 8 B) |
| `VM_PROGRAM_MAX_SIZE` | 1,024 | 256 KB |
| `VM_MAX_THREADS` | 2 | 24 |
| op / call stack | 64 / 16 slots | huge (heap-allocated per thread) |

The 200 KB Pico heap is **intentional**; do not "optimise" it down.

---

## 2. Allocator facts that the thesis depends on

* **Hybrid**: slab classes `{8, 16, 32}` slots (64/128/256 B) when
  `n_slots + 1 ≤ 32`; otherwise a **buddy forest**.
* **Buddy forest, not a buddy system**: `vm_alloc_init()` decomposes
  `VM_HEAP_SLOTS / 64` base blocks into one tree per set bit, so a heap need
  not be a power of two.
  * Pico: 400 base blocks = 256 + 128 + 16 → trees of order 8, 7, 4.
  * POSIX: 2,048 = 2^11 → a single tree of order 11.
* Base block = 64 slots = 512 B. `buddy2` layout: one `longest[]` byte per
  node holding `order + 1`; 0 = allocated.
* Slab caches: each cache is one base block split into
  `VM_BUDDY_BASE_BLOCK_SLOTS / class_size` sub-blocks, threaded as an
  intrusive free list (a free sub-block's `data` field holds the next
  index). At most `VM_MAX_SLAB_CACHES` (64) caches live at once — a hard
  resource limit, not a hint.
* **16-bit block ceiling**: metadata is packed as
  `(block_size << 16) | n_slots`, so a single block cannot exceed 65,536
  slots. `finish_alloc()` asserts this. Worth a stated limitation.
* **GC**: root-only mark-sweep. Roots are each thread's `op_stack`,
  `call_stack_frame`, and `global_heap_block_address`. Sweep walks
  `ram[]` in strides of the block size. Runs **only** when `g_malloc`
  fails, then retries the allocation.
* **The core invariant**: `ram[]` never holds `TYPE_REFERENCE`. Enforced at
  runtime in `set_vm_mem()` (`include/grrvm/vm_mem.h`), which rejects such a
  store with an assert plus `STATE_ERROR`. Consequence: the object graph is
  not traced, cycles are unrepresentable, and no cycle collector exists.
* Because root scanning is bounded by configuration and sweeping is bounded
  by `VM_HEAP_SLOTS`, GC pause has a **static upper bound** that does not
  grow with live-object count. This is the thesis's real-time argument —
  derive it rather than borrowing one from the literature (see §6).

---

## 3. The three SOP variables — operational definitions

| Variable | Definition used by the code | Where |
|---|---|---|
| External fragmentation | `Ef = 1 − longest / total_free`, where `total_free = buddy_free + slab_free` and `longest = max(longest_buddy_block, longest_slab_block)`. Returns `NAN` when `total_free == 0`. | `measure_ext_frag()` |
| External fragmentation, full heap scan | `Ef_full = 1 − longest_free_region / free_slots`, where both terms come from a single linear walk of `ram[]` that skips every `TYPE_HEAP_BLOCK` by its physical `block_size` (buddy and slab together). Returns `NAN` when `free_slots == 0`. | `vm_full_heap_scan()` + `measure_full_heap_frag()` |
| GC jitter | Distribution of stop-the-world pause `= post − pre` per collection, **excluding** the heap-statistics scans. Report n, mean, σ, min, max, p99. | `pre/post_gc_telemetry()` |
| Allocation speed | `elapsed_us / allocs` over repeated batches, fast-path allocations only, frees outside the timed window. | `telemetry_bench_alloc()` |

Rationale for the definitions:

* **Both free pools must be in the denominator.** An earlier version divided
  by buddy free slots only. That understates Ef and, worse, reports
  `0.0` (perfectly unfragmented) for a heap packed entirely into slab
  caches — the most fragmented state possible. The `NAN` guard exists so a
  full heap is never mistaken for a perfect one.
* **`longest` is `max()` of both sides** because a free 8/16/32-slot slab
  sub-block really can satisfy a small request.
* **Sample outside the timed window.** `vm_heap_stats()` is
  O(#trees + #slab caches) and used to run *inside* the measured GC
  interval, inflating every pause by the cost of the fragmentation metric.
  `pre_gc_telemetry()` now samples Ef first and opens the window second;
  `post_gc_telemetry()` closes the window before sampling Ef.
* **Batch timing for allocation speed**, not per-allocation timestamps: two
  `hal_clock_us()` reads per allocation would swamp the sub-microsecond
  paths being measured. Per-GC timestamps are fine — there are far fewer of
  them.
* **Two fragmentation metrics, on purpose.** `Ef` asks the allocator a
  question: *what is the largest block you can hand out right now?* It reads
  buddy `longest[]` and the slab free lists, so it is the right number for
  GRRVM's own behaviour but it embeds the allocator's policy. `Ef_full`
  asks the heap a question: *how much of the address space is physically
  contiguous?* It walks `ram[]` from slot 0 the same way `sweep_objects()`
  does, uses each block's header to skip allocated regions, and counts the
  gaps. Because that walk only assumes block-size headers — which
  MicroPython's linear block heap also has — it is the definition that can
  be applied to both runtimes. The honest caveat is that a free run found
  this way can cross slab sub-block or slab-cache boundaries and therefore
  need not be allocatable as one request; `Ef_full` measures contiguity,
  not allocatability, and that is exactly why it is fair for a
  cross-runtime layout comparison. Report it alongside `Ef`, not instead
  of it.

---

## 4. Telemetry implementation map

Files:

| File | Role |
|---|---|
| `include/grrvm/telemetry/vm_telemetry.h` | sample structs + sizes |
| `include/grrvm/telemetry/gc_telemetry.h` | API + the no-print contract |
| `src/core/memory/gc_telemetry.c` | sampling, bench, CSV flush, CRC |
| `src/core/native_wrappers/internal_telemetry_wrapper.c` | FFI surface |
| `src/ports/pico/init.c` | strong `telemetry_pre_flush_hook()` |

### Contract

1. **Nothing prints while the guest runs.** Sampling only writes to static
   arrays. All output happens in `telemetry_flush()`, invoked once at the end
   of the program via `INVOKE_NATIVE`. USB-CDC/UART blocking therefore cannot
   enter a sample.
2. **Static, saturating buffers.** No `malloc`, so exhaustion shows up in the
   linker map. Each stream keeps a monotonic total; a full buffer *drops*
   samples and the dump reports `(recorded, total)` — silent overwriting was
   the old behaviour and would have corrupted a run past 2,048 collections.
3. **CRC trailer** over the emitted payload, so a truncated USB stream is
   detectable. Verified against `zlib.crc32`.

### FFI (all reachable with `INVOKE_NATIVE`)

| Native | Args (pushed left→right) | Effect |
|---|---|---|
| `telemetry_reset` | — | clears flags, buffers and counters (call first) |
| `set_gc_runtime_telemetry_flag` | on | enable pause stream |
| `set_ext_frag_telemetry_flag` | on | enable Ef stream |
| `bench_alloc` | batches, per_batch, min_slots, max_slots, seed | timed allocation batches |
| `full_heap_scan` | — | O(`VM_HEAP_SLOTS`) unified layout scan; records one `FULLSCAN` row and pushes `Ef_full` as a `TYPE_FLOAT` |
| `rand_seed` | seed | fixes `math_rng_range`'s sequence (0 = reseed from entropy, rejected) |
| `set_gc_enabled` | on | 0 = `g_malloc` skips GC (isolate allocator) |
| `telemetry_flush` | — | emit CSV once, after the run |
| `timer_us` | — | pushes monotonic µs (guest-side section timing) |
| `dump_gc_runtime_telemetry`, `dump_ext_frag_telemetry` | — | deprecated, rows only |

Natives pop in reverse push order (last pushed pops first) — see
`wrap_math_rng_range()` for the convention.

### Output format

```
#GRRTELEM,3,heap_slots,25600,pause_max,512,frag_max,128,alloc_max,64,fullscan_max,64
#GRRBENCH,batches,30,per_batch,100,min_slots,32,max_slots,2048,seed,12345
PAUSE,<seq>,<delta_us>
FRAG,<seq>,<pre_buddy_free>,<pre_slab_free>,<pre_longest>,<post_buddy_free>,<post_slab_free>,<post_longest>
ALLOC,<seq>,<allocs>,<elapsed_us>
FULLSCAN,<seq>,<total_slots>,<allocated_slots>,<free_slots>,<free_regions>,<longest_free_region>
#GRRTELEM,END,pause,<recorded>,<total>,frag,<recorded>,<total>,alloc,<recorded>,<total>,fullscan,<recorded>,<total>,crc,0x<CRC32>
```

Version 3 added the `FULLSCAN` row and the `fullscan` trailer pair; a
version-2 reader that ignores unknown row kinds and trailer keys still
parses the stream.

The CRC covers every byte from the first `#GRRTELEM` line up to (not
including) the `END` line. Host-side:

```python
payload = b"".join(line + b"\n" for line in collected_lines_before_END)
assert zlib.crc32(payload) & 0xFFFFFFFF == trailer_crc
ef = 1 - longest / (buddy_free + slab_free)      # per FRAG row
speed_us = elapsed_us / allocs                   # per ALLOC row, allocs > 0
ef_full = 1 - longest_free_region / free_slots   # per FULLSCAN row
```

### Constraint: a native cannot hold live blocks

The collector's only roots are each active thread's `call_stack_frame` and
`op_stack` plus the global heap block header (`mark_object()` in
`src/core/memory/gc.c`). A reference held in a C static — for example inside
a benchmark native — is invisible to the collector, so the block it points at
is swept at the next collection. That rules out a C-side "retained islands"
workload: an early `bench_churn` native kept 8 islands in a static array and
every collection reclaimed them, leaving the heap fully free and producing a
degenerate post-GC Ef of 0.0.

Any workload that must keep data alive across collections has to hold it in
**guest locals**, which are part of the call frame. That is why
`gc_jitter_test.grr` runs its churn loop in guest code with anchors in locals
1..3, and why `telemetry_bench_alloc()` uses only the no-GC fast path.

### Buffer sizing (static, `.bss`)

`VM_TELEM_MAX_PAUSE` 512 (2 KB), `VM_TELEM_MAX_FRAG` 128 (3 KB),
`VM_TELEM_MAX_ALLOC` 64 (512 B), `VM_TELEM_MAX_FULLSCAN` 64 (1.25 KB),
`VM_TELEM_BENCH_MAX_LIVE` 256 refs (1 KB). Total ≈ 7.9 KB (the original
6.6 KB plus the 64-entry FULLSCAN buffer).

---

## 5. Measurement hygiene — mandatory controls

* Build the measurement firmware with `VM_DEBUG_ENABLE=OFF` and
  `ENABLE_VM_COLORS=OFF`. `mark_object()`/`sweep_objects()` call `vm_info()`
  per object; USB serial formatting inside the measured region would
  dominate the pause it is measuring.
* `vm_start()` prints a `PROGRAM EXECUTION … Delta` line — exclude it.
* Match heap capacity between GRRVM and MicroPython, or report both and
  normalise. Otherwise the comparison is confounded.
* Replay the **same seeded allocation sequence** on both runtimes. The seed
  is echoed in `#GRRBENCH` for exactly this reason.
* Clock is 32-bit µs on both ports (wraps ≈ 71 min); deltas are wrap-safe,
  absolute timestamps are not.
* Statistics: Welch's t-test compares *pause samples*, not σ. Comparing
  dispersion needs Levene / Brown–Forsythe. Report CIs and effect sizes, not
  just p-values. "n = 30" matches 30 timed batches and deserves a real
  justification rather than a CLT appeal.

---

## 6. Manuscript review — findings

### 6.1 Citation audit (AI-assisted search left bad entries)

Confirmed **wrong** — remove or replace:

1. `Doe, J., Smith, A., & Jones, B. (2023). … IEEE IoT Journal 10(4),
   3120–3131.` Placeholder names; no such paper.
2. `Zhao, L., Hu, X., & Chen, Y. (2021). Deterministic Stop-The-World GC
   bounds… IEEE TCAD 40(8), 1620–1633.` Not found.
3. `Maj, P., Czajkowski, G., & Hoffmann, C. (2010). Schism…` Wrong authors,
   pages and DOI. Correct: **Pizlo, F., Ziarek, L., Maj, P., Hosking, A. L.,
   Blanton, E., & Vitek, J. (2010).** *PLDI '10*, pp. **146–159**,
   doi:10.1145/1806596.1806615.
4. `West, R. M. (2021)` — Vancouver fragment pasted into an APA list; no
   title/journal, year out of position.

Verified **real** (keep, with fixes): García, May & Nutting, *Integrated
hardware garbage collection*, ACM TECS 20(5), art. 40, 2021 — add the
article number; Bosse, Bornemann & Lüssem 2023 (REXAVM), arXiv:2302.09002 —
three authors, venue is a preprint/FICLOUD paper, not solo Bosse.

Also: duplicate 2026 MicroPython entries need `2026a`/`2026b`; confirm
1.29.0 is the version actually flashed. Disclose AI-assisted literature
search in the methodology.

### 6.2 Theoretical framework is a misreading of Schism

The draft claims Schism "proved that real-time predictability is restored
when GC traversal is structurally constrained to fixed-size pools (slabs)".
Schism does the opposite: it tolerates fragmentation in **large arrays** via
arraylets and concurrent/incremental reclamation. Replace the borrowed claim
with the project's own bound: marking is bounded by
`VM_MAX_THREADS × (op_stack + call_stack)`, sweeping by `VM_HEAP_SLOTS`, so
pause is statically bounded and independent of live-set size. Then validate
measured max pause ≤ bound.

### 6.3 Scope contradictions vs the artifact

The draft declares the assembler out of scope and says it will "not include
multithreading", yet ships a 1,620-LOC assembler, a compiler/IDE that lowers
a TypeScript-like language (including **n-dimensional arrays**, linearised
row-major) to the ISA, and multiple thread contexts. Rewrite Scope to: VM
evaluated; assembler and compiler/IDE present but not evaluated; concurrency
present but not evaluated. The "we will use GCC" sentence is also confused —
GCC builds the VM, it does not produce guest code.

### 6.4 Terminology and factual fixes

* `y * width + x` is **row-major linearisation** (index polynomial evaluated
  by Horner's scheme), not "Horner's Method" for root-finding. Cite a
  linear-algebra/textbook source and name it precisely.
* "Avoiding cyclic references vulnerable to deadlock" is wrong: cycles do
  not deadlock, and mark-sweep handles them correctly; they *leak* under
  reference counting. The real benefit is that cycles are unrepresentable,
  so no cycle collector is needed.

### 6.5 Trade-offs to state honestly ("Cons")

1. No linked lists, trees, graphs, objects or closures in guest code — flat
   arrays of scalars only (the compiler flattens n-d arrays).
2. Bounds checks are per block, not per dimension: `set_vm_mem()` validates
   `address < VM_HEAP_SLOTS` and `offset ≤ n_slots` of the flat block, so an
   out-of-range `A[y][x]` inside a legal block is invisible to the VM unless
   the front end inserts checks.
3. No `REALLOC` opcode: growth is allocate-copy-free, which is exactly the
   churn that creates fragmentation.
4. `H_FREE` exists alongside collection — "minimal manual memory management"
   means optional manual free, automatic on exhaustion.
5. No compaction, so an allocation can still fail after a collection if no
   single region fits.
6. The 16-bit metadata ceiling caps a single block at 65,536 slots.

---

## 7. Open work

1. ~~Rewrite the evaluation guest programs~~ — **done**:
   * `gc_jitter_test.grr`: `rand_seed 12345`, anchors in locals 1..3, 500
     rounds x 4 allocations = the SOP's 2,000, both telemetry flags on, one
     `telemetry_flush` at the end. Islands survive collections, so post-GC
     Ef is meaningful (observed: 128,256 slots free but longest block only
     65,536).
   * `allocation_speed.grr`: `bench_alloc(30, 64, 8, 255, 20261)` = 1,920
     timed allocations, peak live heap 64 * 256 = 16,384 slots so it fits
     the Pico. The bounds matter: a request of n slots reserves n + 1 for
     the header, is a slab allocation only while that fits in 32 slots
     (hence min 8, not 32 — 32 slots needs 33 and goes to buddy) and rounds
     up to a buddy block (hence max 255, not 256 — 256 slots needs 257 and
     becomes a 512-slot block, 64 * 512 = 32,768 slots, more than the Pico
     heap).
   * Both `.grr.o` images regenerated (`assembler/bin/grrasm x.grr > x.grr.o`);
     `tests/posix/test_load_vm_file.c` loads them, so the suite covers them.
2. ~~Host-side analysis script~~ — **done**: `tools/analyze_telemetry.py`
   verifies the CRC, reports dropped samples, and computes pause stats
   (n/mean/σ/min/p50/p99/max), Ef before and after each GC, full-heap `Ef_full`
   from `FULLSCAN` rows, and µs per allocation. Use `--json` for
   machine-readable output and `--strict` to fail on a CRC mismatch.
3. **Evaluation chapter**: one table per SOP variable, both runtimes, matched
   heaps, same seed.
4. **References**: apply §6.1, then the scope and framework rewrites.
5. Optional: fold in unused artifact material (`gc_telemetry` pre/post
   samples, `vm_heap_stats`, `guest_programs/tensor_horner.grr`, the upload
   protocol's CRC32+SHA-256 verification) as non-evaluated features.

---

## 8. Verification commands

```bash
./build.sh                 # POSIX, no tests (default: tests are not packaged)
./build.sh --test          # POSIX, compile + run all suites via one runner
./build.sh --pico          # firmware + UF2
./build.sh --pico --test   # firmware with system checks at boot

# Assembler on POSIX: the port's compile() is a stub, so invoke the jar.
assembler/bin/grrasm prog.grr > prog.grr.o
./build/posix/grrvm -r prog.grr.o

# Measure: run a workload image, then analyse the flushed dump.
./build/posix/grrvm -r guest_programs/gc_jitter_test.grr.o \
    | python3 tools/analyze_telemetry.py
./build/posix/grrvm -r guest_programs/allocation_speed.grr.o \
    | python3 tools/analyze_telemetry.py --json --strict
```

Note: `-r` prints the VM's instruction log to stderr, so redirect `2>/dev/null`
when piping a dump into the analyser (the CSV goes to stdout).

Latest known sizes (Pico, no tests, with telemetry): FLASH 101,628 B,
RAM 19,740 B. With tests: FLASH 127,436 B, RAM 21,956 B (pre-telemetry).

---

## 9. Pitfalls encountered

* **`malloc` in hot/measured paths.** Telemetry used to `malloc` its buffers
  and memzero them on every flag set, silently discarding earlier samples.
  Buffers are static now and flag setting is idempotent.
* **Ring-buffer wraparound** destroyed data past capacity while the dump
  printed a short, non-chronological slice. Saturation + total counters
  replaced it.
* **USB CDC has no flow control.** A long dump can be truncated; the CRC
  trailer plus per-row sequence numbers make that visible.
* **`telemetry_pre_flush_hook()`** waits up to ~5 s for the host to attach on
  the Pico. It runs after the program, so it costs nothing measured.
* **POSIX `compile()` is a stub** (logs and returns). Binary images come from
  the Java assembler, written to stdout.
* `hal_clock_us()` returns 32-bit µs on both ports; `hal_clock_ns()` is 64-bit
  but on the Pico it is derived from µs, so there is no extra resolution.
* Debug logging inside mark/sweep will dominate any pause measurement —
  always measure with debug off.
