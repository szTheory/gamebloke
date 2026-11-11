#include "sha256.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * Known answers are taken from the IETF RFC 6234 SHA-256 example and the
 * NIST CAVP SHA256ShortMsg.rsp / SHA256LongMsg.rsp files. NIST describes these
 * response files as informal implementation checks, not validation results.
 * Sources: https://www.rfc-editor.org/rfc/rfc6234.html and
 * https://csrc.nist.gov/projects/cryptographic-algorithm-validation-program/secure-hashing
 * The generated 65-byte range span and 192-byte synthetic image (byte i is
 * (i * 29 + 7) modulo 256) and final-byte mutation were independently checked
 * with Python hashlib and OpenSSL before their results were pinned here.
 */

#if SIZE_MAX > UINT64_MAX / 8u
#define HASH_TEST_COUNT 14u
#else
#define HASH_TEST_COUNT 13u
#endif

static unsigned int test_number;
static unsigned int test_failures;

static void test_case(bool passed, const char *description)
{
    test_number++;
    printf("%s %u - %s\n", passed ? "ok" : "not ok", test_number,
           description);
    if (!passed) {
        test_failures++;
    }
}

static unsigned int hex_digit(char value)
{
    if (value >= '0' && value <= '9') {
        return (unsigned int)(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return (unsigned int)(value - 'a') + 10u;
    }
    if (value >= 'A' && value <= 'F') {
        return (unsigned int)(value - 'A') + 10u;
    }
    return 16u;
}

static bool decode_hex(const char *hex, uint8_t *bytes, size_t capacity,
                       size_t *length_out)
{
    const size_t hex_length = strlen(hex);
    size_t index;

    if ((hex_length % 2u) != 0u || hex_length / 2u > capacity) {
        return false;
    }
    for (index = 0u; index < hex_length / 2u; ++index) {
        const unsigned int high = hex_digit(hex[index * 2u]);
        const unsigned int low = hex_digit(hex[index * 2u + 1u]);

        if (high > 15u || low > 15u) {
            return false;
        }
        bytes[index] = (uint8_t)((high << 4u) | low);
    }
    *length_out = hex_length / 2u;
    return true;
}

static bool matches_vector(const char *input_hex, const char *expected_hex)
{
    uint8_t input[163];
    char actual[GBL_SHA256_HEX_CAPACITY];
    size_t input_length = 0u;

    if (!decode_hex(input_hex, input, sizeof input, &input_length)) {
        return false;
    }
    return gbl_sha256_span(input, input_length, actual) &&
           strcmp(actual, expected_hex) == 0;
}

static int run_wrong_expected_digest_control(void)
{
    static const uint8_t input[] = { UINT8_C(0x61), UINT8_C(0x62),
                                     UINT8_C(0x63) };
    static const char wrong_expected[] =
        "aa7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    char actual[GBL_SHA256_HEX_CAPACITY];
    const bool accepted = gbl_sha256_span(input, sizeof input, actual) &&
                          strcmp(actual, wrong_expected) == 0;

    if (accepted) {
        fputs("not ok 1 - deliberately wrong independent digest was accepted\n",
              stderr);
        return 1;
    }
    puts("1..1");
    puts("ok 1 - deliberately wrong independent digest is rejected");
    return 0;
}

int main(int argc, char **argv)
{
    static const char multi_block_input[] =
        "451101250ec6f26652249d59dc974b7361d571a8101cdfd36aba3b5854d3ae086"
        "b5fdd4597721b66e3c0dc5d8c606d9657d0e323283a5217d1f53f2f284f57b85"
        "c8a61ac8924711f895c5ed90ef17745ed2d728abd22a5f7a13479a462d71b56c"
        "19a74a40b655c58edfe0a188ad2cf46cbf30524f65d423c837dd1ff2bf462ac"
        "4198007345bb44dbb7b1c861298cdf61982a833afc728fae1eda2f87aa2c9480"
        "858bec";
    static const char multi_block_digest[] =
        "3c593aa539fdcdae516cdf2f15000f6634185c88f505b39775fb9ab137a10aa2";
    char output[GBL_SHA256_HEX_CAPACITY];
    char output_before[sizeof output];
    uint8_t image[192];
    uint8_t overlap[129];
    uint8_t overlap_before[sizeof overlap];
    uint8_t vector_bytes[163];
    size_t vector_length = 0u;
    size_t index;

    if (argc == 2 && strcmp(argv[1], "--wrong-independent-digest") == 0) {
        return run_wrong_expected_digest_control();
    }
    if (argc != 1) {
        fputs("usage: hash_test [--wrong-independent-digest]\n", stderr);
        return 2;
    }

    printf("1..%u\n", HASH_TEST_COUNT);

    test_case(gbl_sha256_span(NULL, 0u, output) &&
                  strcmp(output,
                         "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") ==
                      0,
              "hashes the empty span from the NIST CAVP vector");
    test_case(matches_vector(
                  "616263",
                  "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
              "hashes abc from the RFC 6234 SHA-256 example");
    test_case(matches_vector(
                  "3ebfb06db8c38d5ba037f1363e118550aad94606e26835a01af05078533cc25f2f39573c04b632f62f68c294ab31f2a3e2a1a0d8c2be51",
                  "6595a2ef537a69ba8583dfbf7f5bec0ab1f93ce4c8ee1916eff44a93af5749c4"),
              "hashes the NIST CAVP 55-byte padding-boundary vector");
    test_case(matches_vector(
                  "2d52447d1244d2ebc28650e7b05654bad35b3a68eedc7f8515306b496d75f3e73385dd1b002625024b81a02f2fd6dffb6e6d561cb7d0bd7a",
                  "cfb88d6faf2de3a69d36195acec2e255e2af2b7d933997f348e09f6ce5758360"),
              "hashes the NIST CAVP 56-byte padding-boundary vector");
    test_case(matches_vector(
                  "e2f76e97606a872e317439f1a03fcd92e632e5bd4e7cbc4e97f1afc19a16fde92d77cbe546416b51640cddb92af996534dfd81edb17c4424cf1ac4d75aceeb",
                  "18041bd4665083001fba8c5411d2d748e8abbfdcdfd9218cb02b68a78e7d4c23"),
              "hashes the NIST CAVP 63-byte padding-boundary vector");
    test_case(matches_vector(
                  "5a86b737eaea8ee976a0a24da63e7ed7eefad18a101c1211e2b3650c5187c2a8a650547208251f6d4237e661c7bf4c77f335390394c37fa1a9f9be836ac28509",
                  "42e61e174fbb3897d6dd6cef3dd2802fe67b331953b06114a65c772859dfc1aa"),
              "hashes the NIST CAVP 64-byte full-block vector");

    for (index = 0u; index < 65u; ++index) {
        vector_bytes[index] = (uint8_t)index;
    }
    test_case(gbl_sha256_span(vector_bytes, 65u, output) &&
                  strcmp(output,
                         "4bfd2c8b6f1eec7a2afeb48b934ee4b2694182027e6d0fc075074f2fabb31781") ==
                      0,
              "hashes all 65 bytes using the independent host-tool oracle");

    test_case(decode_hex(multi_block_input, vector_bytes, sizeof vector_bytes,
                         &vector_length) &&
                  vector_length == 163u &&
                  gbl_sha256_span(vector_bytes, vector_length, output) &&
                  strcmp(output, multi_block_digest) == 0,
              "hashes the 163-byte NIST CAVP multi-block vector");

    for (index = 0u; index < sizeof image; ++index) {
        image[index] = (uint8_t)(index * 29u + 7u);
    }
    test_case(gbl_sha256_span(image, sizeof image, output) &&
                  strcmp(output,
                         "2fab4f0a508c865101d01445973308736bfafe51558ff59fe96269828d18a5b3") ==
                      0,
              "hashes the complete synthetic 192-byte image span");
    image[sizeof image - 1u] ^= UINT8_C(0x01);
    test_case(gbl_sha256_span(image, sizeof image, output) &&
                  strcmp(output,
                         "4a63547bc36094961c8c8356d537b070a5bd144cb440bacb48e6f47705a115f1") ==
                      0,
              "includes the final byte when hashing a complete image span");

    memset(output, 'Q', sizeof output);
    memcpy(output_before, output, sizeof output);
    test_case(!gbl_sha256_span(NULL, 1u, output) &&
                  memcmp(output, output_before, sizeof output) == 0,
              "refuses a NULL nonempty span without changing output");
    test_case(!gbl_sha256_span(vector_bytes, 1u, NULL),
              "refuses a NULL output buffer");

    for (index = 0u; index < sizeof overlap; ++index) {
        overlap[index] = (uint8_t)index;
    }
    memcpy(overlap_before, overlap, sizeof overlap);
    test_case(!gbl_sha256_span(overlap, 64u, (char *)&overlap[16]) &&
                  memcmp(overlap, overlap_before, sizeof overlap) == 0,
              "refuses overlapping input and output without changing bytes");

#if SIZE_MAX > UINT64_MAX / 8u
    memset(output, 'R', sizeof output);
    memcpy(output_before, output, sizeof output);
    test_case(!gbl_sha256_span((const uint8_t *)(uintptr_t)1u,
                               (size_t)(UINT64_MAX / UINT64_C(8) +
                                        UINT64_C(1)),
                               output) &&
                  memcmp(output, output_before, sizeof output) == 0,
              "refuses a span whose SHA-256 bit length overflows");
#endif

    return test_failures == 0u && test_number == HASH_TEST_COUNT ? 0 : 1;
}
