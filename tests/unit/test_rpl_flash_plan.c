#include "rpl-flash-write.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include <stdlib.h>

static const uint8_t record[] = {0x53, 0x3e, 0x00, 0x10, 0x62, 0x2b, 0x10, 0x03, 0x00, 0x08, 0x46,
                                 0x4c, 0x41, 0x53, 0x48, 0x4f, 0x4e, 0x45, 0x08, 0x14, 0x26, 0x90,
                                 0x00, 0x00, 0x87, 0x09, 0x11, 0x29, 0x00, 0xc8, 0xd1, 0xff};
static uint8_t flash[6u * 0x20000u], before[sizeof flash];
static const uint32_t base = 0x140000u;
static uint32_t fail_address = UINT32_MAX;
static unsigned fail_crc_read;
static int allow_program;
static unsigned write_calls, fail_program, cut_program, fail_readback;
static uint16_t partial_mask;
static jmp_buf power_cut;

static int read16(void *context, uint32_t address, uint16_t *value)
{
    assert(context == flash && !(address & 1u) && address >= base && address - base < sizeof flash);
    if (address == fail_address)
        return 0;
    if (write_calls && write_calls - 1u == fail_readback) {
        fail_readback = UINT32_MAX;
        return 0;
    }
    if (fail_crc_read == 2 && address >= base + 0x104u && address < base + 0x125u)
        return 0;
    if (fail_crc_read && address == base + sizeof flash - 2u)
        fail_crc_read = 2;
    uint32_t at = address - base;
    *value = (uint16_t)(flash[at] | (uint16_t)flash[at + 1u] << 8u);
    return 1;
}

static int program16(void *context, uint32_t address, uint16_t value)
{
    assert(allow_program && "planning must never program Flash");
    assert(context == flash && !(address & 1u) && address >= base && address - base < sizeof flash);
    uint32_t at = address - base;
    uint16_t old = (uint16_t)(flash[at] | (uint16_t)flash[at + 1u] << 8u);
    assert((old & value) == value);
    unsigned call = write_calls++;
    uint16_t written = call == cut_program || call == fail_program
                           ? old & (value | (uint16_t)~partial_mask)
                           : value;
    flash[at] = (uint8_t)written;
    flash[at + 1u] = (uint8_t)(written >> 8u);
    if (call == cut_program)
        longjmp(power_cut, 1);
    return call != fail_program;
}

static void init(void)
{
    memset(flash, 0xff, sizeof flash);
    for (uint32_t bank = 0; bank < 6; ++bank) {
        flash[bank * 0x20000u + 0x100u] = 0xf0;
        flash[bank * 0x20000u + 0x101u] = (uint8_t)(bank + 8u);
    }
    memcpy(flash + 0x105u, record, sizeof record);
    fail_address = UINT32_MAX;
    fail_crc_read = 0;
    allow_program = 0;
    write_calls = 0;
    fail_program = cut_program = fail_readback = UINT32_MAX;
}

/* Fill with one tombstone after any live prefix, leaving exactly free_bytes
 * of halfword-allocatable tail and the final FF status byte. */
static uint32_t leave_tail(uint32_t bank, uint32_t prefix, uint32_t free_bytes)
{
    uint32_t at = bank * 0x20000u + 0x105u + prefix;
    uint32_t bytes = 0x20000u - 0x105u - prefix - free_bytes - 1u;
    assert(bytes >= 8u && !(bytes & 1u));
    uint32_t extent = bytes * 2u - 2u;
    flash[at] = 0;
    flash[at + 1u] = (uint8_t)extent;
    flash[at + 2u] = (uint8_t)(extent >> 8u);
    flash[at + 3u] = (uint8_t)(0xf0u | (extent >> 16u));
    return base + at + bytes;
}

static void rejected(const struct hpclang_rpl_flash_io *io, int expected)
{
    struct hpclang_rpl_flash_replace_plan plan, saved;
    memset(&plan, 0x5a, sizeof plan);
    memcpy(&saved, &plan, sizeof saved);
    memcpy(before, flash, sizeof flash);
    assert(hpclang_rpl_flash_plan_replace(io, record, sizeof record, &plan) == expected);
    assert(!memcmp(&plan, &saved, sizeof plan));
    assert(!memcmp(flash, before, sizeof flash));
}

