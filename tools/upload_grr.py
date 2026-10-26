#!/usr/bin/env python3
"""Reference host uploader for GRRVM runtime guest-program installation.

Sends a compiled .grr.o image to a GRRVM Raspberry Pi Pico over USB serial
using the "grr-upload v1" protocol (see documentation/RUNTIME_UPLOAD.MD
and include/grrvm/program_transfer.h for the authoritative spec).

Key properties:
  * SHA-256 is computed over the EXACT .grr.o bytes on disk - the same
    bytes the assembler emitted. Nothing is re-encoded or re-formatted.
  * Success is reported only after the device's final VERIFIED response,
    which it sends after re-reading the stored image, comparing the
    SHA-256 digest and structurally validating the binary. Finishing the
    upload of bytes is NOT reported as success.
  * Per-chunk CRC32 + acknowledgements, with bounded retries and resume
    on sequence errors. Timeouts and failures exit non-zero.

Transport: uses pyserial when installed (any OS, recommended on
Windows/macOS); otherwise falls back to a stdlib termios implementation
on POSIX systems. On Windows without pyserial this script exits with a
clear message.

Usage:
    python3 tools/upload_grr.py /dev/ttyACM0 guest_programs/blink_led.grr.o
"""

import argparse
import hashlib
import os
import struct
import sys
import time
import zlib

# --- Protocol constants (must match include/grrvm/program_transfer.h) ---

PROTOCOL_VERSION = 1
IMAGE_FORMAT_VERSION = 1
CHUNK_SIZE = 256
HEADER_PAYLOAD_SIZE = 48

FRAME_HEADER = 0x01
FRAME_CHUNK = 0x02
FRAME_ABORT = 0x03
FRAME_RESP = 0x80

RESP_OK = 0x00
RESP_VERIFIED = 0x01
RESP_ERR_BAD_VERSION = 0x02
RESP_ERR_BAD_LENGTH = 0x03
RESP_ERR_FORMAT = 0x04
RESP_ERR_SEQ = 0x05
RESP_ERR_CRC = 0x06
RESP_ERR_FLASH = 0x07
RESP_ERR_HASH = 0x08
RESP_ERR_STRUCTURE = 0x09
RESP_ERR_VM_MEMORY = 0x0A
RESP_ERR_BUSY = 0x0B
RESP_ERR_FRAME = 0x0C
RESP_ERR_CANCELLED = 0x0D

RESP_FRAME_SIZE = 13

ERROR_HINTS = {
    RESP_ERR_BAD_VERSION: "device speaks an older/newer protocol version",
    RESP_ERR_BAD_LENGTH: "image length/chunk size invalid for the device storage",
    RESP_ERR_FORMAT: "unsupported guest-image format version",
    RESP_ERR_SEQ: "chunk sequence error (resumed automatically when possible)",
    RESP_ERR_CRC: "chunk checksum mismatch (resend; serial line may be noisy)",
    RESP_ERR_FLASH: "device flash erase/program failed",
    RESP_ERR_HASH: "stored bytes do not match the SHA-256 - transfer corrupted",
    RESP_ERR_STRUCTURE: "image failed structural validation on the device",
    RESP_ERR_VM_MEMORY: "not enough VM memory on the device for this image",
    RESP_ERR_BUSY: "device not ready for this message (retry)",
    RESP_ERR_FRAME: "malformed frame (retry)",
    RESP_ERR_CANCELLED: "transfer cancelled",
}


def build_header_frame(image: bytes, sha256: bytes) -> bytes:
    payload = struct.pack(
        "<BBHII",
        PROTOCOL_VERSION,
        0,  # flags
        IMAGE_FORMAT_VERSION,
        len(image),
        CHUNK_SIZE,
    ) + sha256
    assert len(payload) == 44
    payload += struct.pack("<I", zlib.crc32(payload) & 0xFFFFFFFF)
    assert len(payload) == HEADER_PAYLOAD_SIZE
    return bytes([FRAME_HEADER]) + struct.pack("<I", len(payload)) + payload


def build_chunk_frame(index: int, data: bytes) -> bytes:
    body = struct.pack("<II", index, len(data)) + data
    crc = zlib.crc32(body) & 0xFFFFFFFF
    frame = bytes([FRAME_CHUNK]) + struct.pack("<I", len(body) + 4) + body
    return frame + struct.pack("<I", crc)


