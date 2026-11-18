#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    IMAGE_SIZE = 0x240u,
    PLAYABLE0_IMAGE_SIZE = 0x800u,
    ARM_ENTRY_OFFSET = 0x00u,
    ARM_CODE_OFFSET = 0xC0u,
    THUMB_CODE_OFFSET = 0x1A0u,
    TIMER0_MAILBOX_OFFSET = 0x80u,
    ARM_LABEL_COUNT = 14u,
    ARM_BRANCH_CAPACITY = 48u,
    ARM_LITERAL_CAPACITY = 24u
};

#define MAILBOX_ADDRESS UINT32_C(0x03007FC0)
#define MAILBOX_MAGIC UINT32_C(0x47424C4B)
#define MAILBOX_VERSION UINT32_C(1)
#define MAILBOX_COMPLETION UINT32_C(0x434F4D50)

enum {
    ARM_LABEL_BACKGROUND_ROW = 0,
    ARM_LABEL_BACKGROUND_PIXEL,
    ARM_LABEL_INITIAL_LEAVE_VBLANK,
    ARM_LABEL_INITIAL_WAIT_VBLANK,
    ARM_LABEL_INPUT_FRAME,
    ARM_LABEL_NEXT_LEAVE_VBLANK,
    ARM_LABEL_NEXT_WAIT_VBLANK,
    ARM_LABEL_FINISH,
    ARM_LABEL_FINISH_LEAVE_VBLANK,
    ARM_LABEL_FINISH_WAIT_VBLANK,
    ARM_LABEL_DONE,
    ARM_LABEL_DRAW_RECT,
    ARM_LABEL_DRAW_ROW,
    ARM_LABEL_DRAW_PIXEL
};

typedef struct arm_branch_fixup {
    size_t instruction_offset;
    size_t label;
    uint32_t condition;
    bool link;
} arm_branch_fixup;

typedef struct arm_literal_fixup {
    size_t instruction_offset;
    size_t literal_index;
    uint32_t destination_register;
    uint32_t condition;
} arm_literal_fixup;

typedef struct arm_image_builder {
    uint8_t *image;
    size_t cursor;
    size_t labels[ARM_LABEL_COUNT];
    bool label_is_set[ARM_LABEL_COUNT];
    arm_branch_fixup branches[ARM_BRANCH_CAPACITY];
    size_t branch_count;
    arm_literal_fixup literal_fixups[ARM_LITERAL_CAPACITY];
    size_t literal_fixup_count;
    uint32_t literal_values[ARM_LITERAL_CAPACITY];
    size_t literal_count;
    bool okay;
} arm_image_builder;

enum {
    ARM_CONDITION_EQ = 0x0u,
    ARM_CONDITION_NE = 0x1u,
    ARM_CONDITION_AL = 0xEu
};

static void put_u32(uint8_t *image, size_t offset, uint32_t value);
static void put_direct_start_branch(uint8_t *image);

static void arm_emit(arm_image_builder *builder, uint32_t instruction)
{
    if (builder->cursor + 4u > PLAYABLE0_IMAGE_SIZE) {
        builder->okay = false;
        return;
    }
    put_u32(builder->image, builder->cursor, instruction);
    builder->cursor += 4u;
}

static void arm_set_label(arm_image_builder *builder, size_t label)
{
    if (label >= ARM_LABEL_COUNT || builder->label_is_set[label]) {
        builder->okay = false;
        return;
    }
    builder->labels[label] = builder->cursor;
    builder->label_is_set[label] = true;
}

static void arm_branch(arm_image_builder *builder, uint32_t condition,
                      size_t label, bool link)
{
    arm_branch_fixup *fixup;

    if (label >= ARM_LABEL_COUNT ||
        builder->branch_count >= ARM_BRANCH_CAPACITY) {
        builder->okay = false;
        return;
    }
    fixup = &builder->branches[builder->branch_count++];
    fixup->instruction_offset = builder->cursor;
    fixup->label = label;
    fixup->condition = condition;
    fixup->link = link;
    arm_emit(builder, 0u);
}

