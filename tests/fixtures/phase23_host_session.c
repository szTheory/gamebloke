#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef GBL_PHASE23_OUTPUT_PATH
#error "Phase 23 generator requires one fixed local output path"
#endif

enum {
    IMAGE_BYTES = 4096,
    CODE_OFFSET = 0x100,
    WORD_CAPACITY = 640,
    LITERAL_CAPACITY = 160,
    FIXUP_CAPACITY = 160
};

typedef enum guest_label {
    LABEL_WAIT_LINE_1,
    LABEL_WAIT_LINE_6,
    LABEL_WAIT_LINE_14,
    LABEL_WAIT_LINE_24,
    LABEL_WAIT_LINE_162,
    LABEL_PASS,
    LABEL_FAIL_A,
    LABEL_FAIL_LEFT,
    LABEL_FAIL_RELEASE,
    LABEL_COUNT
} guest_label;

typedef struct literal_patch {
    size_t instruction;
    size_t literal;
    uint8_t destination;
} literal_patch;

typedef struct branch_patch {
    size_t instruction;
    guest_label destination;
    uint8_t condition;
} branch_patch;

typedef struct guest_builder {
    uint32_t words[WORD_CAPACITY];
    uint32_t literals[LITERAL_CAPACITY];
    literal_patch literal_patches[FIXUP_CAPACITY];
    branch_patch branch_patches[FIXUP_CAPACITY];
    size_t word_count;
    size_t literal_count;
    size_t literal_patch_count;
    size_t branch_patch_count;
    size_t labels[LABEL_COUNT];
    bool valid;
} guest_builder;

static void write_u32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)(value & UINT32_C(0xff));
    destination[1] = (uint8_t)((value >> 8u) & UINT32_C(0xff));
    destination[2] = (uint8_t)((value >> 16u) & UINT32_C(0xff));
    destination[3] = (uint8_t)((value >> 24u) & UINT32_C(0xff));
}

static bool emit(guest_builder *builder, uint32_t instruction)
{
    if (builder == NULL || !builder->valid ||
        builder->word_count >= WORD_CAPACITY) {
        if (builder != NULL) {
            builder->valid = false;
        }
        return false;
    }
    builder->words[builder->word_count++] = instruction;
    return true;
}

static bool load_word(guest_builder *builder, uint8_t destination,
                      uint32_t value)
{
    size_t literal_index;

    if (builder == NULL || destination > 12u || !builder->valid ||
        builder->literal_patch_count >= FIXUP_CAPACITY) {
        if (builder != NULL) {
            builder->valid = false;
        }
        return false;
    }
    for (literal_index = 0u; literal_index < builder->literal_count;
         ++literal_index) {
        if (builder->literals[literal_index] == value) {
            break;
        }
    }
    if (literal_index == builder->literal_count) {
        if (builder->literal_count >= LITERAL_CAPACITY) {
            builder->valid = false;
            return false;
        }
        builder->literals[builder->literal_count++] = value;
    }
    builder->literal_patches[builder->literal_patch_count++] =
        (literal_patch){ builder->word_count, literal_index, destination };
    return emit(builder, UINT32_C(0xe59f0000) |
                         ((uint32_t)destination << 12u));
}

static bool branch_to(guest_builder *builder, uint8_t condition,
                      guest_label destination)
{
    if (builder == NULL || destination >= LABEL_COUNT ||
        builder->branch_patch_count >= FIXUP_CAPACITY) {
        if (builder != NULL) {
            builder->valid = false;
        }
        return false;
    }
    builder->branch_patches[builder->branch_patch_count++] =
        (branch_patch){ builder->word_count, destination, condition };
    return emit(builder, ((uint32_t)condition << 28u) |
                         UINT32_C(0x0a000000));
}

static void mark(guest_builder *builder, guest_label label)
{
    if (builder == NULL || label >= LABEL_COUNT ||
        builder->labels[label] != SIZE_MAX) {
        if (builder != NULL) {
            builder->valid = false;
        }
        return;
    }
    builder->labels[label] = builder->word_count;
}

static bool store_halfword_constant(guest_builder *builder,
                                   uint32_t address, uint32_t value)
{
    return load_word(builder, 0u, address) && load_word(builder, 1u, value) &&
           emit(builder, UINT32_C(0xe1c010b0));
}

