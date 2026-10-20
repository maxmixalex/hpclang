#include "rpl-flash-write.h"
#include "rpl-flash-record.h"
#include "rpl-backup.h"
#include <assert.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t record[] = {0x53, 0x3e, 0x00, 0x10, 0x62, 0x2b, 0x10, 0x03, 0x00, 0x08, 0x46,
                                 0x4c, 0x41, 0x53, 0x48, 0x4f, 0x4e, 0x45, 0x08, 0x14, 0x26, 0x90,
                                 0x00, 0x00, 0x87, 0x09, 0x11, 0x29, 0x00, 0xc8, 0xd1, 0xff};
static uint8_t flash[6u * 0x20000u];
static unsigned writes;
static uint32_t fail_address;
static unsigned write_failures;
static unsigned fail_write_at;
static int fail_read_after_write;
static unsigned power_cut_at = UINT32_MAX;
static uint16_t power_cut_mask;
static jmp_buf power_cut;
static const uint32_t base = 0x140000u;

static int read16(void *context, uint32_t address, uint16_t *value)
{
    (void)context;
    assert(!(address & 1u) && address >= base && address - base < sizeof flash);
    if (address == fail_address)
        return 0;
    if (fail_read_after_write && writes) {
        fail_read_after_write = 0;
        return 0;
    }
    uint32_t at = address - base;
    *value = (uint16_t)(flash[at] | (uint16_t)flash[at + 1u] << 8u);
    return 1;
}

static int program16(void *context, uint32_t address, uint16_t value)
{
    uint16_t old;
    assert(read16(context, address, &old));
    assert((old & value) == value);
    if (writes == power_cut_at) {
        uint16_t partial = old & (value | (uint16_t)~power_cut_mask);
        flash[address - base] = (uint8_t)partial;
        flash[address - base + 1u] = (uint8_t)(partial >> 8u);
        longjmp(power_cut, 1); /* No returned error, readback or caller cleanup. */
    }
    if (write_failures || writes == fail_write_at) {
        if (write_failures)
            --write_failures;
        flash[address - base] &= (uint8_t)value;
        ++writes;
        return 0;
    }
    flash[address - base] = (uint8_t)value;
    flash[address - base + 1u] = (uint8_t)(value >> 8u);
    ++writes;
    return 1;
}

static void init(void)
{
    memset(flash, 0xff, sizeof flash);
    for (uint32_t i = 0; i < 6; ++i) {
        flash[i * 0x20000u + 0x100u] = 0xf0;
        flash[i * 0x20000u + 0x101u] = (uint8_t)(i + 8u);
    }
    writes = 0;
    write_failures = 0;
    fail_write_at = UINT32_MAX;
    fail_address = UINT32_MAX;
    fail_read_after_write = 0;
    power_cut_at = UINT32_MAX;
}

static void interrupted_tails(void)
{
    init();
    for (uint32_t index = 0; index < 6u; ++index) {
        uint32_t first = index * 0x20000u + 0x105u;
        memcpy(flash + first, record, sizeof record);
        /* The next body's write stopped before status publication. */
        memcpy(flash + first + sizeof record + 1u, record + 1u, 12u);
    }
}

static void recover_port_tails(const struct hpclang_rpl_flash_io *io)
{
    uint8_t *before = malloc(sizeof flash);
    assert(before);
    interrupted_tails();
    memcpy(before, flash, sizeof flash);
    assert(!hpclang_rpl_flash_recover_tails(io));
    unsigned complete_writes = writes;
    assert(complete_writes);
    /* Every original live record, header and partial body is preserved; only
     * the four-byte discard header may change in each interrupted slot. */
    for (uint32_t index = 0; index < 6u; ++index) {
        uint32_t first = index * 0x20000u, slot = first + 0x105u + sizeof record;
        assert(flash[slot] == 0);
        assert(!memcmp(flash + first, before + first, slot - first));
        assert(!memcmp(flash + slot + 4u, before + slot + 4u, first + 0x20000u - slot - 4u));
    }
    assert(!hpclang_rpl_flash_recover_tails(io) && writes == complete_writes);
    for (uint32_t index = 0; index < 6u; ++index) {
        for (unsigned failure = 0; failure < 4u; ++failure) {
            interrupted_tails();
            uint32_t bank = index * 0x20000u;
            if (failure == 0)
                flash[bank + 0x101u] ^= 1u;
            else if (failure == 1)
                flash[bank + 0x105u + sizeof record] = 0x7fu;
            else
                fail_address = base + bank + (failure == 2 ? 0x1fffeu : 0x100u);
            memcpy(before, flash, sizeof flash);
            int expected = failure >= 2 ? HPCLANG_RPL_FLASH_FAILED : HPCLANG_RPL_FLASH_DIRTY;
            assert(hpclang_rpl_flash_recover_tails(io) == expected);
            assert(!writes && !memcmp(before, flash, sizeof flash));
        }
    }
    /* Retry after each returned programming failure, including failures in
     * later banks after earlier tails have already been recovered. */
    for (unsigned cut = 0; cut < complete_writes; ++cut) {
        interrupted_tails();
        fail_write_at = cut;
        assert(hpclang_rpl_flash_recover_tails(io) == HPCLANG_RPL_FLASH_DIRTY);
        fail_write_at = UINT32_MAX;
        assert(!hpclang_rpl_flash_recover_tails(io));
        for (uint32_t index = 0; index < 6u; ++index)
            assert(!memcmp(flash + index * 0x20000u + 0x105u, record, sizeof record));
    }
    free(before);
}

static void prepare_mutation(const struct hpclang_rpl_flash_io *io)
{
    assert(hpclang_rpl_flash_prepare_write(NULL) == HPCLANG_RPL_FLASH_INVALID);
    interrupted_tails();
    flash[5u * 0x20000u + 0x101u] = 0;
    assert(hpclang_rpl_flash_prepare_write(io) == HPCLANG_RPL_FLASH_DIRTY && !writes);
    interrupted_tails();
    fail_address = base + 5u * 0x20000u + 0x100u;
    assert(hpclang_rpl_flash_prepare_write(io) == HPCLANG_RPL_FLASH_FAILED && !writes);
    for (unsigned fault = 0; fault < 2; ++fault) {
        interrupted_tails();
        if (fault)
            fail_read_after_write = 1;
        else
            write_failures = 3;
        assert(!hpclang_rpl_flash_prepare_write(io));
        assert(!write_failures && !fail_read_after_write);
        for (uint32_t index = 0; index < 6; ++index) {
            uint32_t first = index * 0x20000u + 0x105u;
            assert(!memcmp(flash + first, record, sizeof record));
            assert(!flash[first + sizeof record]);
        }
        unsigned completed = writes;
        assert(!hpclang_rpl_flash_prepare_write(io) && writes == completed);
    }
}