static void cut_publication(const struct hpclang_rpl_flash_io *io)
{
    struct hpclang_rpl_flash_replace_plan lost;
    if (!setjmp(power_cut)) {
        (void)hpclang_rpl_flash_publish_intent(io, record, sizeof record, &lost);
        assert(0 && "publication cut was not reached");
    }
    cut_program = UINT32_MAX;
}

static void check_publication(const struct hpclang_rpl_flash_io *io,
                              const struct hpclang_rpl_flash_replace_plan *expected)
{
    uint32_t slot = expected->intent_slot - base;
    assert(!memcmp(before, flash, slot));
    assert(!memcmp(before + slot + HPCLANG_RPL_FLASH_INTENT_BYTES,
                   flash + slot + HPCLANG_RPL_FLASH_INTENT_BYTES,
                   sizeof flash - slot - HPCLANG_RPL_FLASH_INTENT_BYTES));
    assert(flash[slot] != 0x53);
    if (flash[slot] != 0xff) {
        struct hpclang_rpl_flash_intent intent;
        enum hpclang_rpl_flash_intent_phase phase;
        assert(!hpclang_rpl_flash_intent_decode(io->first, io->end, expected->intent_slot,
                                                flash + slot, HPCLANG_RPL_FLASH_INTENT_BYTES,
                                                &intent, &phase));
        assert(!memcmp(&intent, &expected->intent, sizeof intent));
        assert(phase == HPCLANG_RPL_FLASH_INTENT_READY ||
               phase == HPCLANG_RPL_FLASH_INTENT_UNPUBLISHED);
    }
}

static void recover_publication(const struct hpclang_rpl_flash_io *io,
                                const struct hpclang_rpl_flash_replace_plan *expected)
{
    fail_program = fail_readback = UINT32_MAX;
    assert(!hpclang_rpl_flash_recover_tails(io));
    uint32_t slot = expected->intent_slot - base;
    assert(flash[slot] == 0 || flash[slot] == 0xff);
    assert(!memcmp(before, flash, slot));
    assert(!memcmp(before + slot + HPCLANG_RPL_FLASH_INTENT_BYTES,
                   flash + slot + HPCLANG_RPL_FLASH_INTENT_BYTES,
                   sizeof flash - slot - HPCLANG_RPL_FLASH_INTENT_BYTES));
    unsigned completed = write_calls;
    assert(!hpclang_rpl_flash_recover_tails(io) && write_calls == completed);
}

static void publication(const struct hpclang_rpl_flash_io *io)
{
    init();
    allow_program = 1;
    memcpy(before, flash, sizeof flash);
    struct hpclang_rpl_flash_replace_plan expected, actual;
    assert(!hpclang_rpl_flash_publish_intent(io, record, sizeof record, &expected));
    unsigned calls = write_calls;
    assert(calls && flash[expected.intent_slot - base] == 0x7e);
    check_publication(io, &expected);
    recover_publication(io, &expected);
    const uint16_t masks[] = {0, 0x00ff, 0xff00, 0xaaaa, 0x5555, 0xffff};
    for (unsigned at = 0; at < calls; ++at) {
        for (unsigned m = 0; m < sizeof masks / sizeof masks[0]; ++m) {
            for (unsigned hard = 0; hard < 2; ++hard) {
                init();
                allow_program = 1;
                partial_mask = masks[m];
                if (hard) {
                    cut_program = at;
                    cut_publication(io);
                } else {
                    fail_program = at;
                    assert(hpclang_rpl_flash_publish_intent(io, record, sizeof record, &actual) ==
                           HPCLANG_RPL_FLASH_DIRTY);
                    assert(!memcmp(&actual, &expected, sizeof actual));
                }
                check_publication(io, &expected);
                recover_publication(io, &expected);
            }
        }
        init();
        allow_program = 1;
        fail_readback = at;
        assert(hpclang_rpl_flash_publish_intent(io, record, sizeof record, &actual) ==
               HPCLANG_RPL_FLASH_DIRTY);
        check_publication(io, &expected);
        recover_publication(io, &expected);
    }
    init();
    allow_program = 1;
    flash[0xa0101u] = 0;
    actual = expected;
    memcpy(before, flash, sizeof flash);
    assert(hpclang_rpl_flash_publish_intent(io, record, sizeof record, &actual) ==
           HPCLANG_RPL_FLASH_DIRTY);
    assert(!write_calls && !memcmp(&actual, &expected, sizeof actual));
    assert(!memcmp(before, flash, sizeof flash));
}