def parse_response(frame: bytes):
    """Returns (status, ack_type, aux) or None if not a RESPONSE frame."""
    if len(frame) != RESP_FRAME_SIZE or frame[0] != FRAME_RESP:
        return None
    if struct.unpack_from("<I", frame, 1)[0] != 8:
        return None
    status, ack = frame[5], frame[6]
    aux = struct.unpack_from("<I", frame, 9)[0]
    return status, ack, aux


# --- Transport -----------------------------------------------------------


class SerialTransport:
    """Byte-oriented serial transport with read timeout."""

    def __init__(self, port: str, baud: int, timeout: float):
        self._pyserial = None
        self._fd = None
        self._buf = b""

        try:
            import serial  # type: ignore

            self._pyserial = serial.Serial(port, baud, timeout=timeout)
            return
        except ImportError:
            pass

        if os.name != "posix":
            raise SystemExit(
                "error: pyserial is required on this platform "
                "(pip install pyserial), or run on a POSIX system."
            )

        import select
        import termios
        import tty

        self._select = select
        self._fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attrs = termios.tcgetattr(self._fd)
        tty.setraw(self._fd)
        termios.tcsetattr(self._fd, termios.TCSANOW, attrs)
        self._timeout = timeout

    def write(self, data: bytes) -> None:
        if self._pyserial is not None:
            self._pyserial.write(data)
            self._pyserial.flush()
            return
        total = 0
        while total < len(data):
            try:
                total += os.write(self._fd, data[total:])
            except BlockingIOError:
                self._select.select([], [self._fd], 1.0)

    def _read_some(self) -> bytes:
        if self._pyserial is not None:
            waiting = self._pyserial.in_waiting
            chunk = self._pyserial.read(max(waiting, 1))
            return chunk
        ready, _, _ = self._select.select([self._fd], [], self._timeout)
        if not ready:
            return b""
        try:
            return os.read(self._fd, 4096)
        except BlockingIOError:
            return b""

    def _find_response(self) -> int:
        """Index of a RESPONSE frame start in the buffer, or -1. Trims
        leading garbage (keeping a possible partial length field)."""
        idx = 0
        limit = len(self._buf) - 5
        while idx <= limit:
            if (
                self._buf[idx] == FRAME_RESP
                and self._buf[idx + 1 : idx + 5] == struct.pack("<I", 8)
            ):
                self._buf = self._buf[idx:]
                return 0
            idx += 1
        if len(self._buf) > 4:
            self._buf = self._buf[-4:]
        return -1

    def read_response(self, timeout: float) -> tuple:
        """Reads until one well-formed RESPONSE frame arrives or timeout."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self._find_response() == 0:
                if len(self._buf) >= RESP_FRAME_SIZE:
                    frame = self._buf[:RESP_FRAME_SIZE]
                    self._buf = self._buf[RESP_FRAME_SIZE:]
                    parsed = parse_response(frame)
                    if parsed is not None:
                        return parsed
                    continue  # not parseable: resync on the next scan

            # Frame incomplete (or nothing yet): always read more bytes.
            chunk = self._read_some()
            if chunk:
                self._buf += chunk

        return None

    def drain(self, seconds: float = 0.2) -> None:
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            chunk = self._read_some()
            if not chunk:
                continue


# --- Upload session ------------------------------------------------------


def fail(message: str, code: int = 1):
    print(f"error: {message}", file=sys.stderr)
    sys.exit(code)


def upload(args) -> None:
    with open(args.image, "rb") as f:
        image = f.read()

    if len(image) == 0:
        fail(f"{args.image} is empty")
    if len(image) % 4 != 0:
        fail(
            f"{args.image} is {len(image)} bytes - not a multiple of 4, "
            "so it cannot be a .grr.o word stream"
        )

    sha256 = hashlib.sha256(image).digest()
    print(f"image:    {args.image} ({len(image)} bytes)")
    print(f"sha256:   {sha256.hex()}")
    print(f"port:     {args.port}")

    transport = SerialTransport(args.port, args.baud, args.timeout)
    transport.drain()

    header = build_header_frame(image, sha256)

    # --- HEADER / READY handshake (retried: device may still be booting) ---
    ready = False
    for attempt in range(1, args.retries + 1):
        transport.write(header)
        resp = transport.read_response(args.header_timeout)
        if resp is None:
            print(f"  no response to header (attempt {attempt}/{args.retries})")
            continue
        status, ack, aux = resp
        if status == RESP_OK and ack == 0x01:
            ready = True
            print(f"  READY (chunk size {aux})")
            break
        fail(
            f"device rejected the header: {ERROR_HINTS.get(status, hex(status))} "
            "(nothing was written to device storage)"
        )
    if not ready:
        fail(
            "device never answered READY - is it plugged in, in GRRVM firmware, "
            "and is this the right serial port?"
        )

    # --- CHUNK loop --------------------------------------------------------
    index = 0
    total_chunks = (len(image) + CHUNK_SIZE - 1) // CHUNK_SIZE
    retries_used = 0

    while index < total_chunks:
        offset = index * CHUNK_SIZE
        chunk = image[offset : offset + CHUNK_SIZE]
        transport.write(build_chunk_frame(index, chunk))

        resp = transport.read_response(args.timeout)
        if resp is None:
            if retries_used >= args.retries:
                fail(f"timeout waiting for ACK of chunk {index}")
            retries_used += 1
            print(f"  timeout on chunk {index}, resending "
                  f"({retries_used}/{args.retries})")
            continue

        status, ack, aux = resp

        if status == RESP_OK and ack == 0x02 and aux == index:
            index += 1
            if index % 16 == 0 or index == total_chunks:
                pct = 100 * index // total_chunks
                print(f"  {index}/{total_chunks} chunks ({pct}%)")
            continue

        if status == RESP_ERR_SEQ:
            # Device expects a different index (duplicate/out-of-order):
            # resume from what it asked for.
            if aux <= index and retries_used < args.retries:
                retries_used += 1
                index = aux
                print(f"  device expects chunk {aux}, resuming")
                continue
            fail(f"sequence error, device expects chunk {aux}")

        if status == RESP_ERR_CRC:
            if retries_used >= args.retries:
                fail(f"chunk {aux} failed CRC {args.retries} times")
            retries_used += 1
            index = aux
            print(f"  CRC error on chunk {aux}, resending")
            continue

        fail(f"chunk {index} rejected: {ERROR_HINTS.get(status, hex(status))}")

    # --- Final verification (device: SHA-256 readback + structure check) ---
    print("  image sent, waiting for device verification...")
    resp = transport.read_response(args.verify_timeout)
    if resp is None:
        fail(
            "timeout waiting for the final verification response - the upload "
            "is incomplete; simply run this script again (the device restarts "
            "the transfer from its header)"
        )

    status, ack, aux = resp
    if status == RESP_VERIFIED:
        print("SUCCESS: device verified SHA-256, validated the binary and "
              "installed the program.")
        print("It will run on the next device reset (or immediately, if idle).")
        return

    if status == RESP_OK and ack == 0x01:
        # Device restarted the transfer (e.g. our header retry raced ahead).
        fail("device re-answered READY instead of VERIFIED; re-run the upload")

    fail(
        "device rejected the image: "
        + ERROR_HINTS.get(status, hex(status))
        + " - the stored program is NOT valid; fix the problem and re-upload"
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Upload a .grr.o guest program to a GRRVM Pico over USB serial."
    )
    parser.add_argument("port", help="serial port, e.g. /dev/ttyACM0 or COM5")
    parser.add_argument("image", help="path to the compiled .grr.o file")
    parser.add_argument("--baud", type=int, default=115200,
                        help="baud rate (ignored by USB CDC; default 115200)")
    parser.add_argument("--timeout", type=float, default=2.0,
                        help="per-chunk response timeout in seconds")
    parser.add_argument("--header-timeout", type=float, default=3.0,
                        help="timeout waiting for READY")
    parser.add_argument("--verify-timeout", type=float, default=30.0,
                        help="timeout waiting for final verification")
    parser.add_argument("--retries", type=int, default=3,
                        help="retries for header/chunks (default 3)")
    args = parser.parse_args()

    if not os.path.isfile(args.image):
        fail(f"no such file: {args.image}")

    upload(args)


if __name__ == "__main__":
    main()
