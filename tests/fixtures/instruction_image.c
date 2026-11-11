#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    IMAGE_SIZE = 0x240u,
    ARM_ENTRY_OFFSET = 0x00u,
    ARM_CODE_OFFSET = 0xC0u,
    THUMB_CODE_OFFSET = 0x1A0u
};

#define MAILBOX_ADDRESS UINT32_C(0x03007FC0)
#define MAILBOX_MAGIC UINT32_C(0x47424C4B)
#define MAILBOX_VERSION UINT32_C(1)
#define MAILBOX_COMPLETION UINT32_C(0x434F4D50)

static void put_u16(uint8_t *image, size_t offset, uint16_t value)
{
    image[offset] = (uint8_t)(value & UINT16_C(0x00FF));
    image[offset + 1u] = (uint8_t)(value >> 8u);
}

static void put_u32(uint8_t *image, size_t offset, uint32_t value)
{
    image[offset] = (uint8_t)(value & UINT32_C(0x000000FF));
    image[offset + 1u] = (uint8_t)((value >> 8u) & UINT32_C(0x000000FF));
    image[offset + 2u] = (uint8_t)((value >> 16u) & UINT32_C(0x000000FF));
    image[offset + 3u] = (uint8_t)(value >> 24u);
}

static void put_direct_start_branch(uint8_t *image)
{
    /* ARM B from ROM offset 0 to 0xC0 skips the zero-filled header area. */
    put_u32(image, ARM_ENTRY_OFFSET, UINT32_C(0xEA00002E));
}

static void put_arm_mailbox(uint8_t *image, size_t base,
                            uint32_t state, uint32_t case_id,
                            bool write_completion)
{
    put_u32(image, base + 0x00u, UINT32_C(0xE59F1038)); /* LDR r1,[pc,#0x38] */
    put_u32(image, base + 0x04u, UINT32_C(0xE59F203C)); /* LDR r2,[pc,#0x3C] */
    put_u32(image, base + 0x08u, UINT32_C(0xE5812000)); /* STR r2,[r1,#0] */
    put_u32(image, base + 0x0Cu, UINT32_C(0xE59F2040)); /* LDR r2,[pc,#0x40] */
    put_u32(image, base + 0x10u, UINT32_C(0xE5812004)); /* STR r2,[r1,#4] */
    put_u32(image, base + 0x14u, UINT32_C(0xE59F2040));
    put_u32(image, base + 0x18u, UINT32_C(0xE5812008)); /* STR state */
    put_u32(image, base + 0x1Cu, UINT32_C(0xE59F2040));
    put_u32(image, base + 0x20u, UINT32_C(0xE581200C)); /* STR case_id */

    put_u32(image, base + 0x40u, MAILBOX_ADDRESS);
    put_u32(image, base + 0x48u, MAILBOX_MAGIC);
    put_u32(image, base + 0x54u, MAILBOX_VERSION);
    put_u32(image, base + 0x5Cu, state);
    put_u32(image, base + 0x64u, case_id);

    if (write_completion) {
        put_u32(image, base + 0x24u, UINT32_C(0xE59F2040));
        put_u32(image, base + 0x28u, UINT32_C(0xE5812010)); /* final marker */
        put_u32(image, base + 0x6Cu, MAILBOX_COMPLETION);
        put_u32(image, base + 0x2Cu, UINT32_C(0xEAFFFFFE)); /* bounded loop */
    } else {
        /* Keep an intentionally partial mailbox pending without running zeros. */
        put_u32(image, base + 0x24u, UINT32_C(0xEAFFFFFE));
    }
}

static bool make_path(char *out, size_t capacity, const char *directory,
                      const char *name)
{
    const int written = snprintf(out, capacity, "%s/%s", directory, name);

    return written >= 0 && (size_t)written < capacity;
}

static bool write_image(const char *directory, const char *name,
                        const uint8_t image[IMAGE_SIZE])
{
    char path[1024];
    FILE *file;
    bool okay;

    if (!make_path(path, sizeof path, directory, name)) {
        fputs("instruction-image: output path is too long\n", stderr);
        return false;
    }
    file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "instruction-image: cannot open output: %s\n", name);
        return false;
    }
    okay = fwrite(image, 1u, IMAGE_SIZE, file) == IMAGE_SIZE;
    if (fclose(file) != 0) {
        okay = false;
    }
    if (!okay) {
        fprintf(stderr, "instruction-image: cannot write complete image: %s\n",
                name);
    }
    return okay;
}

