#ifndef GRRVM_PROGRAM_TRANSFER_H
#define GRRVM_PROGRAM_TRANSFER_H

/* Transport-neutral guest-program upload protocol (v1).
 *
 * Receives a .grr.o image over any byte stream, stores it through a
 * storage-ops vtable, verifies the stored bytes against the expected
 * SHA-256 digest and only then lets the platform mark the image valid.
 *
 * Wire format (all multi-byte integers little-endian, no C structs on
 * the wire):
 *
 *   frame            := type:u8  length:u32le  payload[length]
 *   HEADER  (0x01)   payload = version:u8, flags:u8, format_version:u16,
 *                    image_length:u32, chunk_size:u32, sha256[32],
 *                    crc32:u32 over the previous 44 bytes      (48 bytes)
 *   CHUNK   (0x02)   payload = index:u32, data_len:u32, data[data_len],
 *                    crc32:u32 over (index||data_len||data)
 *   ABORT   (0x03)   payload empty
 *   RESPONSE(0x80)   payload = status:u8, ack_type:u8, reserved:u16,
 *                    aux:u32                                        (8 bytes)
 *
 * Sequence: host sends HEADER, device answers READY or an error; host
 * sends numbered CHUNKs, each acknowledged with OK; when the declared
 * length is reached the device flushes, re-reads the stored image,
 * compares SHA-256, structurally validates it, marks it valid and emits
 * VERIFIED (or an error - a failed upload leaves no valid image behind).
 * A HEADER frame may re-appear at any time and restarts the transfer.
 *
 * SHA-256 gives integrity, not authentication: anyone able to talk to
 * the device can install a validly hashed image.
 *
 * This module is platform-independent C: no filesystem, no flash, no
 * serial APIs. The platform binds storage + transport through
 * grr_xfer_ops and drives it with grr_xfer_feed()/grr_xfer_accept().
 */

#include <stddef.h>
#include <stdint.h>
#include "grrvm/sha256.h"

/* Protocol version implemented by this module. */
#define GRR_XFER_PROTOCOL_VERSION 1u
/* Guest-image format version stamped in the transfer header. */
#define GRR_XFER_IMAGE_FORMAT_VERSION 1u
/* Payload bytes per chunk: matches the RP2040 flash page size so each
 * acknowledged chunk maps onto one flash page program operation. */
#define GRR_XFER_CHUNK_SIZE 256u
/* Fixed HEADER payload size. */
#define GRR_XFER_HEADER_PAYLOAD_SIZE 48u
/* Largest payload this engine ever buffers (CHUNK frame). */
#define GRR_XFER_MAX_FRAME_PAYLOAD (8u + GRR_XFER_CHUNK_SIZE + 4u)

/* Host -> device frame types */
#define GRR_XFER_FRAME_HEADER 0x01u
#define GRR_XFER_FRAME_CHUNK  0x02u
#define GRR_XFER_FRAME_ABORT  0x03u
/* Device -> host frame type */
#define GRR_XFER_FRAME_RESP   0x80u

/* ack_type field of RESPONSE frames */
#define GRR_XFER_ACK_NONE     0x00u
#define GRR_XFER_ACK_HEADER   0x01u
#define GRR_XFER_ACK_CHUNK    0x02u
#define GRR_XFER_ACK_ABORT    0x03u

/* RESPONSE status codes */
#define GRR_XFER_OK                 0x00u  /* READY / chunk accepted */
#define GRR_XFER_VERIFIED           0x01u  /* image verified and installed */
#define GRR_XFER_ERR_BAD_VERSION    0x02u  /* unsupported protocol version */
#define GRR_XFER_ERR_BAD_LENGTH     0x03u  /* invalid image length / chunk size */
#define GRR_XFER_ERR_FORMAT         0x04u  /* unsupported image format version */
#define GRR_XFER_ERR_SEQ            0x05u  /* wrong chunk index (aux = expected) */
#define GRR_XFER_ERR_CRC            0x06u  /* chunk checksum mismatch (aux = index) */
#define GRR_XFER_ERR_FLASH          0x07u  /* storage write/erase failed */
#define GRR_XFER_ERR_HASH           0x08u  /* final SHA-256 mismatch */
#define GRR_XFER_ERR_STRUCTURE      0x09u  /* image is not a valid .grr.o */
#define GRR_XFER_ERR_VM_MEMORY      0x0Au  /* not enough VM memory for image */
#define GRR_XFER_ERR_BUSY           0x0Bu  /* frame not valid in this state */
#define GRR_XFER_ERR_FRAME          0x0Cu  /* malformed frame / bad header CRC */
#define GRR_XFER_ERR_CANCELLED      0x0Du  /* upload aborted by host */

/* Fixed sizes of the encoded frames */
#define GRR_XFER_HEADER_FRAME_SIZE (5u + GRR_XFER_HEADER_PAYLOAD_SIZE)
#define GRR_XFER_ABORT_FRAME_SIZE  5u
#define GRR_XFER_RESP_FRAME_SIZE   13u
#define GRR_XFER_CHUNK_FRAME_OVERHEAD 17u /* 5 frame + 8 index/len + 4 crc */

