#if defined(__APPLE__)
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef GBL_PHASE21_OUTPUT_DIR
#error "Phase 21 diagnostic generator must be bound to ignored local output"
#endif

enum {
    IMAGE_SIZE = 0x400u,
    ARM_ENTRY_OFFSET = 0x00u,
    ARM_CODE_OFFSET = 0xC0u,
    ARM_LITERAL_LIMIT = 0x300u,
    LITERAL_CAPACITY = 40u,
    FIXUP_CAPACITY = 80u
};

#define MAILBOX_ADDRESS UINT32_C(0x03007FC0)
#define MAILBOX_MAGIC UINT32_C(0x47424C4B)
#define MAILBOX_VERSION UINT32_C(1)
#define MAILBOX_COMPLETION UINT32_C(0x434F4D50)
#define PHASE21_CASE_ID UINT32_C(21001)

typedef struct literal_fixup {
    size_t instruction_offset;
    size_t literal_index;
    uint8_t destination_register;
} literal_fixup;

typedef struct arm_builder {
    uint8_t *image;
    size_t cursor;
    uint32_t literals[LITERAL_CAPACITY];
    size_t literal_count;
    literal_fixup fixups[FIXUP_CAPACITY];
    size_t fixup_count;
    bool okay;
} arm_builder;

static void put_u32(uint8_t *image, size_t offset, uint32_t value)
{
    image[offset] = (uint8_t)(value & UINT32_C(0xFF));
    image[offset + 1u] = (uint8_t)((value >> 8u) & UINT32_C(0xFF));
    image[offset + 2u] = (uint8_t)((value >> 16u) & UINT32_C(0xFF));
    image[offset + 3u] = (uint8_t)(value >> 24u);
}

static bool emit(arm_builder *builder, uint32_t instruction)
{
    if (builder == NULL || !builder->okay ||
        builder->cursor > ARM_LITERAL_LIMIT - 4u) {
        if (builder != NULL) {
            builder->okay = false;
        }
        return false;
    }
    put_u32(builder->image, builder->cursor, instruction);
    builder->cursor += 4u;
    return true;
}

static bool load_literal(arm_builder *builder, uint8_t destination_register,
                         uint32_t value)
{
    size_t index;

    if (builder == NULL || destination_register > 15u || !builder->okay) {
        return false;
    }
    for (index = 0u; index < builder->literal_count; ++index) {
        if (builder->literals[index] == value) {
            break;
        }
    }
    if (index == builder->literal_count) {
        if (builder->literal_count >= LITERAL_CAPACITY) {
            builder->okay = false;
            return false;
        }
        builder->literals[index] = value;
        ++builder->literal_count;
    }
    if (builder->fixup_count >= FIXUP_CAPACITY) {
        builder->okay = false;
        return false;
    }
    builder->fixups[builder->fixup_count++] = (literal_fixup){
        builder->cursor, index, destination_register
    };
    return emit(builder, 0u);
}

static bool branch_to(arm_builder *builder, uint32_t condition,
                      size_t target_offset)
{
    int64_t delta;
    int64_t word_offset;
    uint32_t instruction;

    if (builder == NULL || !builder->okay) {
        if (builder != NULL) {
            builder->okay = false;
        }
        return false;
    }
    delta = (int64_t)target_offset - (int64_t)(builder->cursor + 8u);
    if ((delta % 4) != 0) {
        builder->okay = false;
        return false;
    }
    word_offset = delta / 4;
    if (word_offset < -INT64_C(0x800000) ||
        word_offset > INT64_C(0x7FFFFF)) {
        builder->okay = false;
        return false;
    }
    instruction = (condition << 28u) | UINT32_C(0x0A000000) |
                  ((uint32_t)word_offset & UINT32_C(0x00FFFFFF));
    return emit(builder, instruction);
}

static bool store_halfword_literal(arm_builder *builder, uint32_t address,
                                   uint16_t value)
{
    return load_literal(builder, 0u, address) &&
           load_literal(builder, 1u, (uint32_t)value) &&
           emit(builder, UINT32_C(0xE1C010B0)); /* STRH r1,[r0] */
}

static bool store_word_literal(arm_builder *builder, uint32_t address,
                               uint32_t value)
{
    return load_literal(builder, 0u, address) &&
           load_literal(builder, 1u, value) &&
           emit(builder, UINT32_C(0xE5801000)); /* STR r1,[r0] */
}

