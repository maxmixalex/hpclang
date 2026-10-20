#include "rpl-recovery-reservation.h"
#include "rpl-object-store.h"
#include <string.h>

static uint32_t read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static void write_le32(uint8_t *bytes, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        bytes[i] = (uint8_t)(value >> (8u * i));
}

static int valid_capacity(uint32_t capacity)
{
    return capacity >= HPCLANG_RPL_RECOVERY_RESERVATION_HEADER_BYTES &&
           capacity <= (0x80000u - 10u) / 2u;
}

static int valid_identity(const uint8_t *identity)
{
    if (memcmp(identity, "HPCROOT1", 8))
        return 0;
    uint32_t span = read_le32(identity + 8), at = read_le32(identity + 12);
    return span <= 0x100004u && span >= 110u && at >= 10u && at <= span - 100u;
}

int hpclang_rpl_recovery_reservation_header(
    const uint8_t identity[HPCLANG_RPL_RECOVERY_IDENTITY_BYTES],
    const struct hpclang_rpl_flash_installed_recovery *installed, uint32_t journal_crc,
    uint32_t capacity, uint8_t header[HPCLANG_RPL_RECOVERY_RESERVATION_HEADER_BYTES])
{
    if (!identity || !installed || !header || !valid_capacity(capacity) ||
        !valid_identity(identity))
        return 0;
    uint8_t encoded[HPCLANG_RPL_RECOVERY_RESERVATION_HEADER_BYTES] = "HPCMEM2";
    memcpy(encoded + 8, identity, HPCLANG_RPL_RECOVERY_IDENTITY_BYTES);
    write_le32(encoded + 56, installed->library_slot);
    write_le32(encoded + 60, installed->journal_slot);
    write_le32(encoded + 64, installed->authority_slot);
    write_le32(encoded + 68, installed->target);
    write_le32(encoded + 72, journal_crc);
    write_le32(encoded + 76, capacity);
    memcpy(header, encoded, sizeof encoded);
    return 1;
}

static int read_word(const struct hpclang_rpl_reader *reader, uint32_t address, unsigned count,
                     uint32_t *word)
{
    uint32_t result = 0;
    for (unsigned i = 0; i < count; ++i) {
        uint8_t nibble;
        if (!reader->read_nibble(reader->context, address + i, &nibble) || nibble > 15)
            return 0;
        result |= (uint32_t)nibble << (4u * i);
    }
    *word = result;
    return 1;
}

int hpclang_rpl_recovery_reservation_match(
    const struct hpclang_rpl_reader *reader, uint32_t object,
    const uint8_t header[HPCLANG_RPL_RECOVERY_RESERVATION_HEADER_BYTES])
{
    if (!reader || !reader->read_nibble || !header || memcmp(header, "HPCMEM2", 8) ||
        !valid_identity(header + 8) || !valid_capacity(read_le32(header + 76)) ||
        object < 0x80000u || object > 0x100000u - 10u - 2u * read_le32(header + 76))
        return -1;
    uint32_t prologue, length;
    if (!read_word(reader, object, 5, &prologue))
        return -1;
    if (prologue != 0x2a2cu)
        return 0;
    if (!read_word(reader, object + 5, 5, &length))
        return -1;
    if (length != 5u + 2u * read_le32(header + 76))
        return 0;
    for (unsigned i = 0; i < HPCLANG_RPL_RECOVERY_RESERVATION_HEADER_BYTES; ++i) {
        uint32_t byte;
        if (!read_word(reader, object + 10u + 2u * i, 2, &byte))
            return -1;
        if (byte != header[i])
            return 0;
    }
    return 1;
}

struct creation
{
    const struct hpclang_rpl_mutation_io *original;
    int attempted;
};

static int read_nibble(void *context, uint32_t address, uint8_t *value)
{
    struct creation *creation = context;
    return creation->original->read_nibble(creation->original->context, address, value);
}

static int write_nibble(void *context, uint32_t address, uint8_t value)
{
    struct creation *creation = context;
    creation->attempted = 1;
    return creation->original->write_nibble(creation->original->context, address, value);
}

static int valid_string(const struct hpclang_object *snapshot)
{
    if (!snapshot || snapshot->kind != HPCLANG_OBJECT_STRING ||
        snapshot->native_prologue != 0x2a2cu || snapshot->nibble_count < 10u ||
        snapshot->nibble_count > 0x100004u || (snapshot->nibble_count & 1u))
        return 0;
    uint32_t header[2] = {0, 0};
    for (unsigned i = 0; i < 10u; ++i) {
        uint8_t value;
        if (!hpclang_object_snapshot_read_nibble(snapshot, i, &value) || value > 15u)
            return 0;
        header[i / 5u] |= (uint32_t)value << (4u * (i % 5u));
    }
    return header[0] == 0x2a2cu && header[1] == snapshot->nibble_count - 5u;
}

/* Keep the directory entry out of the allocator's deeper call chain. */
__attribute__((noinline)) static int
verify_created(const struct hpclang_rpl_reader *reader, const uint8_t *name, uint32_t name_length,
               const struct hpclang_object *snapshot,
               struct hpclang_rpl_recovery_reservation *created)
{
    /* Creating USEROB storage can move HOME. Never reuse its old coordinate. */
    uint32_t home;
    struct hpclang_rpl_directory_entry_view entry;
    if (hpclang_rpl_store_home_object(reader, &home) ||
        hpclang_rpl_directory_find(reader, home, name, name_length, &entry) ||
        entry.prologue != 0x2a2cu || entry.object_address >= 0x100000u ||
        snapshot->nibble_count > 0x100000u - entry.object_address)
        return HPCLANG_RPL_MUTATION_INCOMPLETE;
    for (uint32_t i = 0; i < snapshot->nibble_count; ++i) {
        uint8_t actual, expected;
        if (!reader->read_nibble(reader->context, entry.object_address + i, &actual) ||
            !hpclang_object_snapshot_read_nibble(snapshot, i, &expected) || actual != expected)
            return HPCLANG_RPL_MUTATION_INCOMPLETE;
    }
    *created = (struct hpclang_rpl_recovery_reservation){entry.object_address,
                                                         (snapshot->nibble_count - 10u) / 2u};
    return HPCLANG_RPL_MUTATION_OK;
}

int hpclang_rpl_recovery_reservation_create(const struct hpclang_rpl_mutation_io *io,
                                            const uint8_t *name, uint32_t name_length,
                                            const struct hpclang_object *snapshot,
                                            struct hpclang_rpl_recovery_reservation *created)
{
    if (!io || !io->read_nibble || !io->write_nibble || !created || !name || !name_length ||
        name_length >= HPCLANG_RPL_STORE_NAME_CAPACITY)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    if (!valid_string(snapshot))
        return HPCLANG_RPL_MUTATION_MALFORMED;
    struct creation creation = {io, 0};
    struct hpclang_rpl_mutation_io observed = *io;
    observed.context = &creation;
    observed.read_nibble = read_nibble;
    observed.write_nibble = write_nibble;
    const struct hpclang_rpl_reader reader = {read_nibble, &creation};
    uint32_t home;
    if (hpclang_rpl_store_home_object(&reader, &home))
        return HPCLANG_RPL_MUTATION_IO;
    int result = hpclang_rpl_mutation_store_create(&observed, home, name, name_length, snapshot);
    if (result)
        return creation.attempted ? HPCLANG_RPL_MUTATION_INCOMPLETE : result;
    return verify_created(&reader, name, name_length, snapshot, created);
}
