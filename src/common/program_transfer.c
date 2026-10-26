#include "grrvm/program_transfer.h"
#include <string.h>

/* =========================================================================
 * Little-endian helpers
 * ========================================================================= */

void grr_xfer_put_u32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

uint32_t grr_xfer_get_u32(const uint8_t* p) {
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

void grr_xfer_put_u16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

uint16_t grr_xfer_get_u16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* =========================================================================
 * CRC32 (IEEE 802.3, reflected, polynomial 0xEDB88320)
 *
 * Nibble-table variant: 16 entries instead of 256, identical results to
 * the classic byte-table implementation (e.g. zlib.crc32).
 * Per-chunk CRC only detects corruption early - the final authority is
 * the whole-image SHA-256 comparison.
 * ========================================================================= */

static const uint32_t crc32_nibble_table[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu,
    0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
    0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu
};

uint32_t grr_crc32(const void* data, size_t length) {
    const uint8_t* p = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0; i < length; i++) {
        crc ^= (uint32_t)p[i];
        /* Two nibble steps advance exactly the 8 bits a byte-wise table
         * lookup would (equivalent to crc = (crc >> 8) ^ T256[...]). */
        crc = (crc >> 4) ^ crc32_nibble_table[crc & 0x0Fu];
        crc = (crc >> 4) ^ crc32_nibble_table[crc & 0x0Fu];
    }

    return ~crc;
}

/* =========================================================================
 * Engine basics
 * ========================================================================= */

#define PHASE_SIG     0u  /* IDLE: scanning for the HEADER signature */
#define PHASE_TYPE    1u  /* active transfer: frame type byte */
#define PHASE_LEN     2u  /* active transfer: u32le payload length */
#define PHASE_PAYLOAD 3u  /* collecting payload bytes */

/* HEADER frame start: type 0x01 followed by payload length 48 LE. */
static const uint8_t s_header_signature[5] = {
    GRR_XFER_FRAME_HEADER, 0x30, 0x00, 0x00, 0x00
};

void grr_xfer_init(grr_xfer_engine* engine, const grr_xfer_ops* ops) {
    memset(engine, 0, sizeof(*engine));
    engine->ops = ops;
    engine->state = GRR_XFER_STATE_IDLE;
    engine->frame_phase = PHASE_SIG;
}

void grr_xfer_reset(grr_xfer_engine* engine) {
    engine->state = GRR_XFER_STATE_IDLE;
    engine->frame_phase = PHASE_SIG;
    engine->frame_len = 0;
    engine->frame_pos = 0;
    engine->received = 0;
    engine->next_index = 0;
}

grr_xfer_state grr_xfer_state_of(const grr_xfer_engine* engine) {
    return engine->state;
}

int grr_xfer_busy(const grr_xfer_engine* engine) {
    return engine->state != GRR_XFER_STATE_IDLE
        || engine->frame_phase != PHASE_SIG
        || engine->frame_pos != 0;
}

static void emit_resp(grr_xfer_engine* engine, uint8_t status,
                      uint8_t ack_type, uint32_t aux) {
    uint8_t frame[GRR_XFER_RESP_FRAME_SIZE];

    frame[0] = GRR_XFER_FRAME_RESP;
    grr_xfer_put_u32(frame + 1, 8u);
    frame[5] = status;
    frame[6] = ack_type;
    frame[7] = 0;
    frame[8] = 0;
    grr_xfer_put_u32(frame + 9, aux);

    engine->ops->emit(engine->ops->ctx, frame, sizeof(frame));
}

/* =========================================================================
 * HEADER validation
 * ========================================================================= */

/* Returns GRR_XFER_OK or the status code to report. */
static int parse_header_payload(const grr_xfer_engine* engine,
                                const uint8_t* p, grr_xfer_header* out) {
    uint32_t crc_expect = grr_xfer_get_u32(p + 44);

    if (grr_crc32(p, 44) != crc_expect) {
        return GRR_XFER_ERR_FRAME;
    }

    out->protocol_version = p[0];
    out->image_format_version = grr_xfer_get_u16(p + 2);
    out->image_length = grr_xfer_get_u32(p + 4);
    out->chunk_size = grr_xfer_get_u32(p + 8);
    memcpy(out->sha256, p + 12, GRR_SHA256_DIGEST_SIZE);

    if (out->protocol_version != GRR_XFER_PROTOCOL_VERSION) {
        return GRR_XFER_ERR_BAD_VERSION;
    }

    if (out->image_format_version != GRR_XFER_IMAGE_FORMAT_VERSION) {
        return GRR_XFER_ERR_FORMAT;
    }

    /* chunk_size is part of the negotiated framing: reject mismatches
     * before any state is created. */
    if (out->chunk_size != GRR_XFER_CHUNK_SIZE) {
        return GRR_XFER_ERR_BAD_LENGTH;
    }

    /* The .grr.o image is a stream of 32-bit words: zero-length, shorter
     * than the 3-word header or not word-aligned is inconsistent with the
     * binary format. */
    if (out->image_length < 3u * sizeof(uint32_t)
        || (out->image_length % sizeof(uint32_t)) != 0) {
        return GRR_XFER_ERR_BAD_LENGTH;
    }

    if (out->image_length > engine->ops->capacity(engine->ops->ctx)) {
        return GRR_XFER_ERR_BAD_LENGTH;
    }

    return GRR_XFER_OK;
}

