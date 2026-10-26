/* Upload protocol (grr_xfer engine) tests.
 *
 * The state machine runs against a RAM-backed mock storage vtable, so
 * every scenario the task requires is testable without flash hardware:
 * valid transfer, bad versions/lengths, missing/out-of-order/duplicated
 * chunks, corrupted checksums, final SHA-256 mismatch, invalid binary
 * with a matching hash, flash write failure, abort, restart after an
 * interrupted transfer, and recovery afterwards.
 *
 * The mock's validate() stands in for the platform's structural
 * validation; SHA-256 comparison itself happens inside the shared engine,
 * exactly as on the device.
 */
#include <stdio.h>
#include <string.h>
#include "grrvm/program_transfer.h"

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); g_pass++; } \
    else      { printf("  [FAIL] %s  (line %d)\n", msg, __LINE__); g_fail++; } \
} while (0)

/* --- Mock storage --------------------------------------------------- */

#define MOCK_CAPACITY 8192u

static uint8_t  s_flash[MOCK_CAPACITY];
static uint32_t s_written;        /* bytes handed to write() */
static int      s_begin_count, s_write_count, s_flush_count;
static int      s_validate_count, s_commit_count, s_abort_count;
static int      s_fail_begin, s_fail_write, s_fail_commit;
static int      s_validate_status;   /* returned by validate() when != 0 */
static int      s_committed;
static int      s_erased;            /* begin() wiped the slot */

static uint8_t  s_responses[8192];
static size_t   s_response_len;
static int      s_response_count;

static void mock_reset(void) {
    memset(s_flash, 0, sizeof(s_flash));
    s_written = 0;
    s_begin_count = s_write_count = s_flush_count = 0;
    s_validate_count = s_commit_count = s_abort_count = 0;
    s_fail_begin = s_fail_write = s_fail_commit = 0;
    s_validate_status = 0;
    s_committed = 0;
    s_erased = 0;
    s_response_len = 0;
    s_response_count = 0;
}

static uint32_t mock_capacity(void* ctx) { (void)ctx; return MOCK_CAPACITY; }

static int mock_begin(void* ctx, const grr_xfer_header* h) {
    (void)ctx; (void)h;
    if (s_fail_begin) return -1;
    s_begin_count++;
    memset(s_flash, 0xFF, sizeof(s_flash)); /* old program gone from here on */
    s_written = 0;
    s_erased = 1;
    s_committed = 0;
    return 0;
}

static int mock_write(void* ctx, uint32_t offset, const uint8_t* data, uint32_t len) {
    (void)ctx;
    if (s_fail_write) return -1;
    if (offset + len > MOCK_CAPACITY) return -1;
    memcpy(s_flash + offset, data, len);
    s_written = offset + len;
    s_write_count++;
    return 0;
}

static int mock_flush(void* ctx) { (void)ctx; s_flush_count++; return 0; }

static const uint8_t* mock_image(void* ctx, uint32_t* len_out) {
    (void)ctx;
    *len_out = s_written;
    return s_flash;
}

static int mock_validate(void* ctx, const uint8_t* image, uint32_t len) {
    (void)ctx; (void)image; (void)len;
    s_validate_count++;
    return s_validate_status;
}

static int mock_commit(void* ctx, const grr_xfer_header* h) {
    (void)ctx; (void)h;
    if (s_fail_commit) return -1;
    s_commit_count++;
    s_committed = 1;
    return 0;
}

static int mock_abort(void* ctx) {
    (void)ctx;
    s_abort_count++;
    s_committed = 0;
    return 0;
}

static void mock_emit(void* ctx, const uint8_t* frame, size_t length) {
    (void)ctx;
    if (s_response_len + length <= sizeof(s_responses)) {
        memcpy(s_responses + s_response_len, frame, length);
        s_response_len += length;
        s_response_count++;
    }
}

static const grr_xfer_ops s_mock_ops = {
    .ctx = NULL,
    .capacity = mock_capacity,
    .begin = mock_begin,
    .write = mock_write,
    .flush = mock_flush,
    .image = mock_image,
    .validate = mock_validate,
    .commit = mock_commit,
    .abort = mock_abort,
    .emit = mock_emit,
};

/* --- Engine harness ------------------------------------------------- */

static grr_xfer_engine s_engine;

