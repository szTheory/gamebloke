#ifndef GAMEBLOKE_LOGO_SHA256_H
#define GAMEBLOKE_LOGO_SHA256_H

#include <stddef.h>
#include <stdint.h>

/* Original fixed-window SHA-256 implementation based on NIST FIPS 180-4,
 * sections 4.1.2, 4.2.2, 5.1.1, 5.3.3, and 6.2.2. This is not a FIPS-validated
 * implementation. Its only message domain is the 156-byte GBA header window. */

#define GBL_SHA256_WINDOW_BYTES 156u
#define GBL_SHA256_BLOCK_BYTES 64u
#define GBL_SHA256_PADDED_BYTES 192u

static const uint32_t gbl_sha256_round_constants[64] = {
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

static uint32_t gbl_sha256_rotate_right(uint32_t value, unsigned int count)
{
    return (value >> count) | (value << (32u - count));
}

static uint32_t gbl_sha256_small_sigma0(uint32_t value)
{
    return gbl_sha256_rotate_right(value, 7u) ^
           gbl_sha256_rotate_right(value, 18u) ^ (value >> 3u);
}

static uint32_t gbl_sha256_small_sigma1(uint32_t value)
{
    return gbl_sha256_rotate_right(value, 17u) ^
           gbl_sha256_rotate_right(value, 19u) ^ (value >> 10u);
}

static uint32_t gbl_sha256_big_sigma0(uint32_t value)
{
    return gbl_sha256_rotate_right(value, 2u) ^
           gbl_sha256_rotate_right(value, 13u) ^
           gbl_sha256_rotate_right(value, 22u);
}

static uint32_t gbl_sha256_big_sigma1(uint32_t value)
{
    return gbl_sha256_rotate_right(value, 6u) ^
           gbl_sha256_rotate_right(value, 11u) ^
           gbl_sha256_rotate_right(value, 25u);
}

static uint32_t gbl_sha256_choose(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & y) ^ (~x & z);
}

static uint32_t gbl_sha256_majority(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & y) ^ (x & z) ^ (y & z);
}

static void gbl_logo_sha256_fixed_156(const uint8_t input[156],
                                     uint8_t output[32])
{
    uint8_t padded[GBL_SHA256_PADDED_BYTES];
    uint32_t hash[8] = {
        UINT32_C(0x6a09e667), UINT32_C(0xbb67ae85),
        UINT32_C(0x3c6ef372), UINT32_C(0xa54ff53a),
        UINT32_C(0x510e527f), UINT32_C(0x9b05688c),
        UINT32_C(0x1f83d9ab), UINT32_C(0x5be0cd19)
    };
    size_t index;
    size_t block;

    for (index = 0u; index < GBL_SHA256_WINDOW_BYTES; ++index) {
        padded[index] = input[index];
    }
    padded[156u] = UINT8_C(0x80);
    for (index = 157u; index < 184u; ++index) {
        padded[index] = 0u;
    }
    padded[184u] = 0u;
    padded[185u] = 0u;
    padded[186u] = 0u;
    padded[187u] = 0u;
    padded[188u] = 0u;
    padded[189u] = 0u;
    padded[190u] = UINT8_C(0x04);
    padded[191u] = UINT8_C(0xe0);

    for (block = 0u; block < 3u; ++block) {
        uint32_t schedule[64];
        uint32_t a;
        uint32_t b;
        uint32_t c;
        uint32_t d;
        uint32_t e;
        uint32_t f;
        uint32_t g;
        uint32_t h;
        unsigned int word_index;

        for (word_index = 0u; word_index < 16u; ++word_index) {
            const size_t offset = block * GBL_SHA256_BLOCK_BYTES +
                                  (size_t)word_index * 4u;

            schedule[word_index] =
                ((uint32_t)padded[offset] << 24u) |
                ((uint32_t)padded[offset + 1u] << 16u) |
                ((uint32_t)padded[offset + 2u] << 8u) |
                (uint32_t)padded[offset + 3u];
        }
        for (word_index = 16u; word_index < 64u; ++word_index) {
            schedule[word_index] =
                gbl_sha256_small_sigma1(schedule[word_index - 2u]) +
                schedule[word_index - 7u] +
                gbl_sha256_small_sigma0(schedule[word_index - 15u]) +
                schedule[word_index - 16u];
        }

        a = hash[0];
        b = hash[1];
        c = hash[2];
        d = hash[3];
        e = hash[4];
        f = hash[5];
        g = hash[6];
        h = hash[7];
        for (word_index = 0u; word_index < 64u; ++word_index) {
            const uint32_t first = h + gbl_sha256_big_sigma1(e) +
                                   gbl_sha256_choose(e, f, g) +
                                   gbl_sha256_round_constants[word_index] +
                                   schedule[word_index];
            const uint32_t second = gbl_sha256_big_sigma0(a) +
                                    gbl_sha256_majority(a, b, c);
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
        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
        hash[5] += f;
        hash[6] += g;
        hash[7] += h;
    }

    for (index = 0u; index < 8u; ++index) {
        const uint32_t word = hash[index];
        output[index * 4u] = (uint8_t)(word >> 24u);
        output[index * 4u + 1u] = (uint8_t)(word >> 16u);
        output[index * 4u + 2u] = (uint8_t)(word >> 8u);
        output[index * 4u + 3u] = (uint8_t)word;
    }
}

static int gbl_logo_sha256_matches_hex(const uint8_t digest[32],
                                       const char *hex)
{
    size_t index;

    for (index = 0u; index < 32u; ++index) {
        unsigned int high;
        unsigned int low;
        const char high_char = hex[index * 2u];
        const char low_char = hex[index * 2u + 1u];

        if (high_char >= '0' && high_char <= '9') {
            high = (unsigned int)(high_char - '0');
        } else if (high_char >= 'a' && high_char <= 'f') {
            high = (unsigned int)(high_char - 'a') + 10u;
        } else {
            return 0;
        }
        if (low_char >= '0' && low_char <= '9') {
            low = (unsigned int)(low_char - '0');
        } else if (low_char >= 'a' && low_char <= 'f') {
            low = (unsigned int)(low_char - 'a') + 10u;
        } else {
            return 0;
        }
        if (digest[index] != (uint8_t)((high << 4u) | low)) {
            return 0;
        }
    }
    return hex[64u] == '\0';
}

#undef GBL_SHA256_WINDOW_BYTES
#undef GBL_SHA256_BLOCK_BYTES
#undef GBL_SHA256_PADDED_BYTES

#endif /* GAMEBLOKE_LOGO_SHA256_H */
