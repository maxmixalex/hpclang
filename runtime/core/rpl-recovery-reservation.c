#include "rpl-recovery-reservation.h"
#include "rpl-object-store.h"

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