static void harness_reset(void) {
    mock_reset();
    grr_xfer_init(&s_engine, &s_mock_ops);
}

static void feed(const uint8_t* bytes, size_t len) {
    for (size_t i = 0; i < len; i++) grr_xfer_feed(&s_engine, bytes[i]);
}

/* Response i (all response frames are 13 bytes). */
static int response_at(int i, uint8_t* status, uint8_t* ack, uint32_t* aux) {
    if (i < 0 || i >= s_response_count) return -1;
    return grr_xfer_parse_response(s_responses + (size_t)i * GRR_XFER_RESP_FRAME_SIZE,
                                   GRR_XFER_RESP_FRAME_SIZE, status, ack, aux);
}

static void check_response(int i, uint8_t want_status, uint8_t want_ack,
                           const char* msg) {
    uint8_t status = 0xFF, ack = 0xFF;
    uint32_t aux = 0;
    if (response_at(i, &status, &ack, &aux) != 0) {
        printf("  [FAIL] %s  (no response %d, line %d)\n", msg, i, __LINE__);
        g_fail++;
        return;
    }
    if (status != want_status || ack != want_ack) {
        printf("  [FAIL] %s  (got status=0x%02X ack=0x%02X, want status=0x%02X ack=0x%02X, line %d)\n",
               msg, status, ack, want_status, want_ack, __LINE__);
        g_fail++;
        return;
    }
    printf("  [PASS] %s\n", msg);
    g_pass++;
}

static uint32_t last_aux(void) {
    uint8_t status, ack;
    uint32_t aux = 0;
    if (response_at(s_response_count - 1, &status, &ack, &aux) != 0) return 0;
    return aux;
}

/* --- Fixtures ------------------------------------------------------- */

#define IMAGE_LEN 600u /* 3 chunks: 256 + 256 + 88 */

static uint8_t s_image[IMAGE_LEN];

static void make_image(void) {
    for (uint32_t i = 0; i < IMAGE_LEN; i++) s_image[i] = (uint8_t)(i * 7u + 3u);
}

static void make_header(grr_xfer_header* h) {
    memset(h, 0, sizeof(*h));
    h->protocol_version = GRR_XFER_PROTOCOL_VERSION;
    h->image_format_version = GRR_XFER_IMAGE_FORMAT_VERSION;
    h->image_length = IMAGE_LEN;
    h->chunk_size = GRR_XFER_CHUNK_SIZE;
    grr_sha256(s_image, IMAGE_LEN, h->sha256);
}

static void send_header(const grr_xfer_header* h) {
    uint8_t frame[GRR_XFER_HEADER_FRAME_SIZE];
    size_t n = grr_xfer_build_header_frame(h, frame);
    feed(frame, n);
}

static void send_chunk(uint32_t index, const uint8_t* data, uint32_t len) {
    uint8_t frame[GRR_XFER_CHUNK_FRAME_OVERHEAD + GRR_XFER_CHUNK_SIZE];
    size_t n = grr_xfer_build_chunk_frame(index, data, len, frame);
    feed(frame, n);
}

static void send_abort(void) {
    uint8_t frame[GRR_XFER_ABORT_FRAME_SIZE];
    size_t n = grr_xfer_build_abort_frame(frame);
    feed(frame, n);
}

/* header + accept, asserted READY */
static void start_transfer(const grr_xfer_header* h) {
    int before = s_response_count;
    send_header(h);
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_HEADER_OK,
          "valid header parks in HEADER_OK");
    CHECK(grr_xfer_accept(&s_engine) == GRR_XFER_OK, "accept succeeds");
    (void)before;
    check_response(s_response_count - 1, GRR_XFER_OK, GRR_XFER_ACK_HEADER,
                   "READY emitted after accept");
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_RECEIVING,
          "state RECEIVING after READY");
}

static void send_all_chunks(void) {
    send_chunk(0, s_image, 256);
    send_chunk(1, s_image + 256, 256);
    send_chunk(2, s_image + 512, 88);
}

/* --- Scenarios ------------------------------------------------------ */

