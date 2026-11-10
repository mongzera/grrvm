#ifndef GRRVM_SHA256_H
#define GRRVM_SHA256_H

/* Compact SHA-256 (FIPS 180-4) used for whole-image integrity checks in
 * the runtime guest-program upload path.
 *
 * This is a plain software implementation with no external dependencies,
 * shared by every port (the Pico SDK's pico_sha256 library requires the
 * RP2350 SHA-256 hardware, which the RP2040 does not have).
 *
 * NOTE: SHA-256 provides an integrity comparison against transmission or
 * storage corruption. It does NOT authenticate the sender: anyone who can
 * talk to the upload port can install a validly-hashed image. */

#include <stddef.h>
#include <stdint.h>

#define GRR_SHA256_DIGEST_SIZE 32u

typedef struct grr_sha256_ctx {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t buffer[64];
} grr_sha256_ctx;

void grr_sha256_init(grr_sha256_ctx* ctx);
void grr_sha256_update(grr_sha256_ctx* ctx, const void* data, size_t length);
void grr_sha256_final(grr_sha256_ctx* ctx, uint8_t digest[GRR_SHA256_DIGEST_SIZE]);

/* One-shot convenience wrapper. */
void grr_sha256(const void* data, size_t length, uint8_t digest[GRR_SHA256_DIGEST_SIZE]);

#endif /* GRRVM_SHA256_H */