static uint32_t fingerprint(uint32_t end)
{
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < end; ++i)
        hash = (hash ^ flash[i]) * 16777619u;
    return hash;
}

static void fill_bank(uint32_t index)
{
    uint32_t at = index * 0x20000u + 0x105u;
    uint32_t extent = ((0x40000u - 0x20au - 2u) & ~3u) - 2u;
    flash[at] = 0;
    flash[at + 1u] = (uint8_t)extent;
    flash[at + 2u] = (uint8_t)(extent >> 8u);
    flash[at + 3u] = (uint8_t)(0xf0u | (extent >> 16u));
}

static int string_nibble(void *context, uint32_t address, uint8_t *value)
{
    uint32_t length = *(uint32_t *)context;
    if (address >= 10u + length * 2u)
        return 0;
    *value = address < 5u    ? (0x02a2cu >> (4u * address)) & 15u
             : address < 10u ? ((5u + length * 2u) >> (4u * (address - 5u))) & 15u
                             : (0x78u >> (4u * (address & 1u))) & 15u;
    return 1;
}

static int packed_nibble(void *context, uint32_t address, uint8_t *value)
{
    const uint8_t *data = context;
    *value = (data[address / 2u] >> (4u * (address & 1u))) & 15u;
    return 1;
}

static uint8_t *named_string_record(uint32_t length, const char *name, uint32_t *bytes)
{
    const struct hpclang_rpl_reader reader = {string_nibble, &length};
    uint8_t *backup, *result;
    uint32_t span;
    assert(!hpclang_rpl_backup_encode(&reader, 0, 10u + length * 2u, (const uint8_t *)name,
                                      (uint32_t)strlen(name), &backup, &span));
    const struct hpclang_rpl_reader packed = {packed_nibble, backup};
    assert(!hpclang_rpl_flash_backup_record(&packed, 0, span, &result, bytes));
    free(backup);
    return result;
}

static uint8_t *string_record(uint32_t length, uint32_t *bytes)
{
    return named_string_record(length, "FLASHONE", bytes);
}

static void cut_append(const struct hpclang_rpl_flash_io *io, uint32_t slot)
{
    if (!setjmp(power_cut)) {
        (void)hpclang_rpl_flash_append(io, slot, record, sizeof record);
        assert(0 && "programming cut was not reached");
    }
    power_cut_at = UINT32_MAX;
}

static void restart_after_append(const struct hpclang_rpl_flash_io *io)
{
    uint32_t keep_bytes;
    uint8_t *keep = named_string_record(3, "KEEP", &keep_bytes);
    uint8_t *interrupted = malloc(sizeof flash);
    assert(interrupted);
    const uint32_t slot = 0x105u + keep_bytes;
    init();
    memcpy(flash + 0x105u, keep, keep_bytes);
    assert(!hpclang_rpl_flash_append(io, base + slot, record, sizeof record));
    unsigned count = writes;
    const uint16_t masks[] = {0, 0x00ff, 0x0fff, 0xffff};
    unsigned recovered = 0, discarded = 0, committed = 0;
    for (unsigned cut = 0; cut < count; ++cut) {
        for (unsigned m = 0; m < sizeof masks / sizeof masks[0]; ++m) {
            init();
            memcpy(flash + 0x105u, keep, keep_bytes);
            power_cut_at = cut;
            power_cut_mask = masks[m];
            cut_append(io, base + slot);
            memcpy(interrupted, flash, sizeof flash);
            /* A new invocation gets no append slot/extent/expected record.
             * Only the persistent Flash bytes survive into tail recovery. */
            writes = 0;
            int result = hpclang_rpl_flash_recover_tails(io);
            uint8_t status = interrupted[slot];
            if (status == 0xff) {
                assert(!result);
                ++recovered;
                assert(!memcmp(flash, interrupted, slot));
                assert(
                    !memcmp(flash + slot + 4u, interrupted + slot + 4u, sizeof flash - slot - 4u));
            } else if (status == 0x53) {
                assert(!result && !writes && !memcmp(flash, interrupted, sizeof flash));
                assert(!memcmp(flash + slot, record, sizeof record));
                ++committed;
            } else {
                assert(!result && flash[slot] == 0);
                assert(!memcmp(flash, interrupted, slot));
                assert(
                    !memcmp(flash + slot + 1u, interrupted + slot + 1u, sizeof flash - slot - 1u));
                ++discarded;
            }
            assert(!memcmp(flash + 0x105u, keep, keep_bytes));
        }
    }
    assert(recovered && committed && discarded);
    free(interrupted);
    free(keep);
}

static void restart_failed_append_cleanup(const struct hpclang_rpl_flash_io *io)
{
    const uint32_t slot = 0x105u + sizeof record;
    const uint16_t masks[] = {0, 0x00ff, 0xff00, 0xffff};
    uint8_t *before = malloc(sizeof flash);
    assert(before);
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    assert(!hpclang_rpl_flash_append(io, base + slot, record, sizeof record));
    unsigned append_steps = writes;
    for (unsigned failure = 0; failure < append_steps; ++failure) {
        init();
        memcpy(flash + 0x105u, record, sizeof record);
        fail_write_at = failure;
        assert(hpclang_rpl_flash_append(io, base + slot, record, sizeof record) ==
               HPCLANG_RPL_FLASH_FAILED);
        unsigned steps = writes;
        assert(steps > failure + 1u);
        for (unsigned cut = failure + 1u; cut < steps; ++cut) {
            for (unsigned i = 0; i < sizeof masks / sizeof masks[0]; ++i) {
                init();
                memcpy(flash + 0x105u, record, sizeof record);
                fail_write_at = failure;
                power_cut_at = cut;
                power_cut_mask = masks[i];
                cut_append(io, base + slot);
                memcpy(before, flash, sizeof flash);
                fail_write_at = UINT32_MAX;
                assert(flash[slot] != 0x53);
                assert(!hpclang_rpl_flash_recover_tails(io));
                assert(flash[slot] == 0);
                assert(!memcmp(flash, before, slot));
                assert(!memcmp(flash + slot + 4u, before + slot + 4u, sizeof flash - slot - 4u));
            }
        }
    }
    free(before);
}

