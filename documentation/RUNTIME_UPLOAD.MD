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
