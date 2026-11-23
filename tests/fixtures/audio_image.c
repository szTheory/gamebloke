#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GBL_AUDIO_GUEST_OUTPUT_DIR
#error "audio guest generator must be bound to its local build output"
#endif

enum {
    IMAGE_SIZE = 0x400u,
    ARM_ENTRY_OFFSET = 0x00u,
    ARM_CODE_OFFSET = 0xC0u,
    AUDIO_PAYLOAD_OFFSET = 0x300u,
    LITERAL_CAPACITY = 48u,
    FIXUP_CAPACITY = 64u
};

#define MAILBOX_ADDRESS UINT32_C(0x03007FC0)
#define MAILBOX_MAGIC UINT32_C(0x47424C4B)
#define MAILBOX_VERSION UINT32_C(1)
#define MAILBOX_COMPLETION UINT32_C(0x434F4D50)
#define AUDIO_CASE_ID UINT32_C(19001)

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
    image[offset] = (uint8_t)(value & UINT32_C(0x000000FF));
    image[offset + 1u] = (uint8_t)((value >> 8u) & UINT32_C(0x000000FF));
    image[offset + 2u] = (uint8_t)((value >> 16u) & UINT32_C(0x000000FF));
    image[offset + 3u] = (uint8_t)(value >> 24u);
}

static bool emit(arm_builder *builder, uint32_t instruction)
{
    if (builder == NULL || !builder->okay ||
        builder->cursor > AUDIO_PAYLOAD_OFFSET - 4u) {
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
        builder->literal_count += 1u;
    }
    if (builder->fixup_count >= FIXUP_CAPACITY) {
        builder->okay = false;
        return false;
    }
    builder->fixups[builder->fixup_count++] = (literal_fixup){
        .instruction_offset = builder->cursor,
        .literal_index = index,
        .destination_register = destination_register
    };
    return emit(builder, 0u);
}