static void recover_partial_backups(const struct hpclang_rpl_flash_io *io)
{
    uint8_t *before = malloc(sizeof flash);
    assert(before);
    for (unsigned status = 1; status < 255; ++status) {
        if (status == 0x53)
            continue;
        init();
        memcpy(flash + 0x105u, record, sizeof record);
        flash[0x105u] = (uint8_t)status;
        memcpy(flash + 0x105u + sizeof record, record, sizeof record);
        memcpy(before, flash, sizeof flash);
        int result = hpclang_rpl_flash_recover_tails(io);
        assert(!result && flash[0x105u] == 0);
        assert(!memcmp(before, flash, 0x105u));
        assert(!memcmp(before + 0x106u, flash + 0x106u, sizeof flash - 0x106u));
        unsigned completed = writes;
        assert(!hpclang_rpl_flash_recover_tails(io) && writes == completed);
    }
    /* Reject damaged CRC, padding, metadata and a damaged later bank before
     * changing even the earlier, otherwise recoverable backup. */
    for (unsigned fault = 0; fault < 5; ++fault) {
        init();
        memcpy(flash + 0x105u, record, sizeof record);
        flash[0x105u] = 0x73;
        if (fault == 0)
            flash[0x105u + 29u] ^= 1u;
        else if (fault == 1)
            flash[0x105u + sizeof record - 1u] = 0;
        else if (fault == 2)
            flash[0x108u] ^= 0x10u;
        else if (fault == 3)
            flash[5u * 0x20000u + 0x101u] = 0;
        else
            fail_address = base + 0x120u;
        memcpy(before, flash, sizeof flash);
        assert(hpclang_rpl_flash_recover_tails(io) ==
               (fault == 4 ? HPCLANG_RPL_FLASH_FAILED : HPCLANG_RPL_FLASH_DIRTY));
        assert(!writes && !memcmp(before, flash, sizeof flash));
    }
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    flash[0x105u] = 0x73;
    write_failures = 1;
    assert(hpclang_rpl_flash_recover_tails(io) == HPCLANG_RPL_FLASH_DIRTY);
    assert(!hpclang_rpl_flash_recover_tails(io) && flash[0x105u] == 0);
    assert(!memcmp(flash + 0x106u, record + 1u, sizeof record - 1u));
    free(before);
}

static void cut_discard(const struct hpclang_rpl_flash_io *io, uint32_t address)
{
    if (!setjmp(power_cut))
        (void)hpclang_rpl_flash_discard_record(io, address, record, sizeof record);
    power_cut_at = UINT32_MAX;
}

static void cut_recovery(const struct hpclang_rpl_flash_io *io)
{
    if (!setjmp(power_cut))
        (void)hpclang_rpl_flash_recover_tails(io);
    power_cut_at = UINT32_MAX;
}

static void recovery_never_publishes(const struct hpclang_rpl_flash_io *io)
{
    const uint8_t statuses[] = {0xff, 0x73, 0x03, 0x72, 0x20, 0x80};
    const uint8_t masks[] = {0, 1, 0x20, 0x53, 0xfe, 0xff};
    for (unsigned i = 0; i < sizeof statuses; ++i) {
        for (unsigned step = 0; step < 2; ++step) {
            for (unsigned j = 0; j < sizeof masks; ++j) {
                init();
                memcpy(flash + 0x105u, record, sizeof record);
                flash[0x105u] = statuses[i];
                power_cut_at = step;
                power_cut_mask = (uint16_t)masks[j] << 8u;
                cut_recovery(io);
                assert(flash[0x105u] != 0x53);
                unsigned preserved = statuses[i] == 0xff ? 4u : 1u;
                assert(!memcmp(flash + 0x105u + preserved, record + preserved,
                               sizeof record - preserved));
                /* Restart solely from Flash, then interrupt that recovery too. */
                writes = 0;
                power_cut_at = 0;
                power_cut_mask = 0x2000;
                cut_recovery(io);
                assert(flash[0x105u] != 0x53);
                assert(!hpclang_rpl_flash_recover_tails(io));
                assert(flash[0x105u] == 0);
                assert(!memcmp(flash + 0x105u + preserved, record + preserved,
                               sizeof record - preserved));
            }
        }
    }
    /* A discard marker cannot consume a later record or an unverified extent. */
    for (unsigned fault = 0; fault < 3; ++fault) {
        init();
        memcpy(flash + 0x105u, record, sizeof record);
        flash[0x105u] = 0xfe;
        flash[0x108u] &= 15u;
        if (fault == 0)
            memcpy(flash + 0x105u + sizeof record, record, sizeof record);
        else if (fault == 1)
            flash[0x106u] = 0x1e;
        else
            fail_address = base + 0x1fffeu;
        uint32_t before = fingerprint(sizeof flash);
        assert(hpclang_rpl_flash_recover_tails(io) ==
               (fault == 2 ? HPCLANG_RPL_FLASH_FAILED : HPCLANG_RPL_FLASH_DIRTY));
        assert(!writes && fingerprint(sizeof flash) == before);
    }
}

static void discard_never_publishes(const struct hpclang_rpl_flash_io *io)
{
    /* Exhaust every subset of bits cleared by either status programming step,
     * in both halves of the physical word. No caller cleanup runs after a cut. */
    for (unsigned alignment = 0; alignment < 2; ++alignment) {
        uint32_t slot = 0x104u + alignment;
        for (unsigned status = 0; status <= 255; ++status) {
            if (status == 0x53 || (status & 0x53u) != 0x53u)
                continue;
            for (unsigned step = 0; step < (status == 0xff ? 3u : 2u); ++step) {
                for (unsigned mask = 0; mask <= 255; ++mask) {
                    init();
                    memcpy(flash + slot, record, sizeof record);
                    flash[slot] = (uint8_t)status;
                    power_cut_at = step;
                    power_cut_mask = (uint16_t)(mask << (alignment * 8u));
                    cut_discard(io, base + slot);
                    assert(flash[slot] != 0x53);
                    unsigned preserved = status == 0xff ? 4u : 1u;
                    assert(!memcmp(flash + slot + preserved, record + preserved,
                                   sizeof record - preserved));
                    assert(flash[slot - 1u] == 0xff && flash[slot + sizeof record] == 0xff);
                }
            }
        }
    }
}