static void reject_changed_intent(const struct hpclang_rpl_flash_io *io)
{
    for (unsigned fault = 0; fault < 4; ++fault) {
        init();
        allow_program = 1;
        struct hpclang_rpl_flash_replace_plan plan;
        assert(!hpclang_rpl_flash_publish_intent(io, record, sizeof record, &plan));
        if (fault == 0) {
            plan.intent.old_crc ^= 1u;
            assert(!hpclang_rpl_flash_intent_encode(io->first, io->end, plan.intent_slot,
                                                    &plan.intent, flash + plan.intent_slot - base,
                                                    HPCLANG_RPL_FLASH_INTENT_BYTES));
        } else if (fault == 1) {
            /* Cannot form the reserved extent using NOR bit clears. */
            flash[plan.intent.new_slot - base + 1u] = 0;
        } else if (fault == 2) {
            flash[0xa0101u] = 0;
        } else {
            flash[plan.intent_slot - base + 32u] ^= 1u;
        }
        memcpy(before, flash, sizeof flash);
        write_calls = 0;
        assert(hpclang_rpl_flash_recover_tails(io) == HPCLANG_RPL_FLASH_DIRTY);
        assert(!write_calls && !memcmp(before, flash, sizeof flash));
    }
}

static int packed_nibble(void *context, uint32_t address, uint8_t *value)
{
    const uint8_t *bytes = context;
    *value = (bytes[address / 2u] >> ((address & 1u) * 4u)) & 15u;
    return 1;
}

static uint8_t *replacement_named(const char *name, uint32_t *bytes)
{
    uint8_t value[] = {0x14, 0x26, 0x90, 0, 0, 0x86, 0x09}; /* ZINT 986, rather than old 987. */
    const struct hpclang_rpl_reader source = {packed_nibble, value};
    uint8_t *backup, *replacement;
    uint32_t span;
    assert(!hpclang_rpl_backup_encode(&source, 0, 14u, (const uint8_t *)name,
                                      (uint32_t)strlen(name), &backup, &span));
    const struct hpclang_rpl_reader encoded = {packed_nibble, backup};
    assert(!hpclang_rpl_flash_backup_record(&encoded, 0, span, &replacement, bytes));
    free(backup);
    return replacement;
}

static uint8_t *replacement_record(uint32_t *bytes)
{
    return replacement_named("FLASHONE", bytes);
}

static void committed_recovery(const struct hpclang_rpl_flash_io *io)
{
    uint32_t bytes;
    uint8_t *replacement = replacement_record(&bytes);
    for (unsigned layout = 0; layout < 3; ++layout) {
        for (unsigned status = 0; status <= 0x53u; ++status) {
            if (status & ~0x53u)
                continue;
            init();
            if (layout) {
                for (unsigned bank = 2; bank < 6; ++bank)
                    leave_tail(bank, 0, 0);
                leave_tail(0, sizeof record, layout == 1 ? HPCLANG_RPL_FLASH_INTENT_BYTES : bytes);
                leave_tail(1, 0, layout == 1 ? bytes : HPCLANG_RPL_FLASH_INTENT_BYTES);
            }
            allow_program = 1;
            struct hpclang_rpl_flash_replace_plan plan;
            assert(!hpclang_rpl_flash_publish_intent(io, replacement, bytes, &plan));
            assert(plan.intent.old_crc != plan.intent.new_crc);
            assert(!hpclang_rpl_flash_append(io, plan.intent.new_slot, replacement, bytes));
            uint32_t old = plan.intent.old_slot - base, journal = plan.intent_slot - base;
            flash[old] = (uint8_t)status;
            memcpy(before, flash, sizeof flash);
            assert(!hpclang_rpl_flash_recover_tails(io));
            for (uint32_t at = 0; at < sizeof flash; ++at)
                assert(flash[at] == (at == old || at == journal ? 0 : before[at]));
            assert(!memcmp(flash + plan.intent.new_slot - base, replacement, bytes));
            unsigned completed = write_calls;
            assert(!hpclang_rpl_flash_recover_tails(io) && completed == write_calls);
        }
    }
    free(replacement);
}