/* begin + READY. Shared by grr_xfer_accept() and HEADER restarts. */
static int accept_begin(grr_xfer_engine* engine) {
    if (engine->ops->begin(engine->ops->ctx, &engine->header) != 0) {
        emit_resp(engine, GRR_XFER_ERR_FLASH, GRR_XFER_ACK_HEADER, 0);
        engine->state = GRR_XFER_STATE_IDLE;
        return GRR_XFER_ERR_FLASH;
    }

    engine->received = 0;
    engine->next_index = 0;
    emit_resp(engine, GRR_XFER_OK, GRR_XFER_ACK_HEADER, GRR_XFER_CHUNK_SIZE);
    engine->state = GRR_XFER_STATE_RECEIVING;
    return GRR_XFER_OK;
}

int grr_xfer_accept(grr_xfer_engine* engine) {
    if (engine->state != GRR_XFER_STATE_HEADER_OK) {
        return GRR_XFER_ERR_BUSY;
    }
    return accept_begin(engine);
}

static void handle_header_frame(grr_xfer_engine* engine) {
    grr_xfer_header header;
    int status = parse_header_payload(engine, engine->frame_buf, &header);

    if (status != GRR_XFER_OK) {
        /* Rejected before any flash/erase operation: the previously
         * stored image (if any) is untouched. */
        emit_resp(engine, (uint8_t)status, GRR_XFER_ACK_HEADER, 0);
        engine->state = GRR_XFER_STATE_IDLE;
        return;
    }

    engine->header = header;
    engine->received = 0;
    engine->next_index = 0;

    if (engine->state == GRR_XFER_STATE_RECEIVING) {
        /* Host restarted the transfer: erase again and go. */
        accept_begin(engine);
        return;
    }

    /* IDLE or HEADER_OK: park until the platform calls grr_xfer_accept()
     * at a moment when touching storage is safe. */
    engine->state = GRR_XFER_STATE_HEADER_OK;
}

/* =========================================================================
 * CHUNK handling and completion
 * ========================================================================= */

static void run_finish(grr_xfer_engine* engine) {
    const grr_xfer_ops* ops = engine->ops;
    uint8_t digest[GRR_SHA256_DIGEST_SIZE];
    const uint8_t* image;
    uint32_t image_len = 0;
    int status;

    if (ops->flush(ops->ctx) != 0) {
        emit_resp(engine, GRR_XFER_ERR_FLASH, GRR_XFER_ACK_NONE, 0);
        engine->state = GRR_XFER_STATE_IDLE;
        return;
    }

    image = ops->image(ops->ctx, &image_len);
    if (image == NULL || image_len != engine->header.image_length) {
        emit_resp(engine, GRR_XFER_ERR_FLASH, GRR_XFER_ACK_NONE, 0);
        engine->state = GRR_XFER_STATE_IDLE;
        return;
    }

    /* SHA-256 over the exact stored bytes, compared with the digest the
     * host hashed from the assembler output. */
    grr_sha256(image, image_len, digest);
    if (memcmp(digest, engine->header.sha256, GRR_SHA256_DIGEST_SIZE) != 0) {
        emit_resp(engine, GRR_XFER_ERR_HASH, GRR_XFER_ACK_NONE, 0);
        engine->state = GRR_XFER_STATE_IDLE;
        return;
    }

    /* A matching hash is not proof of a structurally valid image: the
     * platform must still validate the .grr.o structure / VM fit. */
    status = ops->validate(ops->ctx, image, image_len);
    if (status != 0) {
        emit_resp(engine, (uint8_t)(status < 0 ? GRR_XFER_ERR_STRUCTURE : status),
                  GRR_XFER_ACK_NONE, 0);
        engine->state = GRR_XFER_STATE_IDLE;
        return;
    }

    /* Only now - hash matched AND structure validated - is the image
     * marked valid in storage. */
    if (ops->commit(ops->ctx, &engine->header) != 0) {
        emit_resp(engine, GRR_XFER_ERR_FLASH, GRR_XFER_ACK_NONE, 0);
        engine->state = GRR_XFER_STATE_IDLE;
        return;
    }

    emit_resp(engine, GRR_XFER_VERIFIED, GRR_XFER_ACK_NONE, 0);
    engine->state = GRR_XFER_STATE_DONE;
}