static void test_valid_transfer(void) {
    printf("test_valid_transfer\n");
    harness_reset();

    grr_xfer_header h;
    make_header(&h);
    start_transfer(&h);

    int responses_before = s_response_count;
    send_all_chunks();

    CHECK(s_begin_count == 1, "storage begin called exactly once");
    CHECK(s_write_count == 3, "three chunks written");
    CHECK(s_flush_count == 1, "flush called before readback");
    CHECK(s_validate_count == 1, "structural validation ran");
    CHECK(s_commit_count == 1, "image committed after verification");
    CHECK(s_committed == 1, "mock storage marks image valid");
    CHECK(s_written == IMAGE_LEN, "all image bytes stored");
    CHECK(memcmp(s_flash, s_image, IMAGE_LEN) == 0, "stored bytes match the image");
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_DONE, "engine DONE");

    check_response(responses_before + 0, GRR_XFER_OK, GRR_XFER_ACK_CHUNK,
                   "chunk 0 acknowledged");
    check_response(responses_before + 1, GRR_XFER_OK, GRR_XFER_ACK_CHUNK,
                   "chunk 1 acknowledged");
    check_response(responses_before + 2, GRR_XFER_OK, GRR_XFER_ACK_CHUNK,
                   "chunk 2 acknowledged");
    check_response(responses_before + 3, GRR_XFER_VERIFIED, GRR_XFER_ACK_NONE,
                   "final VERIFIED after hash + validation");
    CHECK(s_response_count == responses_before + 4, "exactly 4 responses total");
}

static void test_reject_header(void) {
    printf("test_reject_header\n");

    grr_xfer_header h;

    /* Wrong protocol version */
    harness_reset();
    make_header(&h);
    h.protocol_version = 2;
    send_header(&h);
    check_response(0, GRR_XFER_ERR_BAD_VERSION, GRR_XFER_ACK_HEADER,
                   "protocol version 2 rejected");
    CHECK(s_begin_count == 0, "flash untouched after bad version");
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_IDLE,
          "engine back to IDLE");

    /* Unsupported image format version */
    harness_reset();
    make_header(&h);
    h.image_format_version = 99;
    send_header(&h);
    check_response(0, GRR_XFER_ERR_FORMAT, GRR_XFER_ACK_HEADER,
                   "image format version 99 rejected");
    CHECK(s_begin_count == 0, "flash untouched after bad format");

    /* Zero-length image */
    harness_reset();
    make_header(&h);
    h.image_length = 0;
    send_header(&h);
    check_response(0, GRR_XFER_ERR_BAD_LENGTH, GRR_XFER_ACK_HEADER,
                   "zero-length image rejected");

    /* Not word-aligned (inconsistent with the .grr.o format) */
    harness_reset();
    make_header(&h);
    h.image_length = 50;
    send_header(&h);
    check_response(0, GRR_XFER_ERR_BAD_LENGTH, GRR_XFER_ACK_HEADER,
                   "non-word-aligned length rejected");

    /* Below the 3-word minimum header */
    harness_reset();
    make_header(&h);
    h.image_length = 8;
    send_header(&h);
    check_response(0, GRR_XFER_ERR_BAD_LENGTH, GRR_XFER_ACK_HEADER,
                   "length below 12 bytes rejected");

    /* Larger than the storage capacity */
    harness_reset();
    make_header(&h);
    h.image_length = MOCK_CAPACITY + 4;
    send_header(&h);
    check_response(0, GRR_XFER_ERR_BAD_LENGTH, GRR_XFER_ACK_HEADER,
                   "oversized image rejected against capacity");
    CHECK(s_begin_count == 0, "flash untouched after oversized length");

    /* Wrong chunk size (framing mismatch) */
    harness_reset();
    make_header(&h);
    h.chunk_size = 128;
    send_header(&h);
    check_response(0, GRR_XFER_ERR_BAD_LENGTH, GRR_XFER_ACK_HEADER,
                   "mismatched chunk size rejected");

    /* Corrupted header payload (CRC) */
    harness_reset();
    make_header(&h);
    uint8_t frame[GRR_XFER_HEADER_FRAME_SIZE];
    size_t n = grr_xfer_build_header_frame(&h, frame);
    frame[10] ^= 0x40; /* flip a bit inside image_length */
    feed(frame, n);
    check_response(0, GRR_XFER_ERR_FRAME, GRR_XFER_ACK_HEADER,
                   "header CRC mismatch rejected");
    CHECK(s_begin_count == 0, "flash untouched after corrupt header");

    /* Garbage in IDLE is dropped silently: a CHUNK frame never starts
     * anything and gets no response before a header. */
    harness_reset();
    send_chunk(0, s_image, 256);
    CHECK(s_response_count == 0, "stray chunk in IDLE ignored silently");
    CHECK(s_begin_count == 0, "no begin from stray chunk");
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_IDLE, "still IDLE");
}

