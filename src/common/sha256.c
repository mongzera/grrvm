#include "grrvm/sha256.h"
#include <string.h>

/* Compact SHA-256 (FIPS 180-4). Self-contained, no dependencies, correct
 * for arbitrary input lengths and streaming (chunked) updates. */

static const uint32_t k_round_constants[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32u - (n))))

static void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;
    int i;

    // Prepare the message schedule (big-endian words)
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24)
             | ((uint32_t)block[i * 4 + 1] << 16)
             | ((uint32_t)block[i * 4 + 2] << 8)
             | ((uint32_t)block[i * 4 + 3]);
    }
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROTR32(w[i - 15], 7) ^ ROTR32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROTR32(w[i - 2], 17) ^ ROTR32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];

    for (i = 0; i < 64; i++) {
        uint32_t s1 = ROTR32(e, 6) ^ ROTR32(e, 11) ^ ROTR32(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        t1 = h + s1 + ch + k_round_constants[i] + w[i];
        uint32_t s0 = ROTR32(a, 2) ^ ROTR32(a, 13) ^ ROTR32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        t2 = s0 + maj;

        h = g; g = f; f = e;
        e = d + t1;
        d = c; c = b; b = a;
        a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void grr_sha256_init(grr_sha256_ctx* ctx) {
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;
    ctx->bit_count = 0;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void grr_sha256_update(grr_sha256_ctx* ctx, const void* data, size_t length) {
    const uint8_t* p = (const uint8_t*)data;
    size_t buffered = (size_t)((ctx->bit_count >> 3) & 0x3Fu);

    ctx->bit_count += (uint64_t)length * 8u;

    if (buffered > 0) {
        size_t fill = 64u - buffered;
        if (length < fill) {
            memcpy(ctx->buffer + buffered, p, length);
            return;
        }
        memcpy(ctx->buffer + buffered, p, fill);
        sha256_transform(ctx->state, ctx->buffer);
        p += fill;
        length -= fill;
    }

    while (length >= 64u) {
        sha256_transform(ctx->state, p);
        p += 64u;
        length -= 64u;
    }

    if (length > 0) {
        memcpy(ctx->buffer, p, length);
    }
}

void grr_sha256_final(grr_sha256_ctx* ctx, uint8_t digest[GRR_SHA256_DIGEST_SIZE]) {
    uint8_t padding[72];
    size_t buffered = (size_t)((ctx->bit_count >> 3) & 0x3Fu);
    uint64_t bit_count = ctx->bit_count;
    size_t pad_len;
    int i;

    // 0x80 byte, then zeros, then 64-bit big-endian bit length,
    // padded out to a whole number of 64-byte blocks.
    memset(padding, 0, sizeof(padding));
    padding[0] = 0x80u;
    pad_len = (buffered < 56u) ? (56u - buffered) : (120u - buffered);

    for (i = 0; i < 8; i++) {
        padding[pad_len + (size_t)i] = (uint8_t)((bit_count >> (56 - 8 * i)) & 0xFFu);
    }

    grr_sha256_update(ctx, padding, pad_len + 8u);

    for (i = 0; i < 8; i++) {
        digest[i * 4]      = (uint8_t)((ctx->state[i] >> 24) & 0xFFu);
        digest[i * 4 + 1]  = (uint8_t)((ctx->state[i] >> 16) & 0xFFu);
        digest[i * 4 + 2]  = (uint8_t)((ctx->state[i] >> 8) & 0xFFu);
        digest[i * 4 + 3]  = (uint8_t)(ctx->state[i] & 0xFFu);
    }
}

void grr_sha256(const void* data, size_t length, uint8_t digest[GRR_SHA256_DIGEST_SIZE]) {
    grr_sha256_ctx ctx;
    grr_sha256_init(&ctx);
    grr_sha256_update(&ctx, data, length);
    grr_sha256_final(&ctx, digest);
}