static bool arm_finish(arm_builder *builder)
{
    size_t literal_base;
    size_t index;

    if (builder == NULL || !builder->okay ||
        builder->cursor > ARM_LITERAL_LIMIT -
                              builder->literal_count * sizeof(uint32_t)) {
        return false;
    }
    literal_base = builder->cursor;
    for (index = 0u; index < builder->literal_count; ++index) {
        put_u32(builder->image, literal_base + index * 4u,
                builder->literals[index]);
    }
    for (index = 0u; index < builder->fixup_count; ++index) {
        const literal_fixup *fixup = &builder->fixups[index];
        const size_t literal_offset =
            literal_base + fixup->literal_index * 4u;
        const size_t pc_offset = fixup->instruction_offset + 8u;
        uint32_t instruction;

        if (literal_offset < pc_offset ||
            literal_offset - pc_offset > 4095u) {
            return false;
        }
        instruction = UINT32_C(0xE59F0000) |
                      ((uint32_t)fixup->destination_register << 12u) |
                      (uint32_t)(literal_offset - pc_offset);
        put_u32(builder->image, fixup->instruction_offset, instruction);
    }
    return true;
}

static bool wait_for_vcount(arm_builder *builder, uint16_t line)
{
    const size_t loop = builder->cursor;

    return load_literal(builder, 0u, UINT32_C(0x04000006)) &&
           load_literal(builder, 1u, (uint32_t)line) &&
           emit(builder, UINT32_C(0xE1D020B0)) && /* LDRH r2,[r0] */
           emit(builder, UINT32_C(0xE1520001)) && /* CMP r2,r1 */
           branch_to(builder, UINT32_C(1), loop); /* BNE */
}

static bool write_mailbox(arm_builder *builder)
{
    const size_t pass_state = builder->cursor + 16u;
    const size_t state_store = builder->cursor + 20u;

    if (!emit(builder, UINT32_C(0xE3560001)) || /* CMP r6,#1 */
        !branch_to(builder, UINT32_C(0), pass_state) || /* BEQ */
        !emit(builder, UINT32_C(0xE3A07003)) || /* MOV r7,#3: FAIL */
        !branch_to(builder, UINT32_C(14), state_store) || /* B */
        !emit(builder, UINT32_C(0xE3A07002))) { /* MOV r7,#2: PASS */
        return false;
    }
    if (!load_literal(builder, 0u, MAILBOX_ADDRESS) ||
        !load_literal(builder, 1u, MAILBOX_MAGIC) ||
        !emit(builder, UINT32_C(0xE5801000)) ||
        !load_literal(builder, 1u, MAILBOX_VERSION) ||
        !emit(builder, UINT32_C(0xE5801004)) ||
        !emit(builder, UINT32_C(0xE1A01007)) || /* MOV r1,r7 */
        !emit(builder, UINT32_C(0xE5801008)) ||
        !load_literal(builder, 1u, PHASE21_CASE_ID) ||
        !emit(builder, UINT32_C(0xE580100C)) ||
        !load_literal(builder, 1u, MAILBOX_COMPLETION) ||
        !emit(builder, UINT32_C(0xE5801010)) ||
        !emit(builder, UINT32_C(0xEF000003))) { /* no-BIOS SWI terminal */
        return false;
    }
    return true;
}