static void test_chunk_sequence(void) {
    printf("test_chunk_sequence\n");

    grr_xfer_header h;
    make_header(&h);

    /* Out-of-order first chunk */
    harness_reset();
    start_transfer(&h);
    send_chunk(1, s_image + 256, 256);
    check_response(1, GRR_XFER_ERR_SEQ, GRR_XFER_ACK_CHUNK, "chunk 1 first rejected");
    CHECK(last_aux() == 0, "device reports expected index 0");
    CHECK(s_write_count == 0, "nothing written for the out-of-order chunk");

    /* Recover: resend in order, full transfer completes */
    send_all_chunks();
    CHECK(s_committed == 1, "transfer recovers and completes after SEQ error");
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_DONE, "DONE after recovery");

    /* Duplicated chunk after a successful ack */
    harness_reset();
    start_transfer(&h);
    send_chunk(0, s_image, 256);
    check_response(1, GRR_XFER_OK, GRR_XFER_ACK_CHUNK, "chunk 0 acked");
    send_chunk(0, s_image, 256); /* duplicate */
    check_response(2, GRR_XFER_ERR_SEQ, GRR_XFER_ACK_CHUNK, "duplicate chunk rejected");
    CHECK(last_aux() == 1, "device wants index 1 next");
    send_chunk(1, s_image + 256, 256);
    send_chunk(2, s_image + 512, 88);
    CHECK(s_committed == 1, "transfer completes after duplicate rejection");

    /* Wrong data_len for the expected chunk */
    harness_reset();
    start_transfer(&h);
    send_chunk(0, s_image, 100); /* first chunk must be 256 */
    check_response(1, GRR_XFER_ERR_SEQ, GRR_XFER_ACK_CHUNK, "wrong chunk length rejected");
    CHECK(s_write_count == 0, "wrong-length chunk not written");
}

static void test_corrupted_chunk(void) {
    printf("test_corrupted_chunk\n");

    grr_xfer_header h;
    make_header(&h);
    harness_reset();
    start_transfer(&h);

    /* Build a chunk, then flip a data byte without fixing the CRC. */
    uint8_t frame[GRR_XFER_CHUNK_FRAME_OVERHEAD + GRR_XFER_CHUNK_SIZE];
    size_t n = grr_xfer_build_chunk_frame(0, s_image, 256, frame);
    frame[13 + 10] ^= 0xFF; /* corrupt payload */
    feed(frame, n);

    check_response(1, GRR_XFER_ERR_CRC, GRR_XFER_ACK_CHUNK, "corrupted chunk rejected");
    CHECK(last_aux() == 0, "CRC error reports the corrupted index");
    CHECK(s_write_count == 0, "corrupted chunk never reached storage");
    CHECK(s_begin_count == 1, "storage still prepared");

    /* Host retries the same chunk with correct bytes. */
    send_all_chunks();
    CHECK(s_committed == 1, "retry after CRC error completes the transfer");
    CHECK(memcmp(s_flash, s_image, IMAGE_LEN) == 0, "stored image intact after retry");
}

static void test_final_hash_mismatch(void) {
    printf("test_final_hash_mismatch\n");

    grr_xfer_header h;
    make_header(&h);
    h.sha256[0] ^= 0x01; /* host hashed different bytes */

    harness_reset();
    start_transfer(&h);
    send_all_chunks();

    check_response(s_response_count - 1, GRR_XFER_ERR_HASH, GRR_XFER_ACK_NONE,
                   "final SHA-256 mismatch reported");
    CHECK(s_commit_count == 0, "image NOT marked valid after hash mismatch");
    CHECK(s_committed == 0, "storage stays invalid");
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_IDLE,
          "engine recoverable (IDLE) after hash mismatch");

    /* Recovery: a correct transfer right afterwards succeeds. */
    grr_xfer_header good;
    make_header(&good);
    send_header(&good);
    CHECK(grr_xfer_accept(&s_engine) == GRR_XFER_OK, "restarted transfer accepted");
    send_all_chunks();
    CHECK(s_committed == 1, "device recovers and installs a good image");
}

