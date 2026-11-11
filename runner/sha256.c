#include "sha256.h"

#include <stdint.h>
#include <string.h>

/*
 * Original SHA-256 implementation written from the algorithm in NIST FIPS
 * 180-4 (2015), Sections 5.1.1, 5.2, and 6.2. The existing logo-window
 * helper is a distinct fixed-size implementation and is not included here.
 * This code has not undergone FIPS validation.
 * Standard text: https://csrc.nist.gov/files/pubs/fips/180-4/final/docs/fips180-4.pdf
 */

enum {
    GBL_SHA256_BLOCK_BYTES = 64u,
    GBL_SHA256_LENGTH_OFFSET = 56u,
    GBL_SHA256_ROUNDS = 64u
};

static const uint32_t round_constant[GBL_SHA256_ROUNDS] = {
    UINT32_C(0x428a2f98), UINT32_C(0x71374491), UINT32_C(0xb5c0fbcf),
    UINT32_C(0xe9b5dba5), UINT32_C(0x3956c25b), UINT32_C(0x59f111f1),
    UINT32_C(0x923f82a4), UINT32_C(0xab1c5ed5), UINT32_C(0xd807aa98),
    UINT32_C(0x12835b01), UINT32_C(0x243185be), UINT32_C(0x550c7dc3),
    UINT32_C(0x72be5d74), UINT32_C(0x80deb1fe), UINT32_C(0x9bdc06a7),
    UINT32_C(0xc19bf174), UINT32_C(0xe49b69c1), UINT32_C(0xefbe4786),
    UINT32_C(0x0fc19dc6), UINT32_C(0x240ca1cc), UINT32_C(0x2de92c6f),
    UINT32_C(0x4a7484aa), UINT32_C(0x5cb0a9dc), UINT32_C(0x76f988da),
    UINT32_C(0x983e5152), UINT32_C(0xa831c66d), UINT32_C(0xb00327c8),
    UINT32_C(0xbf597fc7), UINT32_C(0xc6e00bf3), UINT32_C(0xd5a79147),
    UINT32_C(0x06ca6351), UINT32_C(0x14292967), UINT32_C(0x27b70a85),
    UINT32_C(0x2e1b2138), UINT32_C(0x4d2c6dfc), UINT32_C(0x53380d13),
    UINT32_C(0x650a7354), UINT32_C(0x766a0abb), UINT32_C(0x81c2c92e),
    UINT32_C(0x92722c85), UINT32_C(0xa2bfe8a1), UINT32_C(0xa81a664b),
    UINT32_C(0xc24b8b70), UINT32_C(0xc76c51a3), UINT32_C(0xd192e819),
    UINT32_C(0xd6990624), UINT32_C(0xf40e3585), UINT32_C(0x106aa070),
    UINT32_C(0x19a4c116), UINT32_C(0x1e376c08), UINT32_C(0x2748774c),
    UINT32_C(0x34b0bcb5), UINT32_C(0x391c0cb3), UINT32_C(0x4ed8aa4a),
    UINT32_C(0x5b9cca4f), UINT32_C(0x682e6ff3), UINT32_C(0x748f82ee),
    UINT32_C(0x78a5636f), UINT32_C(0x84c87814), UINT32_C(0x8cc70208),
    UINT32_C(0x90befffa), UINT32_C(0xa4506ceb), UINT32_C(0xbef9a3f7),
    UINT32_C(0xc67178f2)
};

static uint32_t rotate_right(uint32_t value, unsigned int count)
{
    return (value >> count) | (value << (32u - count));
}

static uint32_t small_sigma_zero(uint32_t value)
{
    return rotate_right(value, 7u) ^ rotate_right(value, 18u) ^
           (value >> 3u);
}

static uint32_t small_sigma_one(uint32_t value)
{
    return rotate_right(value, 17u) ^ rotate_right(value, 19u) ^
           (value >> 10u);
}

static uint32_t large_sigma_zero(uint32_t value)
{
    return rotate_right(value, 2u) ^ rotate_right(value, 13u) ^
           rotate_right(value, 22u);
}

static uint32_t large_sigma_one(uint32_t value)
{
    return rotate_right(value, 6u) ^ rotate_right(value, 11u) ^
           rotate_right(value, 25u);
}

static uint32_t choose(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & y) ^ ((~x) & z);
}

static uint32_t majority(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & y) ^ (x & z) ^ (y & z);
}

static uint32_t load_word(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24u) |
           ((uint32_t)bytes[1] << 16u) |
           ((uint32_t)bytes[2] << 8u) | (uint32_t)bytes[3];
}