static void committed_rejections(const struct hpclang_rpl_flash_io *io)
{
    uint32_t bytes;
    uint8_t *replacement = replacement_record(&bytes);
    for (unsigned fault = 0; fault < 6; ++fault) {
        init();
        allow_program = 1;
        struct hpclang_rpl_flash_replace_plan plan;
        assert(!hpclang_rpl_flash_publish_intent(io, replacement, bytes, &plan));
        assert(!hpclang_rpl_flash_append(io, plan.intent.new_slot, replacement, bytes));
        if (fault == 0) {
            flash[plan.intent_slot - base] = 0x7f; /* New value before publication is impossible. */
        } else if (fault == 1) {
            flash[plan.intent_slot - base] = 0x7c; /* Retirement cannot precede old deletion. */
        } else if (fault == 2) {
            uint32_t second = plan.intent.new_slot + bytes;
            assert(!hpclang_rpl_flash_intent_encode(io->first, io->end, second, &plan.intent,
                                                    flash + second - base,
                                                    HPCLANG_RPL_FLASH_INTENT_BYTES));
        } else if (fault == 3) {
            /* The bytes and checksums match, but this is inside a deleted
             * allocation, not a record start. Do not modify retained payload. */
            leave_tail(1, 0, 0);
            plan.intent.old_slot = base + 0x20115u;
            memcpy(flash + plan.intent.old_slot - base, record, sizeof record);
            assert(!hpclang_rpl_flash_intent_encode(io->first, io->end, plan.intent_slot,
                                                    &plan.intent, flash + plan.intent_slot - base,
                                                    HPCLANG_RPL_FLASH_INTENT_BYTES));
        } else if (fault == 4) {
            plan.intent.new_crc ^= 1u;
            assert(!hpclang_rpl_flash_intent_encode(io->first, io->end, plan.intent_slot,
                                                    &plan.intent, flash + plan.intent_slot - base,
                                                    HPCLANG_RPL_FLASH_INTENT_BYTES));
        } else {
            uint32_t other_bytes;
            uint8_t *other = replacement_named("OTHERONE", &other_bytes);
            assert(other_bytes == bytes);
            memcpy(flash + plan.intent.new_slot - base, other, bytes);
            const struct hpclang_rpl_reader reader = {packed_nibble, other};
            assert(!hpclang_rpl_flash_record_crc(&reader, 0, bytes, &plan.intent.new_crc));
            free(other);
            assert(!hpclang_rpl_flash_intent_encode(io->first, io->end, plan.intent_slot,
                                                    &plan.intent, flash + plan.intent_slot - base,
                                                    HPCLANG_RPL_FLASH_INTENT_BYTES));
        }
        memcpy(before, flash, sizeof flash);
        write_calls = 0;
        assert(hpclang_rpl_flash_recover_tails(io) == HPCLANG_RPL_FLASH_DIRTY);
        assert(!write_calls && !memcmp(before, flash, sizeof flash));
    }
    free(replacement);
}

static void cut_committed_recovery(const struct hpclang_rpl_flash_io *io)
{
    if (!setjmp(power_cut)) {
        (void)hpclang_rpl_flash_recover_tails(io);
        assert(0 && "recovery cut was not reached");
    }
    cut_program = UINT32_MAX;
}

static void restart_committed_recovery(const struct hpclang_rpl_flash_io *io, uint8_t status)
{
    uint32_t bytes;
    uint8_t *replacement = replacement_record(&bytes);
    struct hpclang_rpl_flash_replace_plan plan;
    init();
    allow_program = 1;
    assert(!hpclang_rpl_flash_publish_intent(io, replacement, bytes, &plan));
    assert(!hpclang_rpl_flash_append(io, plan.intent.new_slot, replacement, bytes));
    flash[plan.intent.new_slot - base] = status;
    memcpy(before, flash, sizeof flash);
    write_calls = 0;
    assert(!hpclang_rpl_flash_recover_tails(io));
    unsigned calls = write_calls;
    const uint16_t masks[] = {0, 0x00ff, 0xff00, 0x5555, 0xaaaa, 0xffff};
    for (unsigned at = 0; at < calls; ++at) {
        for (unsigned m = 0; m < sizeof masks / sizeof masks[0]; ++m) {
            init();
            memcpy(flash, before, sizeof flash);
            allow_program = 1;
            partial_mask = masks[m];
            cut_program = at;
            cut_committed_recovery(io);
            assert(!hpclang_rpl_flash_recover_tails(io));
            for (uint32_t offset = 0; offset < sizeof flash; ++offset)
                assert(flash[offset] ==
                       (offset == plan.intent.old_slot - base || offset == plan.intent_slot - base
                            ? 0
                        : offset == plan.intent.new_slot - base ? 0x53u
                                                                : before[offset]));
        }
    }
    free(replacement);
}