static void test_invalid_binary_matching_hash(void) {
    printf("test_invalid_binary_matching_hash\n");

    grr_xfer_header h;
    make_header(&h);

    harness_reset();
    s_validate_status = GRR_XFER_ERR_STRUCTURE; /* structurally invalid image */
    start_transfer(&h);
    send_all_chunks();

    check_response(s_response_count - 1, GRR_XFER_ERR_STRUCTURE, GRR_XFER_ACK_NONE,
                   "structure error reported despite matching hash");
    CHECK(s_validate_count == 1, "validation attempted");
    CHECK(s_commit_count == 0, "invalid image never marked valid");

    /* Same for insufficient VM memory reported by validation. */
    harness_reset();
    s_validate_status = GRR_XFER_ERR_VM_MEMORY;
    start_transfer(&h);
    send_all_chunks();
    check_response(s_response_count - 1, GRR_XFER_ERR_VM_MEMORY, GRR_XFER_ACK_NONE,
                   "VM memory error propagated");
    CHECK(s_commit_count == 0, "image not committed on OOM");
}

static void test_flash_write_failure(void) {
    printf("test_flash_write_failure\n");

    grr_xfer_header h;
    make_header(&h);
    harness_reset();
    start_transfer(&h);

    s_fail_write = 1;
    send_chunk(0, s_image, 256);

    check_response(1, GRR_XFER_ERR_FLASH, GRR_XFER_ACK_CHUNK, "flash write failure reported");
    CHECK(s_commit_count == 0, "nothing committed after flash failure");
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_IDLE, "engine recoverable");

    /* begin() failing (erase failure) */
    harness_reset();
    s_fail_begin = 1;
    send_header(&h);
    CHECK(grr_xfer_accept(&s_engine) == GRR_XFER_ERR_FLASH, "accept fails on erase failure");
    check_response(0, GRR_XFER_ERR_FLASH, GRR_XFER_ACK_HEADER, "erase failure reported");
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_IDLE, "back to IDLE");

    /* commit() failing */
    harness_reset();
    s_fail_commit = 1;
    start_transfer(&h);
    send_all_chunks();
    check_response(s_response_count - 1, GRR_XFER_ERR_FLASH, GRR_XFER_ACK_NONE,
                   "commit failure reported");
    CHECK(s_committed == 0, "image not valid when commit failed");
}

static void test_interrupted_and_restarted(void) {
    printf("test_interrupted_and_restarted\n");

    grr_xfer_header h;
    make_header(&h);

    /* Interrupted after one chunk, then the host restarts. */
    harness_reset();
    start_transfer(&h);
    send_chunk(0, s_image, 256);
    CHECK(s_write_count == 1, "partial image written before interruption");

    /* ... host dies here; a new host sends a fresh header ... */
    send_header(&h);
    check_response(2, GRR_XFER_OK, GRR_XFER_ACK_HEADER, "restart re-accepted (re-erase)");
    CHECK(s_begin_count == 2, "storage prepared again on restart");
    CHECK(s_written == 0, "write position reset by restart");

    send_all_chunks();
    CHECK(s_committed == 1, "restarted transfer completes");
    CHECK(memcmp(s_flash, s_image, IMAGE_LEN) == 0, "image correct after restart");

    /* Abort mid-transfer */
    harness_reset();
    start_transfer(&h);
    send_chunk(0, s_image, 256);
    send_abort();
    CHECK(s_abort_count == 1, "abort callback invoked");
    check_response(2, GRR_XFER_ERR_CANCELLED, GRR_XFER_ACK_ABORT, "cancellation acknowledged");
    CHECK(s_committed == 0, "aborted image never valid");
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_IDLE, "IDLE after abort");

    /* And the device still accepts a full transfer afterwards. */
    send_header(&h);
    CHECK(grr_xfer_accept(&s_engine) == GRR_XFER_OK, "post-abort transfer accepted");
    send_all_chunks();
    CHECK(s_committed == 1, "device fully recovered after abort");
}