static void arm_load_literal(arm_image_builder *builder,
                             uint32_t destination_register, uint32_t value,
                             uint32_t condition)
{
    size_t index;
    arm_literal_fixup *fixup;

    for (index = 0u; index < builder->literal_count; ++index) {
        if (builder->literal_values[index] == value) {
            break;
        }
    }
    if (index == builder->literal_count) {
        if (builder->literal_count >= ARM_LITERAL_CAPACITY) {
            builder->okay = false;
            return;
        }
        builder->literal_values[index] = value;
        builder->literal_count += 1u;
    }
    if (builder->literal_fixup_count >= ARM_LITERAL_CAPACITY) {
        builder->okay = false;
        return;
    }
    fixup = &builder->literal_fixups[builder->literal_fixup_count++];
    fixup->instruction_offset = builder->cursor;
    fixup->literal_index = index;
    fixup->destination_register = destination_register;
    fixup->condition = condition;
    arm_emit(builder, 0u);
}

static bool arm_finish(arm_image_builder *builder)
{
    size_t literal_base;
    size_t index;

    if (!builder->okay) {
        return false;
    }
    literal_base = builder->cursor;
    if (literal_base + builder->literal_count * 4u > PLAYABLE0_IMAGE_SIZE) {
        return false;
    }
    for (index = 0u; index < builder->literal_count; ++index) {
        put_u32(builder->image, literal_base + index * 4u,
                builder->literal_values[index]);
    }
    for (index = 0u; index < builder->literal_fixup_count; ++index) {
        const arm_literal_fixup *fixup = &builder->literal_fixups[index];
        const size_t literal_offset =
            literal_base + fixup->literal_index * 4u;
        const size_t pc_offset = fixup->instruction_offset + 8u;
        uint32_t instruction;

        if (literal_offset < pc_offset || literal_offset - pc_offset > 4095u) {
            return false;
        }
        instruction = (fixup->condition << 28u) | UINT32_C(0x059F0000) |
                      (fixup->destination_register << 12u) |
                      (uint32_t)(literal_offset - pc_offset);
        put_u32(builder->image, fixup->instruction_offset, instruction);
    }
    for (index = 0u; index < builder->branch_count; ++index) {
        const arm_branch_fixup *fixup = &builder->branches[index];
        int64_t delta;
        int64_t word_offset;
        uint32_t instruction;

        if (!builder->label_is_set[fixup->label]) {
            return false;
        }
        delta = (int64_t)builder->labels[fixup->label] -
                (int64_t)(fixup->instruction_offset + 8u);
        if ((delta % 4) != 0) {
            return false;
        }
        word_offset = delta / 4;
        if (word_offset < -INT64_C(0x800000) ||
            word_offset > INT64_C(0x7FFFFF)) {
            return false;
        }
        instruction = (fixup->condition << 28u) |
                      (fixup->link ? UINT32_C(0x0B000000)
                                   : UINT32_C(0x0A000000)) |
                      ((uint32_t)word_offset & UINT32_C(0x00FFFFFF));
        put_u32(builder->image, fixup->instruction_offset, instruction);
    }
    return true;
}

static bool make_path(char *out, size_t capacity, const char *directory,
                      const char *name);

static bool write_image_sized(const char *directory, const char *name,
                              const uint8_t *image, size_t image_size)
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
    okay = fwrite(image, 1u, image_size, file) == image_size;
    if (fclose(file) != 0) {
        okay = false;
    }
    if (!okay) {
        fprintf(stderr, "instruction-image: cannot write complete image: %s\n",
                name);
    }
    return okay;
}