static void cut_append(const struct hpclang_rpl_flash_io *io, uint32_t slot,
                       const uint8_t *replacement, uint32_t bytes)
{
    if (!setjmp(power_cut)) {
        (void)hpclang_rpl_flash_append(io, slot, replacement, bytes);
        assert(0 && "append cut was not reached");
    }
    cut_program = UINT32_MAX;
}

static void partial_body_recovery(const struct hpclang_rpl_flash_io *io)
{
    uint32_t bytes;
    uint8_t *replacement = replacement_record(&bytes);
    uint8_t *pending = malloc(sizeof flash);
    assert(pending);
    const uint16_t masks[] = {0, 0x00ff, 0xff00, 0x5555, 0xaaaa, 0xffff};
    for (unsigned layout = 0; layout < 3; ++layout) {
        init();
        if (layout) {
            for (unsigned bank = 2; bank < 6; ++bank)
                leave_tail(bank, 0, 0);
            leave_tail(0, sizeof record, layout == 1 ? HPCLANG_RPL_FLASH_INTENT_BYTES : bytes);
            leave_tail(1, 0, layout == 1 ? bytes : HPCLANG_RPL_FLASH_INTENT_BYTES);
        }
        allow_program = 1;
        struct hpclang_rpl_flash_replace_plan plan;
        assert(!hpclang_rpl_flash_publish_intent(io, replacement, bytes, &plan));
        memcpy(before, flash, sizeof flash);
        write_calls = 0;
        assert(!hpclang_rpl_flash_append(io, plan.intent.new_slot, replacement, bytes));
        unsigned calls = write_calls;
        uint32_t slot = plan.intent.new_slot - base, journal = plan.intent_slot - base;
        for (unsigned at = 0; at < calls; ++at) {
            for (unsigned m = 0; m < sizeof masks / sizeof masks[0]; ++m) {
                memcpy(flash, before, sizeof flash);
                write_calls = 0;
                partial_mask = masks[m];
                cut_program = at;
                cut_append(io, plan.intent.new_slot, replacement, bytes);
                if (flash[slot] != 0xff) {
                    assert(!hpclang_rpl_flash_recover_tails(io));
                    assert(!flash[plan.intent.old_slot - base] && !flash[journal]);
                    assert(!memcmp(flash + slot, replacement, bytes));
                    continue;
                }
                memcpy(pending, flash, sizeof flash);
                write_calls = 0;
                assert(!hpclang_rpl_flash_recover_tails(io));
                unsigned recovery_calls = write_calls;
                for (uint32_t i = 0; i < sizeof flash; ++i)
                    if (i != journal && (i < slot || i >= slot + 4u))
                        assert(flash[i] == pending[i]);
                assert(!memcmp(flash + plan.intent.old_slot - base, record, sizeof record));
                assert(!flash[journal]);
                /* On a representative fully written but unpublished body,
                 * interrupt every rollback write with every partial mask. */
                if (at + 1u == calls && m == 0) {
                    for (unsigned step = 0; step < recovery_calls; ++step) {
                        for (unsigned mask = 0; mask < sizeof masks / sizeof masks[0]; ++mask) {
                            memcpy(flash, pending, sizeof flash);
                            write_calls = 0;
                            cut_program = step;
                            partial_mask = masks[mask];
                            cut_committed_recovery(io);
                            assert(!hpclang_rpl_flash_recover_tails(io));
                            for (uint32_t i = 0; i < sizeof flash; ++i)
                                if (i != journal && (i < slot || i >= slot + 4u))
                                    assert(flash[i] == pending[i]);
                            assert(!memcmp(flash + plan.intent.old_slot - base, record,
                                           sizeof record));
                            assert(!flash[journal]);
                        }
                    }
                }
            }
        }
    }
    free(pending);
    free(replacement);
}