static void handle_chunk_frame(grr_xfer_engine* engine) {
    const uint8_t* p = engine->frame_buf;
    uint32_t index = grr_xfer_get_u32(p);
    uint32_t data_len = grr_xfer_get_u32(p + 4);
    uint32_t remaining;
    uint32_t expected_len;
    uint32_t crc_expect;

    remaining = engine->header.image_length - engine->received;
    expected_len = (remaining < GRR_XFER_CHUNK_SIZE) ? remaining : GRR_XFER_CHUNK_SIZE;

    /* Reject malformed framing and sequence errors without touching
     * storage; aux tells the host which chunk to (re)send. */
    if (engine->frame_len < 12u
        || data_len == 0
        || data_len > GRR_XFER_CHUNK_SIZE
        || engine->frame_len != 12u + data_len
        || index != engine->next_index
        || data_len != expected_len) {
        emit_resp(engine, GRR_XFER_ERR_SEQ, GRR_XFER_ACK_CHUNK, engine->next_index);
        return;
    }

    crc_expect = grr_xfer_get_u32(p + 8 + data_len);
    if (grr_crc32(p, 8u + data_len) != crc_expect) {
        emit_resp(engine, GRR_XFER_ERR_CRC, GRR_XFER_ACK_CHUNK, index);
        return;
    }

    if (engine->ops->write(engine->ops->ctx, engine->received, p + 8, data_len) != 0) {
        emit_resp(engine, GRR_XFER_ERR_FLASH, GRR_XFER_ACK_CHUNK, index);
        engine->state = GRR_XFER_STATE_IDLE;
        return;
    }

    engine->received += data_len;
    engine->next_index++;

    emit_resp(engine, GRR_XFER_OK, GRR_XFER_ACK_CHUNK, index);

    if (engine->received == engine->header.image_length) {
        run_finish(engine);
    }
}

static void handle_abort_frame(grr_xfer_engine* engine) {
    engine->ops->abort(engine->ops->ctx);
    emit_resp(engine, GRR_XFER_ERR_CANCELLED, GRR_XFER_ACK_ABORT, 0);
    grr_xfer_reset(engine);
}

/* =========================================================================
 * Frame dispatch
 * ========================================================================= */

static void handle_frame(grr_xfer_engine* engine) {
    switch (engine->state) {
        case GRR_XFER_STATE_IDLE:
            /* Only a well-formed HEADER can start anything; every other
             * frame type was already dropped at the framing layer. */
            if (engine->frame_type == GRR_XFER_FRAME_HEADER) {
                handle_header_frame(engine);
            }
            break;

        case GRR_XFER_STATE_HEADER_OK:
            if (engine->frame_type == GRR_XFER_FRAME_HEADER) {
                /* A newer header replaces the parked one; the platform
                 * accepts whichever is current. */
                handle_header_frame(engine);
            } else {
                emit_resp(engine, GRR_XFER_ERR_BUSY, engine->frame_type, 0);
            }
            break;

        case GRR_XFER_STATE_RECEIVING:
            switch (engine->frame_type) {
                case GRR_XFER_FRAME_HEADER: handle_header_frame(engine); break;
                case GRR_XFER_FRAME_CHUNK:  handle_chunk_frame(engine);  break;
                case GRR_XFER_FRAME_ABORT:  handle_abort_frame(engine);  break;
                default: emit_resp(engine, GRR_XFER_ERR_BUSY, engine->frame_type, 0); break;
            }
            break;

        case GRR_XFER_STATE_DONE:
            /* Image already installed; the platform is about to reset
             * the engine. Late bytes are reported as busy. */
            emit_resp(engine, GRR_XFER_ERR_BUSY, engine->frame_type, 0);
            break;
    }
}

/* =========================================================================
 * Byte-stream feed
 * ========================================================================= */

/* Re-arms the collector after a frame was handled: back to the HEADER
 * signature scan when idle, back to strict framing otherwise. */
static void dispatch_frame(grr_xfer_engine* engine) {
    handle_frame(engine);

    engine->frame_pos = 0;
    engine->frame_len = 0;
    if (engine->state == GRR_XFER_STATE_IDLE) {
        engine->frame_phase = PHASE_SIG;
    } else {
        engine->frame_phase = PHASE_TYPE;
    }
}