static bool make_diagnostic(uint8_t image[IMAGE_SIZE])
{
    arm_builder builder = { 0 };
    size_t timer_loop;
    size_t key_a_checked;
    size_t key_b_checked;

    if (image == NULL) {
        return false;
    }
    memset(image, 0, IMAGE_SIZE);
    /* Direct start branches over a synthetic, logo-free GBA header area. */
    put_u32(image, ARM_ENTRY_OFFSET, UINT32_C(0xEA00002E));
    builder.image = image;
    builder.cursor = ARM_CODE_OFFSET;
    builder.okay = true;

    /* Mode 3 / BG2 identity. Row 1 receives red then green; x2 stays zero. */
    if (!store_halfword_literal(&builder, UINT32_C(0x04000000),
                                UINT16_C(0x0403)) ||
        !store_word_literal(&builder, UINT32_C(0x060001E0),
                            UINT32_C(0x03E0001F))) {
        return false;
    }

    /* A is full-gain left and B full-gain right; reset both FIFOs. */
    if (!store_halfword_literal(&builder, UINT32_C(0x04000084),
                                UINT16_C(0x0080)) ||
        !store_halfword_literal(&builder, UINT32_C(0x04000082),
                                UINT16_C(0x9A0C)) ||
        !store_word_literal(&builder, UINT32_C(0x040000A0),
                            UINT32_C(0x20202020)) ||
        !store_word_literal(&builder, UINT32_C(0x040000A4),
                            UINT32_C(0xE8E8E8E8)) ||
        !store_halfword_literal(&builder, UINT32_C(0x04000100),
                                UINT16_C(0xFE00)) ||
        !store_halfword_literal(&builder, UINT32_C(0x04000102),
                                UINT16_C(0x0080)) ||
        !load_literal(&builder, 0u, UINT32_C(0x04000100)) ||
        !emit(&builder, UINT32_C(0xE1D020B0))) { /* LDRH r2,[r0] */
        return false;
    }
    timer_loop = builder.cursor;
    if (!emit(&builder, UINT32_C(0xE1D030B0)) || /* LDRH r3,[r0] */
        !emit(&builder, UINT32_C(0xE1530002)) || /* CMP r3,r2 */
        !branch_to(&builder, UINT32_C(3), builder.cursor + 12u) || /* BLO */
        !emit(&builder, UINT32_C(0xE1A02003)) || /* MOV r2,r3 */
        !branch_to(&builder, UINT32_C(14), timer_loop) || /* B */
        !store_halfword_literal(&builder, UINT32_C(0x04000102),
                                UINT16_C(0))) { /* stop after first pop */
        return false;
    }
    /* Sample active-low A on line 1. Late-A control input misses this window. */
    if (!emit(&builder, UINT32_C(0xE3A06001)) || /* MOV r6,#1 */
        !wait_for_vcount(&builder, UINT16_C(1)) ||
        !load_literal(&builder, 0u, UINT32_C(0x04000130)) ||
        !emit(&builder, UINT32_C(0xE1D020B0)) || /* LDRH r2,[r0] */
        !emit(&builder, UINT32_C(0xE3120001))) { /* TST r2,#1 */
        return false;
    }
    key_a_checked = builder.cursor + 8u;
    if (!branch_to(&builder, UINT32_C(0), key_a_checked) || /* BEQ */
        !emit(&builder, UINT32_C(0xE3A06000)) || /* MOV r6,#0 */
        !wait_for_vcount(&builder, UINT16_C(2)) ||
        !load_literal(&builder, 0u, UINT32_C(0x04000130)) ||
        !emit(&builder, UINT32_C(0xE1D020B0)) || /* LDRH r2,[r0] */
        !emit(&builder, UINT32_C(0xE3120002))) { /* TST r2,#2 */
        return false;
    }
    key_b_checked = builder.cursor + 8u;
    if (!branch_to(&builder, UINT32_C(0), key_b_checked) || /* BEQ */
        !emit(&builder, UINT32_C(0xE3A06000)) || /* MOV r6,#0 */
        !wait_for_vcount(&builder, UINT16_C(160)) ||
        !write_mailbox(&builder) || !arm_finish(&builder)) {
        return false;
    }
    return true;
}

static bool write_image(const char *directory)
{
    const char *file_name = "phase21-diagnostic.gba";
    uint8_t image[IMAGE_SIZE];
    size_t offset = 0u;
    int directory_fd = -1;
    int file_fd = -1;
    bool created = false;
    bool okay;

    if (directory == NULL || strcmp(directory, GBL_PHASE21_OUTPUT_DIR) != 0 ||
        !make_diagnostic(image)) {
        fputs("phase21-diagnostic: output path or bounded image invalid\n",
              stderr);
        return false;
    }
    directory_fd = open(directory, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (directory_fd < 0) {
        fputs("phase21-diagnostic: assigned output directory is unavailable\n",
              stderr);
        return false;
    }
    file_fd = openat(directory_fd, file_name,
                     O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0666);
    if (file_fd < 0) {
        fputs("phase21-diagnostic: cannot exclusively create assigned output\n",
              stderr);
        (void)close(directory_fd);
        return false;
    }
    created = true;
    okay = true;
    while (offset < sizeof image) {
        ssize_t written = write(file_fd, image + offset, sizeof image - offset);

        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            okay = false;
            break;
        }
        offset += (size_t)written;
    }
    if (close(file_fd) != 0) {
        okay = false;
    }
    file_fd = -1;
    if (!okay && created) {
        (void)unlinkat(directory_fd, file_name, 0);
    }
    if (close(directory_fd) != 0) {
        okay = false;
    }
    if (!okay) {
        fputs("phase21-diagnostic: incomplete output\n", stderr);
    }
    return okay;
}

int main(int argc, char **argv)
{
    if (argc != 2 || argv[1] == NULL || argv[1][0] == '\0') {
        fputs("usage: phase21-diagnostic <assigned-ignored-output-directory>\n",
              stderr);
        return 2;
    }
    return write_image(argv[1]) ? EXIT_SUCCESS : EXIT_FAILURE;
}