static bool branch_to(arm_builder *builder, uint32_t condition,
                      size_t target_offset)
{
    const int64_t delta = (int64_t)target_offset -
                          (int64_t)(builder->cursor + 8u);
    int64_t word_offset;
    uint32_t instruction;

    if (builder == NULL || !builder->okay || (delta % 4) != 0) {
        if (builder != NULL) {
            builder->okay = false;
        }
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
        builder->cursor > AUDIO_PAYLOAD_OFFSET -
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

static bool make_audio_image(uint8_t image[IMAGE_SIZE],
                             bool valid_completion_marker)
{
    static const uint8_t refill_payload[16] = {
        UINT8_C(0x80), UINT8_C(0x80), UINT8_C(0x80), UINT8_C(0x80),
        UINT8_C(0x80), UINT8_C(0x80), UINT8_C(0x80), UINT8_C(0x80),
        UINT8_C(0x80), UINT8_C(0x80), UINT8_C(0x80), UINT8_C(0x80),
        UINT8_C(0x80), UINT8_C(0x80), UINT8_C(0x80), UINT8_C(0x80)
    };
    arm_builder builder = { 0 };
    size_t wait_loop;
    size_t delay_loop;
    size_t index;

    if (image == NULL) {
        return false;
    }
    memset(image, 0, IMAGE_SIZE);
    put_u32(image, ARM_ENTRY_OFFSET, UINT32_C(0xEA00002E));
    builder.image = image;
    builder.cursor = ARM_CODE_OFFSET;
    builder.okay = true;

    /* Model-4 software sources: CH2 goes left; FIFO A goes right. */
    if (!store_halfword_literal(&builder, UINT32_C(0x04000084),
                                UINT16_C(0x0080)) ||
        !store_halfword_literal(&builder, UINT32_C(0x04000080),
                                UINT16_C(0x2060)) ||
        !store_halfword_literal(&builder, UINT32_C(0x04000082),
                                UINT16_C(0x0106)) ||
        !store_halfword_literal(&builder, UINT32_C(0x04000068),
                                UINT16_C(0xF0C0)) ||
        !store_halfword_literal(&builder, UINT32_C(0x0400006C),
                                UINT16_C(0xFFFF))) {
        return false;
    }

    if (!load_literal(&builder, 0u, UINT32_C(0x040000A0)) ||
        !load_literal(&builder, 1u, UINT32_C(0x7F7F7F7F))) {
        return false;
    }
    for (index = 0u; index < 8u; ++index) {
        if (!emit(&builder, UINT32_C(0xE5801000))) { /* STR r1,[r0] */
            return false;
        }
    }

    /* One special-timing DMA1 refill copies four words from this ROM. */
    if (!store_word_literal(&builder, UINT32_C(0x040000BC),
                            UINT32_C(0x08000300)) ||
        !store_word_literal(&builder, UINT32_C(0x040000C0),
                            UINT32_C(0x040000A0)) ||
        !store_halfword_literal(&builder, UINT32_C(0x040000C4),
                                UINT16_C(1)) ||
        !store_halfword_literal(&builder, UINT32_C(0x040000C6),
                                UINT16_C(0xB000))) {
        return false;
    }

    /* Timer 0 pops FIFO A every 32 cycles; its first reload is 0xFFE0. */
    if (!store_halfword_literal(&builder, UINT32_C(0x04000100),
                                UINT16_C(0xFFE0)) ||
        !store_halfword_literal(&builder, UINT32_C(0x04000102),
                                UINT16_C(0x00C0)) ||
        !load_literal(&builder, 0u, UINT32_C(0x04000202))) {
        return false;
    }

    wait_loop = builder.cursor;
    if (!emit(&builder, UINT32_C(0xE1D020B0)) || /* LDRH r2,[r0] */
        !emit(&builder, UINT32_C(0xE3120008)) || /* TST r2,#8 */
        !branch_to(&builder, UINT32_C(0x0), wait_loop)) { /* BEQ */
        return false;
    }

    /* Keep running after the first timer request so both sample edges occur. */
    if (!emit(&builder, UINT32_C(0xE3A050FF))) { /* MOV r5,#255 */
        return false;
    }
    delay_loop = builder.cursor;
    if (!emit(&builder, UINT32_C(0xE2555001)) || /* SUBS r5,r5,#1 */
        !branch_to(&builder, UINT32_C(0x1), delay_loop)) { /* BNE */
        return false;
    }

    /* Stop both producers before reporting terminal guest progress. */
    if (!store_halfword_literal(&builder, UINT32_C(0x040000C6),
                                UINT16_C(0)) ||
        !store_halfword_literal(&builder, UINT32_C(0x04000102),
                                UINT16_C(0))) {
        return false;
    }

    /* The distinct mailbox marker is written only after the audio interval. */
    if (!load_literal(&builder, 0u, MAILBOX_ADDRESS) ||
        !load_literal(&builder, 1u, MAILBOX_MAGIC) ||
        !emit(&builder, UINT32_C(0xE5801000)) ||
        !load_literal(&builder, 1u, MAILBOX_VERSION) ||
        !emit(&builder, UINT32_C(0xE5801004)) ||
        !load_literal(&builder, 1u, UINT32_C(2)) ||
        !emit(&builder, UINT32_C(0xE5801008)) ||
        !load_literal(&builder, 1u, AUDIO_CASE_ID) ||
        !emit(&builder, UINT32_C(0xE580100C)) ||
        !load_literal(&builder, 1u,
                      valid_completion_marker
                          ? MAILBOX_COMPLETION
                          : (MAILBOX_COMPLETION ^ UINT32_C(1))) ||
        !emit(&builder, UINT32_C(0xE5801010)) ||
        !emit(&builder, UINT32_C(0xEAFFFFFE)) || !arm_finish(&builder)) {
        return false;
    }

    memcpy(image + AUDIO_PAYLOAD_OFFSET, refill_payload,
           sizeof refill_payload);
    return true;
}

static bool write_audio_image(const char *directory,
                              bool valid_completion_marker)
{
    static const char suffix[] = "/generated/fixtures";
    char path[1024];
    size_t directory_length;
    FILE *file;
    uint8_t image[IMAGE_SIZE];
    int written;
    bool okay;

    if (directory == NULL) {
        return false;
    }
    directory_length = strlen(directory);
    if (strcmp(directory, GBL_AUDIO_GUEST_OUTPUT_DIR) != 0 ||
        directory_length < sizeof suffix - 1u ||
        strcmp(directory + directory_length - (sizeof suffix - 1u), suffix) !=
            0) {
        fputs("audio-image: output must be the local generated/fixtures build path\n",
              stderr);
        return false;
    }
    written = snprintf(path, sizeof path, "%s/%s.gba", directory,
                       valid_completion_marker
                           ? "owned-audio-mix"
                           : "owned-audio-mix-invalid-marker");
    if (written < 0 || (size_t)written >= sizeof path ||
        !make_audio_image(image, valid_completion_marker)) {
        fputs("audio-image: could not construct bounded image\n", stderr);
        return false;
    }
    file = fopen(path, "wb");
    if (file == NULL) {
        fputs("audio-image: cannot open build output\n", stderr);
        return false;
    }
    okay = fwrite(image, 1u, sizeof image, file) == sizeof image;
    if (fclose(file) != 0) {
        okay = false;
    }
    if (!okay) {
        (void)remove(path);
        fputs("audio-image: incomplete build output\n", stderr);
    }
    return okay;
}

int main(int argc, char **argv)
{
    bool valid_completion_marker = true;

    if ((argc != 2 && argc != 3) || argv[1] == NULL || argv[1][0] == '\0' ||
        (argc == 3 &&
         (argv[2] == NULL ||
          strcmp(argv[2], "--negative-completion-marker") != 0))) {
        fputs("usage: gbl_audio_guest_image <local-build>/generated/fixtures "
              "[--negative-completion-marker]\n",
              stderr);
        return 2;
    }
    if (argc == 3) {
        valid_completion_marker = false;
    }
    return write_audio_image(argv[1], valid_completion_marker)
               ? EXIT_SUCCESS
               : EXIT_FAILURE;
}