static bool store_word_constant(guest_builder *builder,
                                uint32_t address, uint32_t value)
{
    return load_word(builder, 0u, address) && load_word(builder, 1u, value) &&
           emit(builder, UINT32_C(0xe5801000));
}

static bool wait_for_line(guest_builder *builder, guest_label label,
                          uint32_t line)
{
    if (!load_word(builder, 0u, UINT32_C(0x04000006))) {
        return false;
    }
    mark(builder, label);
    return emit(builder, UINT32_C(0xe1d010b0)) &&
           emit(builder, UINT32_C(0xe3510000) | line) &&
           branch_to(builder, 1u, label);
}

static bool check_key_at_line(guest_builder *builder, guest_label wait_label,
                              uint32_t line, uint32_t expected,
                              guest_label failure)
{
    return wait_for_line(builder, wait_label, line) &&
           load_word(builder, 0u, UINT32_C(0x04000130)) &&
           emit(builder, UINT32_C(0xe1d010b0)) &&
           load_word(builder, 2u, expected) &&
           emit(builder, UINT32_C(0xe1510002)) &&
           branch_to(builder, 1u, failure);
}

static bool write_mailbox(guest_builder *builder, uint32_t state,
                          uint32_t case_id)
{
    return load_word(builder, 0u, UINT32_C(0x03007fc0)) &&
           load_word(builder, 1u, UINT32_C(0x47424c4b)) &&
           emit(builder, UINT32_C(0xe5801000)) &&
           load_word(builder, 1u, UINT32_C(1)) &&
           emit(builder, UINT32_C(0xe5801004)) &&
           load_word(builder, 1u, state) &&
           emit(builder, UINT32_C(0xe5801008)) &&
           load_word(builder, 1u, case_id) &&
           emit(builder, UINT32_C(0xe580100c)) &&
           load_word(builder, 1u, UINT32_C(0x434f4d50)) &&
           emit(builder, UINT32_C(0xe5801010));
}

static bool finish_guest(guest_builder *builder, uint8_t image[IMAGE_BYTES])
{
    size_t literal_base;
    size_t index;

    if (builder == NULL || image == NULL || !builder->valid ||
        builder->word_count + builder->literal_count >
            (IMAGE_BYTES - CODE_OFFSET) / 4u) {
        return false;
    }
    literal_base = builder->word_count;
    for (index = 0u; index < builder->literal_patch_count; ++index) {
        const literal_patch patch = builder->literal_patches[index];
        const size_t instruction_address = CODE_OFFSET + patch.instruction * 4u;
        const size_t literal_address =
            CODE_OFFSET + (literal_base + patch.literal) * 4u;
        const size_t pc_address = instruction_address + 8u;
        const size_t distance = literal_address - pc_address;

        if (literal_address < pc_address || distance > 4095u) {
            return false;
        }
        builder->words[patch.instruction] = UINT32_C(0xe59f0000) |
            ((uint32_t)patch.destination << 12u) | (uint32_t)distance;
    }
    for (index = 0u; index < builder->branch_patch_count; ++index) {
        const branch_patch patch = builder->branch_patches[index];
        const size_t instruction_address = CODE_OFFSET + patch.instruction * 4u;
        int64_t word_offset;
        int64_t delta;

        if (builder->labels[patch.destination] == SIZE_MAX) {
            return false;
        }
        delta = (int64_t)(CODE_OFFSET + builder->labels[patch.destination] * 4u) -
                (int64_t)(instruction_address + 8u);
        if (delta % 4 != 0) {
            return false;
        }
        word_offset = delta / 4;
        if (word_offset < -INT64_C(0x800000) ||
            word_offset > INT64_C(0x7fffff)) {
            return false;
        }
        builder->words[patch.instruction] =
            ((uint32_t)patch.condition << 28u) | UINT32_C(0x0a000000) |
            ((uint32_t)word_offset & UINT32_C(0x00ffffff));
    }
    for (index = 0u; index < builder->word_count; ++index) {
        write_u32(image + CODE_OFFSET + index * 4u, builder->words[index]);
    }
    for (index = 0u; index < builder->literal_count; ++index) {
        write_u32(image + CODE_OFFSET + (literal_base + index) * 4u,
                  builder->literals[index]);
    }
    return true;
}