static void compress_block(uint32_t state[8], const uint8_t block[64])
{
    uint32_t schedule[GBL_SHA256_ROUNDS];
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];
    unsigned int index;

    for (index = 0u; index < 16u; ++index) {
        schedule[index] = load_word(&block[(size_t)index * 4u]);
    }
    for (index = 16u; index < GBL_SHA256_ROUNDS; ++index) {
        schedule[index] = small_sigma_one(schedule[index - 2u]) +
                          schedule[index - 7u] +
                          small_sigma_zero(schedule[index - 15u]) +
                          schedule[index - 16u];
    }

    for (index = 0u; index < GBL_SHA256_ROUNDS; ++index) {
        const uint32_t first = h + large_sigma_one(e) + choose(e, f, g) +
                               round_constant[index] + schedule[index];
        const uint32_t second = large_sigma_zero(a) + majority(a, b, c);
        const uint32_t next_a = first + second;

        h = g;
        g = f;
        f = e;
        e = d + first;
        d = c;
        c = b;
        b = a;
        a = next_a;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

static bool span_overlaps_output(const uint8_t *data, size_t length,
                                 const char *output)
{
    uintptr_t input_start;
    uintptr_t output_start;
    uintptr_t input_end;
    uintptr_t output_end;

    if (length == 0u) {
        return false;
    }

    input_start = (uintptr_t)(const void *)data;
    output_start = (uintptr_t)(const void *)output;
    if ((uintmax_t)length >
            (uintmax_t)UINTPTR_MAX - (uintmax_t)input_start ||
        (uintmax_t)GBL_SHA256_HEX_CAPACITY >
            (uintmax_t)UINTPTR_MAX - (uintmax_t)output_start) {
        return true;
    }

    input_end = input_start + (uintptr_t)length;
    output_end = output_start + (uintptr_t)GBL_SHA256_HEX_CAPACITY;
    return input_start < output_end && output_start < input_end;
}

bool gbl_sha256_span(const uint8_t *data, size_t length, char *out_hex65)
{
    static const char hex_digits[] = "0123456789abcdef";
    uint32_t state[8] = {
        UINT32_C(0x6a09e667), UINT32_C(0xbb67ae85),
        UINT32_C(0x3c6ef372), UINT32_C(0xa54ff53a),
        UINT32_C(0x510e527f), UINT32_C(0x9b05688c),
        UINT32_C(0x1f83d9ab), UINT32_C(0x5be0cd19)
    };
    const uintmax_t byte_length = (uintmax_t)length;
    uint64_t bit_length;
    uint8_t final_block[GBL_SHA256_BLOCK_BYTES] = {0u};
    size_t processed = 0u;
    size_t remainder;
    unsigned int index;

    if (out_hex65 == NULL || (data == NULL && length != 0u) ||
        byte_length > (uintmax_t)(UINT64_MAX / UINT64_C(8))) {
        return false;
    }
    if (span_overlaps_output(data, length, out_hex65)) {
        return false;
    }

    bit_length = (uint64_t)byte_length * UINT64_C(8);
    while (length - processed >= GBL_SHA256_BLOCK_BYTES) {
        compress_block(state, &data[processed]);
        processed += GBL_SHA256_BLOCK_BYTES;
    }

    remainder = length - processed;
    if (remainder != 0u) {
        memcpy(final_block, &data[processed], remainder);
    }
    final_block[remainder] = UINT8_C(0x80);
    if (remainder >= GBL_SHA256_LENGTH_OFFSET) {
        compress_block(state, final_block);
        memset(final_block, 0, sizeof final_block);
    }
    for (index = 0u; index < 8u; ++index) {
        const unsigned int shift = 56u - index * 8u;

        final_block[GBL_SHA256_LENGTH_OFFSET + (size_t)index] =
            (uint8_t)(bit_length >> shift);
    }
    compress_block(state, final_block);

    for (index = 0u; index < 8u; ++index) {
        unsigned int byte_index;

        for (byte_index = 0u; byte_index < 4u; ++byte_index) {
            const uint32_t shift = 24u - (uint32_t)byte_index * 8u;
            const uint8_t byte = (uint8_t)(state[index] >> shift);
            const size_t output_index = (size_t)index * 8u +
                                        (size_t)byte_index * 2u;

            out_hex65[output_index] = hex_digits[(unsigned int)(byte >> 4u)];
            out_hex65[output_index + 1u] =
                hex_digits[(unsigned int)(byte & UINT8_C(0x0f))];
        }
    }
    out_hex65[GBL_SHA256_HEX_CAPACITY - 1u] = '\0';
    return true;
}
