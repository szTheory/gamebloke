#ifndef GAMEBLOKE_RUNNER_SHA256_H
#define GAMEBLOKE_RUNNER_SHA256_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GBL_SHA256_DIGEST_BYTES 32u
#define GBL_SHA256_HEX_CAPACITY 65u

/*
 * Hashes exactly `length` caller-owned bytes and writes 64 lowercase
 * hexadecimal characters followed by NUL to `out_hex65`.
 *
 * A NULL data pointer is valid only when length is zero. The output must be a
 * writable 65-byte buffer and must not overlap the input span. Lengths whose
 * bit count cannot be represented in SHA-256's 64-bit length field are
 * rejected. On any rejected argument, the function returns false and leaves
 * the output buffer unchanged. The caller remains responsible for supplying
 * accessible objects of the declared sizes and for preventing concurrent
 * mutation of the input.
 *
 * This original host-side implementation follows NIST FIPS 180-4, Sections
 * 5.1.1, 5.2, and 6.2. It is not a FIPS-validated cryptographic module.
 */
bool gbl_sha256_span(const uint8_t *data, size_t length, char *out_hex65);

#endif /* GAMEBLOKE_RUNNER_SHA256_H */