static void partial_body_rejections(const struct hpclang_rpl_flash_io *io)
{
    for (unsigned fault = 0; fault < 5; ++fault) {
        init();
        allow_program = 1;
        struct hpclang_rpl_flash_replace_plan plan;
        assert(!hpclang_rpl_flash_publish_intent(io, record, sizeof record, &plan));
        uint32_t slot = plan.intent.new_slot - base;
        flash[slot + 4u] = 0;
        if (fault == 0)
            flash[plan.intent.old_slot - base] = 0;
        else if (fault == 1)
            flash[plan.intent_slot - base] = 0x7f;
        else if (fault == 2)
            flash[plan.intent_slot - base] = 0x7c;
        else if (fault == 3)
            flash[slot + plan.intent.new_bytes] = 0;
        else
            flash[slot + 1u] = 0;
        memcpy(before, flash, sizeof flash);
        write_calls = 0;
        assert(hpclang_rpl_flash_recover_tails(io) == HPCLANG_RPL_FLASH_DIRTY);
        assert(!write_calls && !memcmp(flash, before, sizeof flash));
    }
}

static void publication_status_recovery(const struct hpclang_rpl_flash_io *io)
{
    uint32_t bytes;
    uint8_t *replacement = replacement_record(&bytes);
    for (unsigned status = 0; status < 256u; ++status) {
        init();
        allow_program = 1;
        struct hpclang_rpl_flash_replace_plan plan;
        assert(!hpclang_rpl_flash_publish_intent(io, replacement, bytes, &plan));
        assert(!hpclang_rpl_flash_append(io, plan.intent.new_slot, replacement, bytes));
        uint32_t slot = plan.intent.new_slot - base;
        flash[slot] = (uint8_t)status;
        memcpy(before, flash, sizeof flash);
        write_calls = 0;
        int result = hpclang_rpl_flash_recover_tails(io);
        if (status != 0xffu && (status & 0x53u) == 0x53u) {
            assert(!result && !flash[plan.intent.old_slot - base]);
            assert(!memcmp(flash + slot, replacement, bytes));
        } else if (status == 0xffu) {
            assert(!result && !memcmp(flash + plan.intent.old_slot - base, record, sizeof record));
            assert(!flash[slot]);
        } else {
            assert(result == HPCLANG_RPL_FLASH_DIRTY);
            assert(!write_calls && !memcmp(before, flash, sizeof flash));
        }
    }
    for (unsigned fault = 0; fault < 4; ++fault) {
        init();
        allow_program = 1;
        struct hpclang_rpl_flash_replace_plan plan;
        assert(!hpclang_rpl_flash_publish_intent(io, replacement, bytes, &plan));
        assert(!hpclang_rpl_flash_append(io, plan.intent.new_slot, replacement, bytes));
        flash[plan.intent.new_slot - base] = 0x7f;
        if (fault == 0)
            flash[plan.intent.old_slot - base] = 0;
        else if (fault == 1)
            flash[plan.intent_slot - base] = 0x7f;
        else if (fault == 2)
            flash[plan.intent_slot - base] = 0x7c;
        else
            flash[plan.intent.new_slot - base + 5u] ^= 1u;
        memcpy(before, flash, sizeof flash);
        write_calls = 0;
        assert(hpclang_rpl_flash_recover_tails(io) == HPCLANG_RPL_FLASH_DIRTY);
        assert(!write_calls && !memcmp(before, flash, sizeof flash));
    }
    free(replacement);
}

static void replacement_returned_errors(const struct hpclang_rpl_flash_io *io)
{
    uint32_t bytes;
    uint8_t *replacement = replacement_record(&bytes);
    init();
    allow_program = 1;
    struct hpclang_rpl_flash_replacement transaction;
    assert(!hpclang_rpl_flash_replace_backup(io, replacement, bytes, &transaction));
    unsigned calls = write_calls;
    for (unsigned at = 0; at < calls; ++at) {
        for (unsigned fault = 0; fault < 3; ++fault) {
            init();
            allow_program = 1;
            if (fault == 2)
                fail_readback = at;
            else {
                fail_program = at;
                partial_mask = fault ? 0xffffu : 0;
            }
            int result = hpclang_rpl_flash_replace_backup(io, replacement, bytes, &transaction);
            assert(result == HPCLANG_RPL_FLASH_DIRTY);
            fail_program = fail_readback = UINT32_MAX;
            result = hpclang_rpl_flash_finish_replace(io, &transaction);
            if (result == HPCLANG_RPL_FLASH_WRITTEN) {
                assert(transaction.committed && !flash[transaction.old_slot - base]);
                assert(!memcmp(flash + transaction.new_slot - base, replacement, bytes));
            } else {
                assert(result == HPCLANG_RPL_FLASH_FAILED && !transaction.committed);
                assert(!memcmp(flash + transaction.old_slot - base, record, sizeof record));
                assert(flash[transaction.new_slot - base] != 0x53u);
            }
            assert(
                !memcmp(flash + transaction.old_slot - base + 1u, record + 1u, sizeof record - 1u));
            unsigned recovered = write_calls;
            assert(hpclang_rpl_flash_finish_replace(io, &transaction) == result);
            assert(write_calls == recovered);
        }
    }
    free(replacement);
}