static void restart_partial_tail(const struct hpclang_rpl_flash_io *io)
{
    const uint16_t masks[] = {0, 0x00ff, 0xff00, 0x5555, 0xaaaa, 0xffff};
    const uint32_t slot = 0x105u + sizeof record;
    uint8_t *before = malloc(sizeof flash);
    assert(before);
    for (unsigned shape = 0; shape < 2; ++shape) {
        init();
        memcpy(flash + 0x105u, record, sizeof record);
        if (shape)
            memcpy(flash + slot + 1u, record + 1u, 12u);
        else
            flash[slot + 25u] = 0;
        memcpy(before, flash, sizeof flash);
        assert(!hpclang_rpl_flash_recover_tails(io));
        unsigned steps = writes;
        for (unsigned cut = 0; cut < steps; ++cut) {
            for (unsigned i = 0; i < sizeof masks / sizeof masks[0]; ++i) {
                init();
                memcpy(flash, before, sizeof flash);
                power_cut_at = cut;
                power_cut_mask = masks[i];
                cut_recovery(io);
                assert(flash[slot] != 0x53);
                /* A second interruption must also be resumable without any
                 * saved allocation plan or original object bytes. */
                writes = 0;
                power_cut_at = 0;
                power_cut_mask = masks[(i + 1u) % (sizeof masks / sizeof masks[0])];
                cut_recovery(io);
                assert(flash[slot] != 0x53);
                assert(!hpclang_rpl_flash_recover_tails(io));
                assert(flash[slot] == 0 && !(flash[slot + 3u] & 0xf0));
                assert(!memcmp(before, flash, slot));
                assert(!memcmp(before + slot + 4u, flash + slot + 4u, sizeof flash - slot - 4u));
                unsigned completed = writes;
                assert(!hpclang_rpl_flash_recover_tails(io) && completed == writes);
            }
        }
    }
    free(before);
}

static void cut_replace(const struct hpclang_rpl_flash_io *io, const uint8_t *replacement,
                        uint32_t bytes)
{
    struct hpclang_rpl_flash_replacement transaction;
    if (!setjmp(power_cut)) {
        (void)hpclang_rpl_flash_replace_backup(io, replacement, bytes, &transaction);
        assert(0 && "replacement cut was not reached");
    }
    power_cut_at = UINT32_MAX;
}