static void test_resync_and_done_state(void) {
    printf("test_resync_and_done_state\n");

    grr_xfer_header h;
    make_header(&h);
    harness_reset();

    /* Noise, then a valid header: the engine must resynchronize. */
    static const uint8_t noise[] = { 0x01, 0x02, 0x03, 0xAA, 0xBB, 0x02, 0x00, 0x00, 0x00 };
    feed(noise, sizeof(noise));
    send_header(&h);
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_HEADER_OK,
          "engine resyncs on header after noise");

    /* Partial header frame does not (yet) park the transfer. */
    harness_reset();
    uint8_t frame[GRR_XFER_HEADER_FRAME_SIZE];
    size_t n = grr_xfer_build_header_frame(&h, frame);
    feed(frame, 7); /* half a header */
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_IDLE,
          "partial frame stays IDLE");
    feed(frame + 7, n - 7);
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_HEADER_OK,
          "complete header parks the transfer");

    /* After DONE, stray bytes are ignored until the platform resets. */
    harness_reset();
    start_transfer(&h);
    send_all_chunks();
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_DONE, "DONE");
    int before = s_response_count;
    send_chunk(0, s_image, 256);
    CHECK(s_response_count == before, "bytes after DONE ignored");
    grr_xfer_reset(&s_engine);
    CHECK(grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_IDLE, "reset returns IDLE");
    CHECK(!grr_xfer_busy(&s_engine), "not busy after reset");
}

static void test_busy_tracking(void) {
    printf("test_busy_tracking\n");

    grr_xfer_header h;
    make_header(&h);
    harness_reset();
    CHECK(!grr_xfer_busy(&s_engine), "idle engine not busy");

    uint8_t frame[GRR_XFER_HEADER_FRAME_SIZE];
    size_t n = grr_xfer_build_header_frame(&h, frame);
    feed(frame, 5); /* type + length, payload missing */
    CHECK(grr_xfer_busy(&s_engine), "partial frame is busy");
    feed(frame + 5, n - 5);
    CHECK(grr_xfer_busy(&s_engine), "parked header is busy");
    grr_xfer_accept(&s_engine);
    CHECK(grr_xfer_busy(&s_engine), "receiving is busy");
    send_all_chunks();
    CHECK(grr_xfer_busy(&s_engine), "done is busy");
}

static void test_golden_frames(void) {
    printf("test_golden_frames\n");

    /* Golden bytes generated by the reference host uploader
     * (tools/upload_grr.py: struct.pack('<BBHII'...) + zlib.crc32).
     * This pins the C device and the Python host to one wire format. */
    grr_xfer_header h;
    memset(&h, 0, sizeof(h));
    h.protocol_version = 1;
    h.image_format_version = 1;
    h.image_length = 600;
    h.chunk_size = 256;
    for (int i = 0; i < 32; i++) h.sha256[i] = (uint8_t)i;

    static const uint8_t golden_header[53] = {
        0x01, 0x30, 0x00, 0x00, 0x00,
        0x01, 0x00,
        0x01, 0x00,
        0x58, 0x02, 0x00, 0x00,
        0x00, 0x01, 0x00, 0x00,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x56, 0x70, 0x7f, 0x39
    };

    uint8_t frame[GRR_XFER_HEADER_FRAME_SIZE];
    size_t n = grr_xfer_build_header_frame(&h, frame);
    CHECK(n == sizeof(golden_header)
          && memcmp(frame, golden_header, sizeof(golden_header)) == 0,
          "HEADER frame bytes match the Python reference uploader");

    static const uint8_t golden_chunk[19] = {
        0x02, 0x0e, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0xaa, 0xbb,
        0x1a, 0xf7, 0x19, 0xa6
    };

    uint8_t cframe[GRR_XFER_CHUNK_FRAME_OVERHEAD + 2];
    const uint8_t data[2] = { 0xAA, 0xBB };
    n = grr_xfer_build_chunk_frame(0, data, 2, cframe);
    CHECK(n == sizeof(golden_chunk)
          && memcmp(cframe, golden_chunk, sizeof(golden_chunk)) == 0,
          "CHUNK frame bytes match the Python reference uploader");
}

int main(void) {
    make_image();

    test_golden_frames();
    test_valid_transfer();
    test_reject_header();
    test_chunk_sequence();
    test_corrupted_chunk();
    test_final_hash_mismatch();
    test_invalid_binary_matching_hash();
    test_flash_write_failure();
    test_interrupted_and_restarted();
    test_resync_and_done_state();
    test_busy_tracking();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