/* Parsed HEADER payload */
typedef struct grr_xfer_header {
    uint8_t  protocol_version;
    uint16_t image_format_version;
    uint32_t image_length;   /* bytes; > 0, multiple of 4, >= 12 */
    uint32_t chunk_size;     /* must equal GRR_XFER_CHUNK_SIZE */
    uint8_t  sha256[GRR_SHA256_DIGEST_SIZE];
} grr_xfer_header;

/* Engine states */
typedef enum grr_xfer_state {
    GRR_XFER_STATE_IDLE = 0,    /* no transfer; garbage is dropped */
    GRR_XFER_STATE_HEADER_OK,   /* valid header, awaiting grr_xfer_accept */
    GRR_XFER_STATE_RECEIVING,   /* chunks are being accepted */
    GRR_XFER_STATE_DONE         /* image verified, validated and committed */
} grr_xfer_state;

/* Storage + transport backend supplied by the platform. All callbacks
 * are required. They must not block forever and must be safe to call
 * from the platform's transfer service loop. */
typedef struct grr_xfer_ops {
    void* ctx;
    /* Largest image the storage can hold, in bytes. */
    uint32_t (*capacity)(void* ctx);
    /* Invalidate the stored image and reset the write position. */
    int (*begin)(void* ctx, const grr_xfer_header* header);
    /* Store `len` bytes at byte offset `offset` from the image start. */
    int (*write)(void* ctx, uint32_t offset, const uint8_t* data, uint32_t len);
    /* Persist any buffered tail before the image is read back. */
    int (*flush)(void* ctx);
    /* Borrowed pointer to the stored bytes (valid until the next
     * begin()); length through *len_out. */
    const uint8_t* (*image)(void* ctx, uint32_t* len_out);
    /* Structural/VM validation. Returns 0 or one of the GRR_XFER_ERR_*
     * status codes to report to the host. */
    int (*validate)(void* ctx, const uint8_t* image, uint32_t len);
    /* Mark the verified image as valid (persist metadata). */
    int (*commit)(void* ctx, const grr_xfer_header* header);
    /* Invalidate the stored image (upload cancelled). */
    int (*abort)(void* ctx);
    /* Sink for response frames going back to the host. */
    void (*emit)(void* ctx, const uint8_t* frame, size_t length);
} grr_xfer_ops;

typedef struct grr_xfer_engine {
    const grr_xfer_ops* ops;
    grr_xfer_state state;
    grr_xfer_header header;
    uint32_t received;      /* image bytes accepted so far */
    uint32_t next_index;    /* expected chunk index */

    /* Frame reassembly */
    uint8_t  frame_phase;   /* 0 = type, 1 = length, 2 = payload */
    uint8_t  frame_type;
    uint32_t frame_len;
    uint32_t frame_pos;
    uint8_t  frame_buf[GRR_XFER_MAX_FRAME_PAYLOAD];
} grr_xfer_engine;

/* --- Byte-stream drive --------------------------------------------- */

/* Reset the engine to IDLE (drops any partial frame). */
void grr_xfer_init(grr_xfer_engine* engine, const grr_xfer_ops* ops);
void grr_xfer_reset(grr_xfer_engine* engine);

/* Feed one received byte. Storage callbacks are only invoked in
 * RECEIVING (write) or from grr_xfer_accept()/grr_xfer_finish();
 * feeding in IDLE never touches storage, so it is safe to call while a
 * guest program is running. */
void grr_xfer_feed(grr_xfer_engine* engine, uint8_t byte);

grr_xfer_state grr_xfer_state_of(const grr_xfer_engine* engine);

/* True while a frame is partially collected or a transfer is active.
 * Storage is only ever touched once a full HEADER has been accepted,
 * so a busy engine that is not past HEADER_OK is safe to feed even
 * while a guest program is executing. */
int grr_xfer_busy(const grr_xfer_engine* engine);

/* Advance a HEADER_OK engine: calls ops->begin() and emits READY.
 * Call only when it is safe to touch storage (e.g. the VM has stopped).
 * Returns 0 on success, or the status code emitted to the host. */
int grr_xfer_accept(grr_xfer_engine* engine);

/* --- Helpers (used by platforms, tests and host tools) ------------- */

uint32_t grr_crc32(const void* data, size_t length);
void grr_xfer_put_u32(uint8_t* p, uint32_t v);
uint32_t grr_xfer_get_u32(const uint8_t* p);
void grr_xfer_put_u16(uint8_t* p, uint16_t v);
uint16_t grr_xfer_get_u16(const uint8_t* p);

/* Frame builders (big enough for the caller-provided buffer). */
size_t grr_xfer_build_header_frame(const grr_xfer_header* header,
                                   uint8_t out[GRR_XFER_HEADER_FRAME_SIZE]);
size_t grr_xfer_build_chunk_frame(uint32_t index, const uint8_t* data,
                                  uint32_t data_len, uint8_t* out);
size_t grr_xfer_build_abort_frame(uint8_t out[GRR_XFER_ABORT_FRAME_SIZE]);

/* Parses a RESPONSE frame; returns 0 and fills the fields, or -1 if the
 * frame is not a well-formed RESPONSE. */
int grr_xfer_parse_response(const uint8_t* frame, size_t length,
                            uint8_t* status, uint8_t* ack_type, uint32_t* aux);

#endif /* GRRVM_PROGRAM_TRANSFER_H */