static void make_mailbox_image(uint8_t image[IMAGE_SIZE], uint32_t state,
                               uint32_t case_id, bool write_completion)
{
    memset(image, 0, IMAGE_SIZE);
    put_direct_start_branch(image);
    put_arm_mailbox(image, ARM_CODE_OFFSET, state, case_id, write_completion);
}

static void make_loop_image(uint8_t image[IMAGE_SIZE])
{
    memset(image, 0, IMAGE_SIZE);
    put_direct_start_branch(image);
    put_u32(image, ARM_CODE_OFFSET, UINT32_C(0xEAFFFFFE)); /* B . */
}

static void make_exception_image(uint8_t image[IMAGE_SIZE], uint32_t opcode)
{
    memset(image, 0, IMAGE_SIZE);
    put_direct_start_branch(image);
    put_u32(image, ARM_CODE_OFFSET, opcode);
}

static void make_thumb_bx_image(uint8_t image[IMAGE_SIZE])
{
    memset(image, 0, IMAGE_SIZE);
    put_direct_start_branch(image);

    /* ARM loads a Thumb target and ARM return address, then switches via BX. */
    put_u32(image, ARM_CODE_OFFSET + 0x00u, UINT32_C(0xE59F0038));
    put_u32(image, ARM_CODE_OFFSET + 0x04u, UINT32_C(0xE59F103C));
    put_u32(image, ARM_CODE_OFFSET + 0x08u, UINT32_C(0xE12FFF10)); /* BX r0 */
    put_u32(image, ARM_CODE_OFFSET + 0x40u,
            UINT32_C(0x08000000) + (uint32_t)THUMB_CODE_OFFSET + 1u);
    put_u32(image, ARM_CODE_OFFSET + 0x48u,
            UINT32_C(0x08000000) + UINT32_C(0x120));

    /* Thumb MOV r2,#42 then BX r1 returns to the ARM mailbox program. */
    put_u16(image, THUMB_CODE_OFFSET, UINT16_C(0x222A));
    put_u16(image, THUMB_CODE_OFFSET + 2u, UINT16_C(0x4708)); /* BX r1 */
    put_arm_mailbox(image, 0x120u, UINT32_C(2), UINT32_C(0x9005), true);
}

static bool generate_all(const char *directory)
{
    uint8_t image[IMAGE_SIZE];
    bool okay = true;

    make_mailbox_image(image, UINT32_C(2), UINT32_C(0x9001), true);
    okay = write_image(directory, "owned-arm-pass.gba", image) && okay;

    make_mailbox_image(image, UINT32_C(3), UINT32_C(0x9002), true);
    okay = write_image(directory, "owned-arm-fail.gba", image) && okay;

    make_mailbox_image(image, UINT32_C(2), UINT32_C(0x9003), false);
    okay = write_image(directory, "owned-arm-partial.gba", image) && okay;

    make_loop_image(image);
    okay = write_image(directory, "owned-arm-loop.gba", image) && okay;

    make_exception_image(image, UINT32_C(0xEF000042)); /* SWI #0x42 */
    okay = write_image(directory, "owned-arm-swi.gba", image) && okay;

    make_exception_image(image, UINT32_C(0xF1A00000)); /* undefined */
    okay = write_image(directory, "owned-arm-undefined.gba", image) && okay;

    make_exception_image(image, UINT32_C(0xEE000010)); /* coprocessor */
    okay = write_image(directory, "owned-arm-coprocessor.gba", image) && okay;

    make_thumb_bx_image(image);
    okay = write_image(directory, "owned-arm-thumb-bx-pass.gba", image) &&
           okay;

    /* A generated one-byte mutation is a no-write identity negative control. */
    make_mailbox_image(image, UINT32_C(2), UINT32_C(0x9001), true);
    image[IMAGE_SIZE - 1u] ^= UINT8_C(1);
    okay = write_image(directory, "owned-arm-pass-mutated.gba", image) && okay;

    return okay;
}

int main(int argc, char **argv)
{
    if (argc != 2 || argv[1][0] == '\0') {
        fputs("usage: instruction_image <build-output-directory>\n", stderr);
        return 2;
    }
    if (!generate_all(argv[1])) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