static bool make_playable0_image(uint8_t image[PLAYABLE0_IMAGE_SIZE],
                                bool input_disabled, bool wrong_color,
                                bool wrong_address)
{
    arm_image_builder builder = { 0 };
    const uint32_t neutral_color = wrong_color ? UINT32_C(0x001F) :
                                                 UINT32_C(0x03E0);
    const uint32_t pressed_color = wrong_color ? UINT32_C(0x03E0) :
                                                 UINT32_C(0x7C00);
    const uint32_t row_address = wrong_address ? UINT32_C(0x06009608) :
                                                 UINT32_C(0x06009600);

    memset(image, 0, PLAYABLE0_IMAGE_SIZE);
    put_direct_start_branch(image);
    builder.image = image;
    builder.cursor = ARM_CODE_OFFSET;
    builder.okay = true;

    arm_load_literal(&builder, 4u, UINT32_C(0x0842), ARM_CONDITION_AL);
    arm_load_literal(&builder, 3u, UINT32_C(0x1084), ARM_CONDITION_AL);
    arm_load_literal(&builder, 7u, UINT32_C(0x06000000), ARM_CONDITION_AL);
    arm_load_literal(&builder, 12u, MAILBOX_ADDRESS, ARM_CONDITION_AL);
    arm_load_literal(&builder, 5u, UINT32_C(0x04000006), ARM_CONDITION_AL);
    arm_load_literal(&builder, 6u, UINT32_C(0x04000130), ARM_CONDITION_AL);

    /* Initialize the existing five-word mailbox before recording progress. */
    arm_load_literal(&builder, 0u, MAILBOX_MAGIC, ARM_CONDITION_AL);
    arm_emit(&builder, UINT32_C(0xE58C0000));
    arm_load_literal(&builder, 0u, MAILBOX_VERSION, ARM_CONDITION_AL);
    arm_emit(&builder, UINT32_C(0xE58C0004));
    arm_emit(&builder, UINT32_C(0xE3A00001));
    arm_emit(&builder, UINT32_C(0xE58C0008));

    /* Original two-tone horizontal bands fill the 240x160 mode-3 surface. */
    arm_emit(&builder, UINT32_C(0xE3A09000));
    arm_set_label(&builder, ARM_LABEL_BACKGROUND_ROW);
    arm_emit(&builder, UINT32_C(0xE2090010));
    arm_emit(&builder, UINT32_C(0xE3500000));
    arm_emit(&builder, UINT32_C(0xE1A0A004));
    arm_emit(&builder, UINT32_C(0x11A0A003));
    arm_emit(&builder, UINT32_C(0xE3A0B0F0));
    arm_set_label(&builder, ARM_LABEL_BACKGROUND_PIXEL);
    arm_emit(&builder, UINT32_C(0xE1C7A0B0));
    arm_emit(&builder, UINT32_C(0xE2877002));
    arm_emit(&builder, UINT32_C(0xE25BB001));
    arm_branch(&builder, ARM_CONDITION_NE, ARM_LABEL_BACKGROUND_PIXEL,
               false);
    arm_emit(&builder, UINT32_C(0xE2899001));
    arm_emit(&builder, UINT32_C(0xE35900A0));
    arm_branch(&builder, ARM_CONDITION_NE, ARM_LABEL_BACKGROUND_ROW, false);
    arm_emit(&builder, UINT32_C(0xE1A04003));
    arm_emit(&builder, UINT32_C(0xE3A07000));

    /* Enable mode 3 with BG2 after the background is ready. */
    arm_load_literal(&builder, 0u, UINT32_C(0x04000000), ARM_CONDITION_AL);
    arm_load_literal(&builder, 1u, UINT32_C(0x0403), ARM_CONDITION_AL);
    arm_emit(&builder, UINT32_C(0xE1C010B0));
    arm_emit(&builder, UINT32_C(0xE3A09040));
    arm_emit(&builder, UINT32_C(0xE3A0B000));

    /* Wait for a VBlank edge, then sample active-low logical KEYINPUT. */
    arm_set_label(&builder, ARM_LABEL_INITIAL_LEAVE_VBLANK);
    arm_emit(&builder, UINT32_C(0xE1D500B0));
    arm_emit(&builder, UINT32_C(0xE35000A0));
    arm_branch(&builder, ARM_CONDITION_EQ,
               ARM_LABEL_INITIAL_LEAVE_VBLANK, false);
    arm_set_label(&builder, ARM_LABEL_INITIAL_WAIT_VBLANK);
    arm_emit(&builder, UINT32_C(0xE1D500B0));
    arm_emit(&builder, UINT32_C(0xE35000A0));
    arm_branch(&builder, ARM_CONDITION_NE,
               ARM_LABEL_INITIAL_WAIT_VBLANK, false);

    arm_set_label(&builder, ARM_LABEL_INPUT_FRAME);
    arm_load_literal(&builder, 6u, UINT32_C(0x04000130), ARM_CONDITION_AL);
    arm_emit(&builder, UINT32_C(0xE1D610B0));
    arm_emit(&builder, UINT32_C(0xE58C1018));
    arm_emit(&builder, UINT32_C(0xE2010011));
    arm_emit(&builder, UINT32_C(0xE3A02000));
    arm_emit(&builder, UINT32_C(0xE3500011));
    arm_emit(&builder, UINT32_C(0x03A02001));
    arm_emit(&builder, UINT32_C(0xE3500001));
    arm_emit(&builder, UINT32_C(0x03A02002));
    arm_emit(&builder, UINT32_C(0xE3500000));
    arm_emit(&builder, UINT32_C(0x03A02004));
    arm_emit(&builder, UINT32_C(0xE3500010));
    arm_emit(&builder, UINT32_C(0x03A02008));
    arm_emit(&builder, UINT32_C(0xE1877002));
    if (input_disabled) {
        arm_emit(&builder, UINT32_C(0xE3A0A040));
        arm_emit(&builder, UINT32_C(0xE3A06000));
    } else {
        arm_emit(&builder, UINT32_C(0xE2010010));
        arm_emit(&builder, UINT32_C(0xE3500010));
        arm_emit(&builder, UINT32_C(0xE3A0A040));
        arm_emit(&builder, UINT32_C(0x13A0A080));
        arm_emit(&builder, UINT32_C(0xE2010001));
        arm_emit(&builder, UINT32_C(0xE3500001));
        arm_emit(&builder, UINT32_C(0xE3A06000));
        arm_emit(&builder, UINT32_C(0x13A06001));
    }
    arm_emit(&builder, UINT32_C(0xE1A0500A));
    arm_emit(&builder, UINT32_C(0xE1A0A009));
    arm_emit(&builder, UINT32_C(0xE1A08004));
    arm_branch(&builder, ARM_CONDITION_AL, ARM_LABEL_DRAW_RECT, true);
    arm_emit(&builder, UINT32_C(0xE1A0A005));
    if (input_disabled) {
        arm_load_literal(&builder, 8u, neutral_color, ARM_CONDITION_AL);
    } else {
        arm_emit(&builder, UINT32_C(0xE59C0018));
        arm_emit(&builder, UINT32_C(0xE2000001));
        arm_emit(&builder, UINT32_C(0xE3500001));
        arm_load_literal(&builder, 8u, neutral_color, ARM_CONDITION_AL);
        arm_load_literal(&builder, 8u, pressed_color, ARM_CONDITION_NE);
    }
    arm_branch(&builder, ARM_CONDITION_AL, ARM_LABEL_DRAW_RECT, true);
    arm_emit(&builder, UINT32_C(0xE1A0900A));
    arm_emit(&builder, UINT32_C(0xE28BB001));

    /* case_id encodes signature, iteration, right-key state, x, and color. */
    arm_load_literal(&builder, 3u, UINT32_C(0xA0000000), ARM_CONDITION_AL);
    arm_emit(&builder, UINT32_C(0xE1A00C8B));
    arm_emit(&builder, UINT32_C(0xE1833000));
    arm_emit(&builder, UINT32_C(0xE1A00A87));
    arm_emit(&builder, UINT32_C(0xE1833000));
    arm_emit(&builder, UINT32_C(0xE1A0068A));
    arm_emit(&builder, UINT32_C(0xE1833000));
    arm_emit(&builder, UINT32_C(0xE1A00506));
    arm_emit(&builder, UINT32_C(0xE1833000));
    arm_emit(&builder, UINT32_C(0xE58C300C));
    arm_emit(&builder, UINT32_C(0xE58CB014));
    arm_emit(&builder, UINT32_C(0xE58CA01C));
    arm_emit(&builder, UINT32_C(0xE58C8020));
    arm_load_literal(&builder, 5u, UINT32_C(0x04000006), ARM_CONDITION_AL);
    arm_emit(&builder, UINT32_C(0xE35B0005));
    arm_branch(&builder, ARM_CONDITION_EQ, ARM_LABEL_FINISH, false);

    arm_set_label(&builder, ARM_LABEL_NEXT_LEAVE_VBLANK);
    arm_emit(&builder, UINT32_C(0xE1D500B0));
    arm_emit(&builder, UINT32_C(0xE35000A0));
    arm_branch(&builder, ARM_CONDITION_EQ,
               ARM_LABEL_NEXT_LEAVE_VBLANK, false);
    arm_set_label(&builder, ARM_LABEL_NEXT_WAIT_VBLANK);
    arm_emit(&builder, UINT32_C(0xE1D500B0));
    arm_emit(&builder, UINT32_C(0xE35000A0));
    arm_branch(&builder, ARM_CONDITION_NE,
               ARM_LABEL_NEXT_WAIT_VBLANK, false);
    arm_branch(&builder, ARM_CONDITION_AL, ARM_LABEL_INPUT_FRAME, false);

    /* The final marker is followed by one full supported frame before PASS. */
    arm_set_label(&builder, ARM_LABEL_FINISH);
    arm_set_label(&builder, ARM_LABEL_FINISH_LEAVE_VBLANK);
    arm_emit(&builder, UINT32_C(0xE1D500B0));
    arm_emit(&builder, UINT32_C(0xE35000A0));
    arm_branch(&builder, ARM_CONDITION_EQ,
               ARM_LABEL_FINISH_LEAVE_VBLANK, false);
    arm_set_label(&builder, ARM_LABEL_FINISH_WAIT_VBLANK);
    arm_emit(&builder, UINT32_C(0xE1D500B0));
    arm_emit(&builder, UINT32_C(0xE35000A0));
    arm_branch(&builder, ARM_CONDITION_NE,
               ARM_LABEL_FINISH_WAIT_VBLANK, false);
    arm_emit(&builder, UINT32_C(0xE3A00002));
    arm_emit(&builder, UINT32_C(0xE58C0008));
    arm_load_literal(&builder, 0u, MAILBOX_COMPLETION, ARM_CONDITION_AL);
    arm_emit(&builder, UINT32_C(0xE58C0010));
    arm_set_label(&builder, ARM_LABEL_DONE);
    arm_branch(&builder, ARM_CONDITION_AL, ARM_LABEL_DONE, false);

    arm_set_label(&builder, ARM_LABEL_DRAW_RECT);
    arm_load_literal(&builder, 0u, row_address, ARM_CONDITION_AL);
    arm_emit(&builder, UINT32_C(0xE080308A));
    arm_emit(&builder, UINT32_C(0xE3A01008));
    arm_set_label(&builder, ARM_LABEL_DRAW_ROW);
    arm_emit(&builder, UINT32_C(0xE3A02008));
    arm_set_label(&builder, ARM_LABEL_DRAW_PIXEL);
    arm_emit(&builder, UINT32_C(0xE1C380B0));
    arm_emit(&builder, UINT32_C(0xE2833002));
    arm_emit(&builder, UINT32_C(0xE2522001));
    arm_branch(&builder, ARM_CONDITION_NE, ARM_LABEL_DRAW_PIXEL, false);
    /* Advance by 480 bytes after the eight-pixel row has already added 16. */
    arm_emit(&builder, UINT32_C(0xE2833F74));
    arm_emit(&builder, UINT32_C(0xE2511001));
    arm_branch(&builder, ARM_CONDITION_NE, ARM_LABEL_DRAW_ROW, false);
    arm_emit(&builder, UINT32_C(0xE12FFF1E));

    return arm_finish(&builder);
}

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