static void ambiguous_replacement_restart(const struct hpclang_rpl_flash_io *io)
{
    uint32_t bytes;
    uint8_t *replacement = string_record(9, &bytes);
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    struct hpclang_rpl_flash_replacement transaction;
    /* A legacy duplicate pair has no intent. Neither valid backup alone
     * authorizes deleting the other, even with the new replacement writer. */
    memcpy(flash + 0x105u + sizeof record, replacement, bytes);
    assert(!memcmp(flash + 0x105u, record, sizeof record));
    assert(!memcmp(flash + 0x105u + sizeof record, replacement, bytes));
    uint32_t before = fingerprint(sizeof flash);
    writes = 0;
    assert(!hpclang_rpl_flash_prepare_write(io));
    assert(hpclang_rpl_flash_replace_backup(io, replacement, bytes, &transaction) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(!writes && fingerprint(sizeof flash) == before);
    free(replacement);
}

static void journaled_replacement_restart(const struct hpclang_rpl_flash_io *io)
{
    uint32_t bytes;
    uint8_t *replacement = string_record(9, &bytes);
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    struct hpclang_rpl_flash_replace_plan plan;
    assert(!hpclang_rpl_flash_plan_replace(io, replacement, bytes, &plan));
    struct hpclang_rpl_flash_replacement transaction;
    assert(!hpclang_rpl_flash_replace_backup(io, replacement, bytes, &transaction));
    unsigned steps = writes;
    const uint16_t masks[] = {0, 0x00ff, 0xff00, 0x5555, 0xaaaa, 0xffff};
    for (unsigned step = 0; step < steps; ++step) {
        for (unsigned mask = 0; mask < sizeof masks / sizeof masks[0]; ++mask) {
            init();
            memcpy(flash + 0x105u, record, sizeof record);
            power_cut_at = step;
            power_cut_mask = masks[mask];
            cut_replace(io, replacement, bytes);
            assert(!hpclang_rpl_flash_recover_tails(io));
            uint32_t next = plan.intent.new_slot - base;
            if (flash[next] == 0x53u) {
                assert(!flash[0x105u]);
                assert(!memcmp(flash + next, replacement, bytes));
            } else {
                assert(flash[next] == 0 || flash[next] == 0xff);
                assert(!memcmp(flash + 0x105u, record, sizeof record));
            }
            assert(!memcmp(flash + 0x106u, record + 1u, sizeof record - 1u));
            unsigned recovered = writes;
            assert(!hpclang_rpl_flash_recover_tails(io) && writes == recovered);
        }
    }
    free(replacement);
}

static void replace_backups(const struct hpclang_rpl_flash_io *io)
{
    struct hpclang_rpl_flash_replacement transaction = {UINT32_MAX, UINT32_MAX, 0};
    init();
    assert(hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction) ==
           HPCLANG_RPL_FLASH_NOT_FOUND);
    assert(!writes && transaction.new_slot == UINT32_MAX);
    memcpy(flash + 0x105u, record, sizeof record);
    assert(!hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction));
    unsigned complete_writes = writes;
    assert(transaction.committed && transaction.old_slot == base + 0x105u &&
           transaction.new_slot == base + 0x105u + sizeof record + HPCLANG_RPL_FLASH_INTENT_BYTES);
    assert(flash[0x105u] == 0 && !memcmp(flash + 0x106u, record + 1u, sizeof record - 1u));
    assert(!memcmp(flash + transaction.new_slot - base, record, sizeof record));
    for (unsigned cut = 0; cut < complete_writes; ++cut) {
        init();
        memcpy(flash + 0x105u, record, sizeof record);
        fail_write_at = cut;
        int result = hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction);
        if (result == HPCLANG_RPL_FLASH_DIRTY)
            result = hpclang_rpl_flash_finish_replace(io, &transaction);
        if (result == HPCLANG_RPL_FLASH_WRITTEN) {
            assert(transaction.committed);
            assert(flash[0x105u] == 0);
            assert(!memcmp(flash + transaction.new_slot - base, record, sizeof record));
        } else {
            assert(result == HPCLANG_RPL_FLASH_FAILED);
            assert(!memcmp(flash + 0x105u, record, sizeof record));
            assert(flash[transaction.new_slot - base] == 0 ||
                   flash[transaction.new_slot - base] == 0xff);
        }
        assert(!memcmp(flash + 0x106u, record + 1u, sizeof record - 1u));
    }
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    write_failures = 100;
    assert(hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(!transaction.committed && !memcmp(flash + 0x105u, record, sizeof record));
    write_failures = 0;
    assert(hpclang_rpl_flash_finish_replace(io, &transaction) == HPCLANG_RPL_FLASH_FAILED);
    /* Old and new records may reside in different native banks. */
    init();
    memcpy(flash + 5u * 0x20000u + 0x105u, record, sizeof record);
    assert(!hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction));
    assert(transaction.new_slot == base + 0x105u + HPCLANG_RPL_FLASH_INTENT_BYTES);
    assert(transaction.old_slot == base + 5u * 0x20000u + 0x105u);
    assert(flash[5u * 0x20000u + 0x105u] == 0);
    assert(!memcmp(flash + transaction.new_slot - base, record, sizeof record));
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    memcpy(flash + 5u * 0x20000u + 0x105u, record, sizeof record);
    transaction.new_slot = UINT32_MAX;
    uint32_t before = fingerprint(sizeof flash);
    assert(hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(!writes && transaction.new_slot == UINT32_MAX && fingerprint(sizeof flash) == before);
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    flash[5u * 0x20000u + 0x1ffffu] = 0;
    before = fingerprint(sizeof flash);
    assert(hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(!writes && transaction.new_slot == UINT32_MAX && fingerprint(sizeof flash) == before);
    /* Grow and shrink an actual value, retaining every byte of each old body. */
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    uint32_t old_slot = 0x105u;
    for (uint32_t length = 32u;; length = 0) {
        uint32_t bytes;
        uint8_t *replacement = string_record(length, &bytes);
        struct hpclang_rpl_flash_replace_plan plan;
        assert(!hpclang_rpl_flash_plan_replace(io, replacement, bytes, &plan));
        uint32_t next_slot = plan.intent.new_slot - base, journal = plan.intent_slot - base;
        before = fingerprint(sizeof flash);
        assert(!hpclang_rpl_flash_intent_encode(io->first, io->end, plan.intent_slot, &plan.intent,
                                                flash + journal, HPCLANG_RPL_FLASH_INTENT_BYTES));
        flash[journal] = 0;
        memcpy(flash + next_slot, replacement, bytes);
        flash[old_slot] = 0;
        uint32_t expected = fingerprint(sizeof flash);
        memset(flash + next_slot, 255, bytes);
        memset(flash + journal, 255, HPCLANG_RPL_FLASH_INTENT_BYTES);
        flash[old_slot] = 0x53;
        assert(fingerprint(sizeof flash) == before);
        assert(!hpclang_rpl_flash_replace_backup(io, replacement, bytes, &transaction));
        assert(fingerprint(sizeof flash) == expected);
        old_slot = next_slot;
        free(replacement);
        if (!length)
            break;
    }
    /* Filling the remaining tails must not delete the old value to make room. */
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    for (uint32_t index = 0; index < 6; ++index) {
        uint32_t at = index * 0x20000u + 0x105u + (index ? 0u : sizeof record);
        uint32_t extent = (((index + 1u) * 0x20000u - at - 1u) * 2u & ~3u) - 2u;
        flash[at] = 0;
        flash[at + 1u] = (uint8_t)extent;
        flash[at + 2u] = (uint8_t)(extent >> 8u);
        flash[at + 3u] = (uint8_t)(0xf0u | (extent >> 16u));
    }
    transaction.new_slot = UINT32_MAX;
    before = fingerprint(sizeof flash);
    assert(hpclang_rpl_flash_replace_backup(io, record, sizeof record, &transaction) ==
           HPCLANG_RPL_FLASH_NOSPACE);
    assert(!writes && transaction.new_slot == UINT32_MAX && fingerprint(sizeof flash) == before);
}

static void accept_legacy_padding(const struct hpclang_rpl_flash_io *io)
{
    /* Native HP50g `987 :2:BB STO`: the final three bytes are padding.
     * Older SDK encoders used only one padding byte for this object. */
    uint8_t encoded[] = {0x53, 0x36, 0x00, 0x10, 0x62, 0x2b, 0x50, 0x02, 0x00, 0x02,
                         0x42, 0x42, 0x02, 0x14, 0x26, 0x90, 0x00, 0x00, 0x87, 0x09,
                         0x11, 0x29, 0x00, 0x88, 0x9d, 0xff, 0xff, 0xff};
    for (unsigned legacy = 0; legacy < 2; ++legacy) {
        init();
        uint32_t size = sizeof encoded - 2 * legacy;
        encoded[1] = (uint8_t)(size * 2 - 2);
        assert(hpclang_rpl_flash_create_backup(io, encoded, size, NULL) == 0);
        assert(!memcmp(flash + 0x105, encoded, size));
        assert(hpclang_rpl_flash_finish_commit(io, base + 0x105, encoded, size) == 0);
        assert(hpclang_rpl_flash_discard_record(io, base + 0x105, encoded, size) == 0);
        assert(flash[0x105] == 0);
    }
}

static void remove_backups(const struct hpclang_rpl_flash_io *io)
{
    const uint8_t *name = (const uint8_t *)"FLASHONE";
    uint32_t attempted = UINT32_MAX;
    init();
    assert(hpclang_rpl_flash_remove_backup(io, name, 8, &attempted) == HPCLANG_RPL_FLASH_NOT_FOUND);
    assert(!writes && attempted == UINT32_MAX);
    assert(hpclang_rpl_flash_remove_backup(io, NULL, 1, &attempted) == HPCLANG_RPL_FLASH_INVALID);
    for (uint32_t bank = 0; bank < 6; ++bank) {
        init();
        uint32_t slot = bank * 0x20000u + 0x105u;
        memcpy(flash + slot, record, sizeof record);
        /* Compare the entire medium to the intended single-byte change. */
        flash[slot] = 0;
        uint32_t expected = fingerprint(sizeof flash);
        flash[slot] = 0x53;
        assert(!hpclang_rpl_flash_remove_backup(io, name, 8, &attempted));
        assert(attempted == base + slot && writes == 1);
        assert(fingerprint(sizeof flash) == expected);
        assert(hpclang_rpl_flash_remove_backup(io, name, 8, NULL) == HPCLANG_RPL_FLASH_NOT_FOUND);
        assert(writes == 1);
    }
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    memcpy(flash + 5u * 0x20000u + 0x105u, record, sizeof record);
    uint32_t before = fingerprint(sizeof flash);
    attempted = UINT32_MAX;
    assert(hpclang_rpl_flash_remove_backup(io, name, 8, &attempted) == HPCLANG_RPL_FLASH_DIRTY);
    assert(!writes && attempted == UINT32_MAX && fingerprint(sizeof flash) == before);
    for (uint32_t bank = 0; bank < 6; ++bank) {
        init();
        memcpy(flash + 0x105u, record, sizeof record);
        flash[bank * 0x20000u + 0x1ffffu] = 0;
        before = fingerprint(sizeof flash);
        assert(hpclang_rpl_flash_remove_backup(io, name, 8, &attempted) == HPCLANG_RPL_FLASH_DIRTY);
        assert(!writes && attempted == UINT32_MAX && fingerprint(sizeof flash) == before);
        init();
        memcpy(flash + 0x105u, record, sizeof record);
        fail_address = base + bank * 0x20000u + 0x1fffeu;
        assert(hpclang_rpl_flash_remove_backup(io, name, 8, &attempted) ==
               HPCLANG_RPL_FLASH_FAILED);
        assert(!writes && attempted == UINT32_MAX);
    }
    init();
    memcpy(flash + 0x105u, record, sizeof record);
    write_failures = 1;
    assert(hpclang_rpl_flash_remove_backup(io, name, 8, &attempted) == HPCLANG_RPL_FLASH_DIRTY);
    assert(attempted == base + 0x105u);
    assert(!hpclang_rpl_flash_finish_remove(io, attempted));
    assert(flash[0x105u] == 0 && !memcmp(flash + 0x106u, record + 1u, sizeof record - 1u));
}

struct catalog_image
{
    uint8_t nibbles[160];
    uint32_t fail_at;
};
static int catalog_read(void *context, uint32_t address, uint8_t *value)
{
    struct catalog_image *image = context;
    if (address >= sizeof image->nibbles || address == image->fail_at)
        return 0;
    *value = image->nibbles[address];
    return 1;
}
static void catalog_word(struct catalog_image *image, uint32_t address, uint32_t value,
                         unsigned width)
{
    assert(address + width <= sizeof image->nibbles);
    for (unsigned i = 0; i < width; ++i)
        image->nibbles[address + i] = (uint8_t)((value >> (4 * i)) & 15);
}
static void match_installations(const struct hpclang_rpl_flash_io *source, const uint8_t *library,
                                uint32_t bytes)
{
    struct hpclang_rpl_flash_io io = *source;
    io.program16 = NULL;
    struct catalog_image image = {.fail_at = UINT32_MAX};
    const struct hpclang_rpl_reader reader = {catalog_read, &image};
    const struct hpclang_rpl_flash_catalog catalog = {&reader, 120, 35, 0};
    for (uint32_t i = 0; i < 16; ++i)
        catalog_word(&image, 7 * i, 0x12345 + i * i, 5);
    catalog_word(&image, 120, 1, 3);
    catalog_word(&image, 123, 123, 3);
    catalog_word(&image, 126, 0x40222, 5);
    uint32_t slot = UINT32_MAX;
    for (uint32_t bank = 0; bank < 6; ++bank) {
        init();
        memcpy(flash + bank * 0x20000u + 0x105, library, bytes);
        catalog_word(&image, 131, 0x12345 + (bank + 8) * (bank + 8), 5);
        assert(!hpclang_rpl_flash_match_installation(&io, library, bytes, &catalog, &slot));
        assert(slot == base + bank * 0x20000u + 0x105 && !writes);
    }
    init();
    memcpy(flash + 0x105, library, bytes);
    catalog_word(&image, 131, 0x12345 + 64, 5);
    slot = UINT32_MAX;
    catalog_word(&image, 120, 0, 3);
    assert(hpclang_rpl_flash_match_installation(&io, library, bytes, &catalog, &slot) ==
           HPCLANG_RPL_FLASH_NOT_FOUND);
    catalog_word(&image, 120, 2, 3);
    memcpy(image.nibbles + 139, image.nibbles + 123, 16);
    assert(hpclang_rpl_flash_match_installation(&io, library, bytes, &catalog, &slot) ==
           HPCLANG_RPL_FLASH_DIRTY);
    catalog_word(&image, 120, 1, 3);
    catalog_word(&image, 126, 0x40223, 5);
    assert(hpclang_rpl_flash_match_installation(&io, library, bytes, &catalog, &slot) ==
           HPCLANG_RPL_FLASH_DIRTY);
    catalog_word(&image, 126, 0x40222, 5);
    catalog_word(&image, 131, 0x12345 + 81, 5);
    assert(hpclang_rpl_flash_match_installation(&io, library, bytes, &catalog, &slot) ==
           HPCLANG_RPL_FLASH_DIRTY);
    catalog_word(&image, 131, 0x12345 + 64, 5);
    catalog_word(&image, 7 * 9, 0x12345 + 64, 5);
    assert(hpclang_rpl_flash_match_installation(&io, library, bytes, &catalog, &slot) ==
           HPCLANG_RPL_FLASH_DIRTY);
    catalog_word(&image, 7 * 9, 0x12345 + 81, 5);
    image.fail_at = 7 * 15;
    assert(hpclang_rpl_flash_match_installation(&io, library, bytes, &catalog, &slot) ==
           HPCLANG_RPL_FLASH_FAILED);
    assert(slot == UINT32_MAX && !writes);
}

static void match_libraries(const struct hpclang_rpl_flash_io *source, const uint8_t *library,
                            const uint8_t *different, uint32_t bytes)
{
    struct hpclang_rpl_flash_io io = *source;
    io.program16 = NULL;
    uint32_t slot = UINT32_MAX;
    init();
    assert(hpclang_rpl_flash_match_library(&io, library, bytes, &slot) ==
           HPCLANG_RPL_FLASH_NOT_FOUND);
    assert(hpclang_rpl_flash_match_library(&io, record, sizeof record, &slot) ==
           HPCLANG_RPL_FLASH_INVALID);
    assert(hpclang_rpl_flash_match_library(&io, library, bytes, NULL) == HPCLANG_RPL_FLASH_INVALID);
    assert(hpclang_rpl_flash_create_library(&io, library, bytes, &slot) ==
           HPCLANG_RPL_FLASH_INVALID); /* Read-only scanning must not enable mutation. */
    assert(slot == UINT32_MAX && !writes);
    for (unsigned bank = 0; bank < 6; ++bank) {
        init();
        uint32_t at = bank * 0x20000u + 0x105u;
        memcpy(flash + at, library, bytes);
        uint32_t before = fingerprint(sizeof flash);
        assert(!hpclang_rpl_flash_match_library(&io, library, bytes, &slot));
        assert(slot == base + at && !writes && fingerprint(sizeof flash) == before);
        flash[at + 25] &= 15; /* Native STO may zero this non-object high nibble. */
        assert(!hpclang_rpl_flash_match_library(&io, library, bytes, &slot));
        assert(slot == base + at && !writes);
        slot = UINT32_MAX;
        flash[at] = 0;
        assert(hpclang_rpl_flash_match_library(&io, library, bytes, &slot) ==
               HPCLANG_RPL_FLASH_NOT_FOUND);
        assert(slot == UINT32_MAX && !writes);
    }
    init();
    memcpy(flash + 0x105, different, bytes); /* Valid CRC and ID, different content. */
    assert(hpclang_rpl_flash_match_library(&io, library, bytes, &slot) == HPCLANG_RPL_FLASH_DIRTY);
    assert(slot == UINT32_MAX && !writes);
    init();
    memcpy(flash + 0x105, library, bytes);
    memcpy(flash + 0x20105, library, bytes);
    assert(hpclang_rpl_flash_match_library(&io, library, bytes, &slot) == HPCLANG_RPL_FLASH_DIRTY);
    assert(slot == UINT32_MAX && !writes);
    init();
    /* A matching payload nested inside a tombstone is not an installation. */
    uint32_t extent = (bytes + 8) * 2 - 2;
    flash[0x105] = 0;
    flash[0x106] = (uint8_t)extent;
    flash[0x107] = (uint8_t)(extent >> 8);
    flash[0x108] = (uint8_t)(extent >> 16);
    memcpy(flash + 0x10d, library, bytes);
    assert(hpclang_rpl_flash_match_library(&io, library, bytes, &slot) ==
           HPCLANG_RPL_FLASH_NOT_FOUND);
    assert(slot == UINT32_MAX && !writes);
    init();
    memcpy(flash + 0x105, library, bytes);
    flash[0xa0110] = 0; /* A good early match must not conceal a dirty later bank. */
    assert(hpclang_rpl_flash_match_library(&io, library, bytes, &slot) == HPCLANG_RPL_FLASH_DIRTY);
    assert(slot == UINT32_MAX && !writes);
    init();
    memcpy(flash + 0x105, library, bytes);
    fail_address = base + 0xa0100;
    assert(hpclang_rpl_flash_match_library(&io, library, bytes, &slot) == HPCLANG_RPL_FLASH_FAILED);
    assert(slot == UINT32_MAX && !writes);
}

static void plan_libraries(const struct hpclang_rpl_flash_io *io, const uint8_t *library,
                           uint32_t bytes)
{
    struct hpclang_rpl_flash_io readonly = *io;
    readonly.program16 = NULL;
    static uint8_t original[sizeof flash];
    for (unsigned bank = 0; bank < 6; ++bank) {
        init();
        for (unsigned preceding = 0; preceding < bank; ++preceding)
            fill_bank(preceding);
        memcpy(flash + bank * 0x20000u + 0x105u, record, sizeof record);
        memcpy(original, flash, sizeof flash);
        uint32_t slot = UINT32_MAX;
        assert(!hpclang_rpl_flash_plan_library(&readonly, library, bytes, &slot));
        assert(slot == base + bank * 0x20000u + 0x105u + sizeof record);
        assert(!writes && !memcmp(original, flash, sizeof flash));
        uint32_t attempted = UINT32_MAX;
        assert(hpclang_rpl_flash_create_library(&readonly, library, bytes, &attempted) ==
               HPCLANG_RPL_FLASH_INVALID);
        assert(attempted == UINT32_MAX && !writes);
        assert(!hpclang_rpl_flash_create_library(io, library, bytes, &attempted));
        assert(attempted == slot);

        init();
        memcpy(flash + bank * 0x20000u + 0x105u, library, bytes);
        slot = UINT32_MAX;
        assert(hpclang_rpl_flash_plan_library(&readonly, library, bytes, &slot) ==
               HPCLANG_RPL_FLASH_EXISTS);
        assert(slot == UINT32_MAX && !writes);
        flash[sizeof flash - 1u] = 0;
        assert(hpclang_rpl_flash_plan_library(&readonly, library, bytes, &slot) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(slot == UINT32_MAX && !writes);

        init();
        fail_address = base + bank * 0x20000u + 0x1fffeu;
        assert(hpclang_rpl_flash_plan_library(&readonly, library, bytes, &slot) ==
               HPCLANG_RPL_FLASH_FAILED);
        assert(slot == UINT32_MAX && !writes);
    }
    init();
    memcpy(flash + 0x105u, library, bytes);
    flash[0x105u] = 0; /* Deleted content is not an owned installation to adopt. */
    uint32_t slot = UINT32_MAX;
    assert(!hpclang_rpl_flash_plan_library(&readonly, library, bytes, &slot));
    assert(slot == base + 0x105u + bytes && !writes);
    init();
    for (unsigned bank = 0; bank < 6; ++bank)
        fill_bank(bank);
    slot = UINT32_MAX;
    assert(hpclang_rpl_flash_plan_library(&readonly, library, bytes, &slot) ==
           HPCLANG_RPL_FLASH_NOSPACE);
    assert(slot == UINT32_MAX && !writes);
    assert(hpclang_rpl_flash_plan_library(&readonly, record, sizeof record, &slot) ==
           HPCLANG_RPL_FLASH_INVALID);
    assert(hpclang_rpl_flash_plan_library(&readonly, library, bytes, NULL) ==
           HPCLANG_RPL_FLASH_INVALID);
    assert(hpclang_rpl_flash_plan_library(NULL, library, bytes, &slot) ==
           HPCLANG_RPL_FLASH_INVALID);
    assert(slot == UINT32_MAX && !writes);
}

static void create_libraries(const struct hpclang_rpl_flash_io *io)
{
    /* Native library 123, with the unused high nibble left erased by SDK. */
    const uint8_t library[] = {0x53, 0x36, 0x00, 0x10, 0x40, 0x2b, 0x60, 0x02, 0x00, 0x01,
                               0x58, 0x01, 0x7b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0xd0, 0xc5, 0xf2, 0xff, 0xff};
    plan_libraries(io, library, sizeof library);
    init();
    memcpy(flash + 0x105u, library, sizeof library);
    flash[0x105u] = 0x73;
    assert(hpclang_rpl_flash_recover_tails(io) == HPCLANG_RPL_FLASH_DIRTY && !writes);
    assert(flash[0x105u] == 0x73 && !memcmp(flash + 0x106u, library + 1u, sizeof library - 1u));
    init();
    uint32_t attempted = UINT32_MAX;
    struct hpclang_rpl_flash_replacement transaction = {0};
    assert(hpclang_rpl_flash_create_backup(io, library, sizeof library, &attempted) ==
           HPCLANG_RPL_FLASH_INVALID);
    assert(hpclang_rpl_flash_replace_backup(io, library, sizeof library, &transaction) ==
           HPCLANG_RPL_FLASH_INVALID);
    assert(hpclang_rpl_flash_create_library(io, record, sizeof record, &attempted) ==
           HPCLANG_RPL_FLASH_INVALID);
    assert(!writes && attempted == UINT32_MAX);
    uint8_t corrupt[sizeof library];
    memcpy(corrupt, library, sizeof corrupt);
    corrupt[25] ^= 1;
    assert(hpclang_rpl_flash_create_library(io, corrupt, sizeof corrupt, &attempted) ==
           HPCLANG_RPL_FLASH_INVALID);
    assert(!writes && attempted == UINT32_MAX);
    memcpy(flash + 0x105, record, sizeof record);
    assert(!hpclang_rpl_flash_create_library(io, library, sizeof library, &attempted));
    assert(attempted == base + 0x105 + sizeof record);
    assert(!memcmp(flash + 0x105, record, sizeof record));
    assert(!memcmp(flash + 0x105 + sizeof record, library, sizeof library));
    assert(!hpclang_rpl_flash_finish_commit(io, attempted, library, sizeof library));
    unsigned programming_steps = writes;
    uint8_t renamed[sizeof library];
    memcpy(renamed, library, sizeof renamed);
    renamed[10] = 'Y';
    uint32_t crc = 0;
    for (unsigned at = 13; at < 47; ++at) {
        uint8_t nibble = (renamed[at / 2] >> (4 * (at & 1))) & 15;
        crc = (crc >> 4) ^ (((crc ^ nibble) & 15) * 0x1081);
    }
    for (unsigned i = 0; i < 4; ++i) {
        unsigned at = 47 + i, shift = 4 * (at & 1);
        renamed[at / 2] =
            (uint8_t)((renamed[at / 2] & ~(15u << shift)) | (((crc >> (4 * i)) & 15) << shift));
    }
    /* Titles are presentation metadata; identity is the embedded number. */
    assert(hpclang_rpl_flash_create_library(io, renamed, sizeof renamed, NULL) ==
           HPCLANG_RPL_FLASH_EXISTS);
    assert(writes == programming_steps);
    for (unsigned bank = 0; bank < 6; ++bank) {
        init();
        memcpy(flash + bank * 0x20000 + 0x105, library, sizeof library);
        uint32_t before = fingerprint(sizeof flash);
        attempted = UINT32_MAX;
        assert(hpclang_rpl_flash_create_library(io, library, sizeof library, &attempted) ==
               HPCLANG_RPL_FLASH_EXISTS);
        assert(!writes && attempted == UINT32_MAX && fingerprint(sizeof flash) == before);
        /* A matching library must not hide a corrupt later tail. */
        flash[sizeof flash - 1] = 0;
        assert(hpclang_rpl_flash_create_library(io, library, sizeof library, NULL) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(!writes);
        init();
        fail_address = base + bank * 0x20000 + 0x1fffe;
        assert(hpclang_rpl_flash_create_library(io, library, sizeof library, NULL) ==
               HPCLANG_RPL_FLASH_FAILED);
        assert(!writes);
    }
    for (unsigned cut = 0; cut < programming_steps; ++cut) {
        init();
        fail_write_at = cut;
        assert(hpclang_rpl_flash_create_library(io, library, sizeof library, &attempted) ==
               HPCLANG_RPL_FLASH_FAILED);
        assert(flash[0x105] == 0);
        assert(!hpclang_rpl_flash_discard_record(io, attempted, library, sizeof library));
        fail_write_at = UINT32_MAX;
        assert(!hpclang_rpl_flash_create_library(io, library, sizeof library, &attempted));
        assert(attempted == base + 0x105 + sizeof library);
    }
    init();
    for (unsigned bank = 0; bank < 6; ++bank)
        fill_bank(bank);
    assert(hpclang_rpl_flash_create_library(io, library, sizeof library, NULL) ==
           HPCLANG_RPL_FLASH_NOSPACE);
    assert(!writes);
    match_libraries(io, library, renamed, sizeof library);
    match_installations(io, library, sizeof library);
}

int main(void)
{
    const struct hpclang_rpl_flash_io io = {0, read16, program16, base, base + sizeof flash};
    init();
    uint32_t attempted = UINT32_MAX;
    flash[5u * 0x20000u + 0x110u] = 0;
    assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, &attempted) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(attempted == UINT32_MAX && !writes);
    init();
    write_failures = 100;
    assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, &attempted) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(attempted == base + 0x105u && writes);
    assert(hpclang_rpl_flash_discard_record(&io, attempted, record, sizeof record) ==
           HPCLANG_RPL_FLASH_DIRTY);
    write_failures = 0;
    assert(!hpclang_rpl_flash_discard_record(&io, attempted, record, sizeof record));
    assert(!hpclang_rpl_flash_create_backup(&io, record, sizeof record, &attempted));
    assert(attempted == base + 0x105u + sizeof record);
    init();
    assert(!hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL));
    assert(!memcmp(flash + 0x105u, record, sizeof record));
    uint32_t before = fingerprint(sizeof flash);
    writes = 0;
    assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL) ==
           HPCLANG_RPL_FLASH_EXISTS);
    assert(!writes && fingerprint(sizeof flash) == before);

    /* A matching name in the final bank must prevent writes to an empty first bank. */
    init();
    memcpy(flash + 5u * 0x20000u + 0x105u, record, sizeof record);
    before = fingerprint(sizeof flash);
    assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL) ==
           HPCLANG_RPL_FLASH_EXISTS);
    assert(!writes && fingerprint(sizeof flash) == before);

    /* Validation continues through all banks before selecting an earlier free slot. */
    flash[5u * 0x20000u + 0x105u + 30u] ^= 1;
    before = fingerprint(sizeof flash);
    assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(!writes && fingerprint(sizeof flash) == before);
    for (uint32_t index = 0; index < 6; ++index) {
        init();
        flash[index * 0x20000u + 0x110u] = 0;
        before = fingerprint(sizeof flash);
        assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(!writes && fingerprint(sizeof flash) == before);
        init();
        flash[index * 0x20000u + 0x101u] ^= 1;
        assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL) ==
               HPCLANG_RPL_FLASH_DIRTY);
        assert(!writes);
        const uint32_t offsets[] = {0x100u, 0x104u, 0x106u, 0x1fffeu};
        for (unsigned j = 0; j < sizeof offsets / sizeof offsets[0]; ++j) {
            init();
            fail_address = base + index * 0x20000u + offsets[j];
            assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL) ==
                   HPCLANG_RPL_FLASH_FAILED);
            assert(!writes);
        }
    }
    init();
    for (uint32_t i = 0; i < 5; ++i)
        fill_bank(i);
    before = fingerprint(5u * 0x20000u);
    assert(!hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL));
    assert(!memcmp(flash + 5u * 0x20000u + 0x105u, record, sizeof record));
    assert(fingerprint(5u * 0x20000u) == before);
    init();
    for (uint32_t i = 0; i < 6; ++i)
        fill_bank(i);
    before = fingerprint(sizeof flash);
    assert(hpclang_rpl_flash_create_backup(&io, record, sizeof record, NULL) ==
           HPCLANG_RPL_FLASH_NOSPACE);
    assert(!writes && fingerprint(sizeof flash) == before);
    remove_backups(&io);
    replace_backups(&io);
    ambiguous_replacement_restart(&io);
    journaled_replacement_restart(&io);
    accept_legacy_padding(&io);
    create_libraries(&io);
    recover_port_tails(&io);
    prepare_mutation(&io);
    restart_after_append(&io);
    restart_failed_append_cleanup(&io);
    recover_partial_backups(&io);
    discard_never_publishes(&io);
    recovery_never_publishes(&io);
    restart_partial_tail(&io);
    return 0;
}