int main(void)
{
    const struct hpclang_rpl_flash_io io = {flash, read16, program16, base, base + sizeof flash};
    struct hpclang_rpl_flash_replace_plan plan;
    init();
    memcpy(before, flash, sizeof flash);
    assert(!hpclang_rpl_flash_plan_replace(&io, record, sizeof record, &plan));
    assert(plan.intent.old_slot == base + 0x105u && plan.intent.old_bytes == sizeof record);
    assert(plan.intent_slot == base + 0x105u + sizeof record);
    assert(plan.intent.new_slot == plan.intent_slot + HPCLANG_RPL_FLASH_INTENT_BYTES);
    assert(plan.intent.new_bytes == sizeof record && plan.intent.old_crc == plan.intent.new_crc);
    assert(plan.intent.old_crc == UINT32_C(0xc11cf392)); /* Independent zlib vector. */
    assert(!memcmp(flash, before, sizeof flash));
    uint8_t encoded[HPCLANG_RPL_FLASH_INTENT_BYTES];
    assert(!hpclang_rpl_flash_intent_encode(io.first, io.end, plan.intent_slot, &plan.intent,
                                            encoded, sizeof encoded));
    /* Neither bank can fit both allocations; journal-first greed must still
     * find the pair when the only new-record space is in an earlier bank. */
    for (unsigned reverse = 0; reverse < 2; ++reverse) {
        init();
        for (unsigned bank = 2; bank < 6; ++bank)
            leave_tail(bank, 0, 0);
        uint32_t zero = leave_tail(0, sizeof record, reverse ? sizeof record : sizeof encoded);
        uint32_t one = leave_tail(1, 0, reverse ? sizeof encoded : sizeof record);
        memcpy(before, flash, sizeof flash);
        assert(!hpclang_rpl_flash_plan_replace(&io, record, sizeof record, &plan));
        assert(plan.intent_slot == (reverse ? one : zero));
        assert(plan.intent.new_slot == (reverse ? zero : one));
        assert(!memcmp(flash, before, sizeof flash));
    }
    init();
    for (unsigned bank = 0; bank < 6; ++bank)
        leave_tail(bank, bank ? 0 : sizeof record, bank ? 0 : sizeof encoded + sizeof record - 2u);
    rejected(&io, HPCLANG_RPL_FLASH_NOSPACE);
    init();
    memcpy(flash + 0xa0105u, record, sizeof record);
    rejected(&io, HPCLANG_RPL_FLASH_DIRTY);
    init();
    flash[0x105u] = 0;
    rejected(&io, HPCLANG_RPL_FLASH_NOT_FOUND);
    for (unsigned bank = 0; bank < 6; ++bank) {
        init();
        flash[bank * 0x20000u + 0x101u] = 0;
        rejected(&io, HPCLANG_RPL_FLASH_DIRTY);
        init();
        fail_address = base + bank * 0x20000u + 0x1fffeu;
        rejected(&io, HPCLANG_RPL_FLASH_FAILED);
    }
    init();
    fail_crc_read = 1;
    rejected(&io, HPCLANG_RPL_FLASH_FAILED);
    publication(&io);
    reject_changed_intent(&io);
    committed_recovery(&io);
    committed_rejections(&io);
    restart_committed_recovery(&io, 0x53u);
    restart_committed_recovery(&io, 0x7fu);
    partial_body_recovery(&io);
    partial_body_rejections(&io);
    publication_status_recovery(&io);
    replacement_returned_errors(&io);
    return 0;
}
