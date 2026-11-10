/* Verifies the shared SHA-256 (grr_sha256) and CRC32 (grr_crc32) used by
 * the runtime guest-program upload path.
 *
 * Reference digests were produced with Python hashlib / zlib over the
 * exact bytes of a real assembler artifact (guest_programs/
 * sample_fib_iter.grr.o), so this pins "device hashes the same bytes the
 * host hashed" for a real .grr.o image.
 */
#include <stdio.h>
#include <string.h>
#include "grrvm/program_transfer.h"
#include "grrvm/sha256.h"

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); g_pass++; } \
    else      { printf("  [FAIL] %s  (line %d)\n", msg, __LINE__); g_fail++; } \
} while (0)

static void to_hex(const uint8_t* digest, char out[65]) {
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        out[i * 2]     = hex[(digest[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex[digest[i] & 0x0F];
    }
    out[64] = '\0';
}

static int sha_equals(const void* data, size_t len, const char* expected_hex) {
    uint8_t digest[GRR_SHA256_DIGEST_SIZE];
    char hex[65];
    grr_sha256(data, len, digest);
    to_hex(digest, hex);
    return strcmp(hex, expected_hex) == 0;
}

/* Real assembler output: guest_programs/sample_fib_iter.grr.o (148 bytes) */
static const uint8_t fib_image[] = {
    0x03, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x51, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0x22, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x33, 0x00, 0x00, 0x00,
    0x06, 0x00, 0x00, 0x00, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00,
    0x33, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x34, 0x00, 0x00, 0x00, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x34, 0x00, 0x00, 0x00
};

static void test_sha256_vectors(void) {
    printf("test_sha256_vectors\n");

    /* FIPS 180-4 / RFC 6234 published vectors */
    CHECK(sha_equals("", 0,
        "e3b0c44298fc1c149afbf4c8996fb924"
        "27ae41e4649b934ca495991b7852b855"),
        "SHA-256 of empty string");

    CHECK(sha_equals("abc", 3,
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad"),
        "SHA-256 of \"abc\"");

    CHECK(sha_equals("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
        "248d6a61d20638b8e5c026930c3e6039"
        "a33ce45964ff2167f6ecedd419db06c1"),
        "SHA-256 of the 448-bit message");
}

static void test_sha256_incremental(void) {
    printf("test_sha256_incremental\n");

    static const char* msg =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    grr_sha256_ctx ctx;
    uint8_t digest[GRR_SHA256_DIGEST_SIZE];
    char hex[65];

    /* Same message fed in 7-byte pieces must match the one-shot digest:
     * exercises buffer carry-over across block boundaries. */
    grr_sha256_init(&ctx);
    for (size_t i = 0; i < strlen(msg); i += 7) {
        grr_sha256_update(&ctx, msg + i, strlen(msg) - i < 7 ? strlen(msg) - i : 7);
    }
    grr_sha256_final(&ctx, digest);
    to_hex(digest, hex);
    CHECK(strcmp(hex,
        "248d6a61d20638b8e5c026930c3e6039"
        "a33ce45964ff2167f6ecedd419db06c1") == 0,
        "chunked update matches one-shot digest");

    /* byte-at-a-time across the whole fib image */
    grr_sha256_init(&ctx);
    for (size_t i = 0; i < sizeof(fib_image); i++) {
        grr_sha256_update(&ctx, &fib_image[i], 1);
    }
    grr_sha256_final(&ctx, digest);
    to_hex(digest, hex);
    CHECK(strcmp(hex,
        "5a16e7359ff3fe5ebc00d15c6a9a4eb"
        "32ca90dac5e8196aecd20048c9cd0c7f5") == 0,
        "byte-wise streaming over the real .grr.o image");
}

static void test_sha256_real_image(void) {
    printf("test_sha256_real_image\n");

    CHECK(sizeof(fib_image) == 148, "fib image size is 148 bytes");

    /* Python: hashlib.sha256(open('guest_programs/sample_fib_iter.grr.o','rb').read()) */
    CHECK(sha_equals(fib_image, sizeof(fib_image),
        "5a16e7359ff3fe5ebc00d15c6a9a4eb"
        "32ca90dac5e8196aecd20048c9cd0c7f5"),
        "SHA-256 of the exact assembler output matches Python hashlib");
}

static void test_crc32(void) {
    printf("test_crc32\n");

    /* Python: zlib.crc32(b'') == 0, zlib.crc32(b'123456789') == 0xCBF43926 */
    CHECK(grr_crc32("", 0) == 0x00000000u, "CRC32 of empty input");
    CHECK(grr_crc32("123456789", 9) == 0xCBF43926u, "CRC32 check value of \"123456789\"");
    CHECK(grr_crc32(fib_image, sizeof(fib_image)) == 0x3831421Eu,
          "CRC32 of the real .grr.o image matches zlib.crc32");
}

int main(void) {
    test_sha256_vectors();
    test_sha256_incremental();
    test_sha256_real_image();
    test_crc32();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