static bool build_guest(uint8_t image[IMAGE_BYTES])
{
    guest_builder builder = { 0 };
    size_t index;

    builder.valid = true;
    for (index = 0u; index < LABEL_COUNT; ++index) {
        builder.labels[index] = SIZE_MAX;
    }
    memset(image, 0, IMAGE_BYTES);
    write_u32(image, UINT32_C(0xea00003e));

    if (!store_halfword_constant(&builder, UINT32_C(0x05000000),
                                 UINT32_C(0x56aa)) ||
        !wait_for_line(&builder, LABEL_WAIT_LINE_1, 1u) ||
        !store_halfword_constant(&builder, UINT32_C(0x04000084),
                                 UINT32_C(0x0080)) ||
        !store_halfword_constant(&builder, UINT32_C(0x04000082),
                                 UINT32_C(0x2008)) ||
        !store_word_constant(&builder, UINT32_C(0x040000a4),
                             UINT32_C(0xdbdbdbdb)) ||
        !store_halfword_constant(&builder, UINT32_C(0x04000100),
                                 UINT32_C(0xff00)) ||
        !store_halfword_constant(&builder, UINT32_C(0x04000102),
                                 UINT32_C(0x0080)) ||
        !check_key_at_line(&builder, LABEL_WAIT_LINE_6, 6u,
                           UINT32_C(0x03fe), LABEL_FAIL_A) ||
        !check_key_at_line(&builder, LABEL_WAIT_LINE_14, 14u,
                           UINT32_C(0x03df), LABEL_FAIL_LEFT) ||
        !check_key_at_line(&builder, LABEL_WAIT_LINE_24, 24u,
                           UINT32_C(0x03ff), LABEL_FAIL_RELEASE) ||
        !wait_for_line(&builder, LABEL_WAIT_LINE_162, 162u) ||
        !write_mailbox(&builder, 2u, UINT32_C(0x23010001)) ||
        !branch_to(&builder, 14u, LABEL_PASS)) {
        return false;
    }

    mark(&builder, LABEL_PASS);
    if (!branch_to(&builder, 14u, LABEL_PASS)) {
        return false;
    }

    mark(&builder, LABEL_FAIL_A);
    if (!write_mailbox(&builder, 3u, UINT32_C(0x2301f001)) ||
        !branch_to(&builder, 14u, LABEL_FAIL_A)) {
        return false;
    }
    mark(&builder, LABEL_FAIL_LEFT);
    if (!write_mailbox(&builder, 3u, UINT32_C(0x2301f002)) ||
        !branch_to(&builder, 14u, LABEL_FAIL_LEFT)) {
        return false;
    }
    mark(&builder, LABEL_FAIL_RELEASE);
    if (!write_mailbox(&builder, 3u, UINT32_C(0x2301f003)) ||
        !branch_to(&builder, 14u, LABEL_FAIL_RELEASE)) {
        return false;
    }
    return finish_guest(&builder, image);
}

static bool write_all(int descriptor, const uint8_t *bytes, size_t length)
{
    size_t offset = 0u;

    while (offset < length) {
        ssize_t written = write(descriptor, bytes + offset, length - offset);

        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            return false;
        }
        offset += (size_t)written;
    }
    return true;
}

int main(int argc, char **argv)
{
    uint8_t image[IMAGE_BYTES];
    int descriptor;
    bool okay;

    if (argc != 2 || strcmp(argv[1], GBL_PHASE23_OUTPUT_PATH) != 0) {
        fputs("phase23-image: output path differs from its build binding\n",
              stderr);
        return 2;
    }
    if (!build_guest(image)) {
        fputs("phase23-image: bounded ARM guest assembly failed\n", stderr);
        return 1;
    }
    descriptor = open(GBL_PHASE23_OUTPUT_PATH,
                      O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (descriptor < 0) {
        perror("phase23-image: exclusive output creation failed");
        return 1;
    }
    okay = write_all(descriptor, image, sizeof image);
    if (okay && fsync(descriptor) != 0) {
        okay = false;
    }
    if (close(descriptor) != 0) {
        okay = false;
    }
    if (!okay) {
        perror("phase23-image: write or close failed");
        return 1;
    }
    puts("phase23-image: generated bounded local diagnostic");
    return 0;
}