void grr_xfer_feed(grr_xfer_engine* engine, uint8_t byte) {
    if (engine == NULL || engine->ops == NULL) {
        return;
    }

    /* A completed image ignores stray bytes until the platform resets. */
    if (engine->state == GRR_XFER_STATE_DONE) {
        return;
    }

    switch (engine->frame_phase) {
        case PHASE_SIG: {
            /* Idle: literal scan for the HEADER signature
             *   0x01 | 0x30 00 00 00 (type + payload length 48 LE).
             * Pure byte matching, so arbitrary noise can never wedge the
             * parser mid-frame; a false match still has to pass the
             * header CRC before anything happens. */
            if (byte == s_header_signature[engine->frame_pos]) {
                engine->frame_pos++;
                if (engine->frame_pos == sizeof(s_header_signature)) {
                    engine->frame_type = GRR_XFER_FRAME_HEADER;
                    engine->frame_len = GRR_XFER_HEADER_PAYLOAD_SIZE;
                    engine->frame_pos = 0;
                    engine->frame_phase = PHASE_PAYLOAD;
                }
            } else {
                engine->frame_pos = (byte == s_header_signature[0]) ? 1u : 0u;
            }
            break;
        }

        case PHASE_TYPE:
            engine->frame_type = byte;
            engine->frame_len = 0;
            engine->frame_pos = 0;
            engine->frame_phase = PHASE_LEN;
            break;

        case PHASE_LEN:
            engine->frame_len |= (uint32_t)byte << (8u * engine->frame_pos);
            engine->frame_pos++;

            if (engine->frame_pos == 4u) {
                engine->frame_pos = 0;

                if (engine->frame_len > GRR_XFER_MAX_FRAME_PAYLOAD) {
                    /* Impossible length: report and resynchronize on the
                     * next frame boundary. */
                    emit_resp(engine, GRR_XFER_ERR_FRAME, engine->frame_type, 0);
                    engine->frame_phase = PHASE_TYPE;
                } else if (engine->frame_len == 0u) {
                    /* Zero-payload frame (ABORT): dispatch immediately. */
                    dispatch_frame(engine);
                } else {
                    engine->frame_phase = PHASE_PAYLOAD;
                }
            }
            break;

        case PHASE_PAYLOAD:
        default:
            engine->frame_buf[engine->frame_pos++] = byte;
            if (engine->frame_pos == engine->frame_len) {
                dispatch_frame(engine);
            }
            break;
    }
}

/* =========================================================================
 * Frame builders / parser (shared with tests and host-side tools)
 * ========================================================================= */

size_t grr_xfer_build_header_frame(const grr_xfer_header* header,
                                   uint8_t out[GRR_XFER_HEADER_FRAME_SIZE]) {
    uint8_t* p = out + 5;

    out[0] = GRR_XFER_FRAME_HEADER;
    grr_xfer_put_u32(out + 1, GRR_XFER_HEADER_PAYLOAD_SIZE);

    p[0] = header->protocol_version;
    p[1] = 0; /* flags, reserved */
    grr_xfer_put_u16(p + 2, header->image_format_version);
    grr_xfer_put_u32(p + 4, header->image_length);
    grr_xfer_put_u32(p + 8, header->chunk_size);
    memcpy(p + 12, header->sha256, GRR_SHA256_DIGEST_SIZE);
    grr_xfer_put_u32(p + 44, grr_crc32(p, 44));

    return GRR_XFER_HEADER_FRAME_SIZE;
}

size_t grr_xfer_build_chunk_frame(uint32_t index, const uint8_t* data,
                                  uint32_t data_len, uint8_t* out) {
    out[0] = GRR_XFER_FRAME_CHUNK;
    grr_xfer_put_u32(out + 1, 8u + data_len + 4u);
    grr_xfer_put_u32(out + 5, index);
    grr_xfer_put_u32(out + 9, data_len);
    memcpy(out + 13, data, data_len);
    grr_xfer_put_u32(out + 13 + data_len, grr_crc32(out + 5, 8u + data_len));

    return GRR_XFER_CHUNK_FRAME_OVERHEAD + data_len;
}

size_t grr_xfer_build_abort_frame(uint8_t out[GRR_XFER_ABORT_FRAME_SIZE]) {
    out[0] = GRR_XFER_FRAME_ABORT;
    grr_xfer_put_u32(out + 1, 0);
    return GRR_XFER_ABORT_FRAME_SIZE;
}

int grr_xfer_parse_response(const uint8_t* frame, size_t length,
                            uint8_t* status, uint8_t* ack_type, uint32_t* aux) {
    if (frame == NULL || length != GRR_XFER_RESP_FRAME_SIZE
        || frame[0] != GRR_XFER_FRAME_RESP
        || grr_xfer_get_u32(frame + 1) != 8u) {
        return -1;
    }

    if (status != NULL) *status = frame[5];
    if (ack_type != NULL) *ack_type = frame[6];
    if (aux != NULL) *aux = grr_xfer_get_u32(frame + 9);
    return 0;
}