static void make_timer0_polling_image(uint8_t image[IMAGE_SIZE],
                                      bool timer_enabled)
{
    const uint32_t timer_control = timer_enabled ? UINT32_C(0xC0) :
                                                   UINT32_C(0x40);
    const uint32_t timer_reload_and_control =
        UINT32_C(0x0000FFFF) | (timer_control << 16u);

    memset(image, 0, IMAGE_SIZE);
    put_direct_start_branch(image);

    /* Load Timer 0's 16-bit reload/control pair and start it (or leave only
     * its IRQ-enable bit set in the disabled control image). */
    put_u32(image, ARM_CODE_OFFSET + 0x00u, UINT32_C(0xE59F0038));
    put_u32(image, ARM_CODE_OFFSET + 0x04u, UINT32_C(0xE59F1038));
    put_u32(image, ARM_CODE_OFFSET + 0x08u, UINT32_C(0xE5801000));

    /* Poll Timer 0's IF bit directly. IE and IME are not implemented by the
     * current model and are not needed to observe the device request. */
    put_u32(image, ARM_CODE_OFFSET + 0x0Cu, UINT32_C(0xE59F303C));
    put_u32(image, ARM_CODE_OFFSET + 0x10u, UINT32_C(0xE1D340B0));
    put_u32(image, ARM_CODE_OFFSET + 0x14u, UINT32_C(0xE3140008));
    put_u32(image, ARM_CODE_OFFSET + 0x18u, UINT32_C(0x0AFFFFFC));
    put_u32(image, ARM_CODE_OFFSET + 0x1Cu, UINT32_C(0xEA000017));

    put_u32(image, ARM_CODE_OFFSET + 0x40u, UINT32_C(0x04000100));
    put_u32(image, ARM_CODE_OFFSET + 0x44u, timer_reload_and_control);
    put_u32(image, ARM_CODE_OFFSET + 0x50u, UINT32_C(0x04000202));

    put_arm_mailbox(image, ARM_CODE_OFFSET + TIMER0_MAILBOX_OFFSET,
                    UINT32_C(2), UINT32_C(7001), true);
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

static void make_register_arm_image(uint8_t image[IMAGE_SIZE], uint32_t value)
{
    memset(image, 0, IMAGE_SIZE);
    put_direct_start_branch(image);
    put_u32(image, ARM_CODE_OFFSET, UINT32_C(0xE3A0C000) | (value & 0xFFu));
    put_u32(image, ARM_CODE_OFFSET + 4u, UINT32_C(0xEAFFFFFE)); /* B . */
}

static void make_register_arm_stop_image(uint8_t image[IMAGE_SIZE])
{
    memset(image, 0, IMAGE_SIZE);
    put_direct_start_branch(image);
    put_u32(image, ARM_CODE_OFFSET, UINT32_C(0xEE000010)); /* coprocessor */
}

static void make_register_thumb_image(uint8_t image[IMAGE_SIZE], uint16_t value)
{
    const size_t thumb_entry_offset = 0xD0u;

    memset(image, 0, IMAGE_SIZE);
    put_direct_start_branch(image);
    put_u32(image, ARM_CODE_OFFSET, UINT32_C(0xE59F0000)); /* LDR r0,[pc] */
    put_u32(image, ARM_CODE_OFFSET + 4u, UINT32_C(0xE12FFF10)); /* BX r0 */
    put_u32(image, ARM_CODE_OFFSET + 8u,
            UINT32_C(0x08000000) + (uint32_t)thumb_entry_offset + 1u);
    put_u16(image, thumb_entry_offset, (uint16_t)(UINT16_C(0x2700) | value));
    put_u16(image, thumb_entry_offset + 2u, UINT16_C(0xE7FE)); /* B . */
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

static bool generate_all(const char *directory, bool include_local_timer0,
                         bool include_local_playable0)
{
    uint8_t image[IMAGE_SIZE];
    uint8_t playable0_image[PLAYABLE0_IMAGE_SIZE];
    bool okay = true;

    make_mailbox_image(image, UINT32_C(2), UINT32_C(0x9001), true);
    okay = write_image(directory, "owned-arm-pass.gba", image) && okay;

    make_mailbox_image(image, UINT32_C(2), 0u, true);
    okay = write_image(directory, "owned-arm-pass-zero-case.gba", image) &&
           okay;

    make_mailbox_image(image, UINT32_C(3), UINT32_C(0x9002), true);
    okay = write_image(directory, "owned-arm-fail.gba", image) && okay;

    make_mailbox_image(image, UINT32_C(2), UINT32_C(0x9003), false);
    okay = write_image(directory, "owned-arm-partial.gba", image) && okay;

    make_loop_image(image);
    okay = write_image(directory, "owned-arm-loop.gba", image) && okay;

    make_exception_image(image, UINT32_C(0xEF000042)); /* SWI #0x42 */
    okay = write_image(directory, "owned-arm-swi.gba", image) && okay;

    make_exception_image(image, UINT32_C(0xE7F000F0));
    /* ARM ARM DDI0100I A3.16.5: architecturally undefined encoding. */
    okay = write_image(directory, "owned-arm-undefined.gba", image) && okay;

    make_exception_image(image, UINT32_C(0xEE000010)); /* coprocessor */
    okay = write_image(directory, "owned-arm-coprocessor.gba", image) && okay;

    make_register_arm_image(image, 0u);
    okay = write_image(directory, "owned-register-arm-zero.gba", image) &&
           okay;
    make_register_arm_image(image, 1u);
    okay = write_image(directory, "owned-register-arm-fail.gba", image) &&
           okay;
    make_register_arm_stop_image(image);
    okay = write_image(directory, "owned-register-arm-stop.gba", image) &&
           okay;
    make_register_thumb_image(image, 0u);
    okay = write_image(directory, "owned-register-thumb-zero.gba", image) &&
           okay;
    make_register_thumb_image(image, 1u);
    okay = write_image(directory, "owned-register-thumb-fail.gba", image) &&
           okay;

    make_thumb_bx_image(image);
    okay = write_image(directory, "owned-arm-thumb-bx-pass.gba", image) &&
           okay;

    if (include_local_timer0) {
        make_timer0_polling_image(image, true);
        okay = write_image(directory, "owned-timer0-pass.gba", image) &&
               okay;
        make_timer0_polling_image(image, false);
        okay = write_image(directory, "owned-timer0-disabled.gba", image) &&
               okay;
    }

    if (include_local_playable0) {
        if (!make_playable0_image(playable0_image, false, false, false)) {
            fputs("instruction-image: could not assemble Playable 0 guest\n",
                  stderr);
            return false;
        }
        okay = write_image_sized(directory, "owned-playable0.gba",
                                 playable0_image, PLAYABLE0_IMAGE_SIZE) &&
               okay;
        if (!make_playable0_image(playable0_image, true, false, false)) {
            fputs("instruction-image: could not assemble input-disabled control\n",
                  stderr);
            return false;
        }
        okay = write_image_sized(directory, "owned-playable0-input-disabled.gba",
                                 playable0_image, PLAYABLE0_IMAGE_SIZE) &&
               okay;
        if (!make_playable0_image(playable0_image, false, true, false)) {
            fputs("instruction-image: could not assemble wrong-color control\n",
                  stderr);
            return false;
        }
        okay = write_image_sized(directory, "owned-playable0-wrong-color.gba",
                                 playable0_image, PLAYABLE0_IMAGE_SIZE) &&
               okay;
        if (!make_playable0_image(playable0_image, false, false, true)) {
            fputs("instruction-image: could not assemble wrong-address control\n",
                  stderr);
            return false;
        }
        okay = write_image_sized(directory, "owned-playable0-wrong-address.gba",
                                 playable0_image, PLAYABLE0_IMAGE_SIZE) &&
               okay;
    }

    /* A generated one-byte mutation is a no-write identity negative control. */
    make_mailbox_image(image, UINT32_C(2), UINT32_C(0x9001), true);
    image[IMAGE_SIZE - 1u] ^= UINT8_C(1);
    okay = write_image(directory, "owned-arm-pass-mutated.gba", image) && okay;

    return okay;
}

int main(int argc, char **argv)
{
    bool include_local_timer0 = false;
    bool include_local_playable0 = false;

    if (argc < 2 || argc > 3 || argv[1][0] == '\0' ||
        (argc == 3 &&
         strcmp(argv[2], "--include-local-timer0") != 0 &&
         strcmp(argv[2], "--include-local-playable0") != 0)) {
        fputs("usage: instruction_image <build-output-directory> "
              "[--include-local-timer0|--include-local-playable0]\n",
              stderr);
        return 2;
    }
    if (argc == 3 && strcmp(argv[2], "--include-local-timer0") == 0) {
        include_local_timer0 = true;
    } else if (argc == 3) {
        include_local_playable0 = true;
    }
    if (!generate_all(argv[1], include_local_timer0,
                      include_local_playable0)) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
