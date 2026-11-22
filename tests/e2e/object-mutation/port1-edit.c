#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <hpclang/port.h>
#include "object-snapshot.h"
#include "rpl-backup.h"
#include "rpl-port.h"
#include "rpl-port-mutation.h"
#include "rpl-stack-view.h"
#include "rpl-stock-memory.h"
#include "saturn-view.h"

static const uint8_t first_name[] = "HPCE1A", tail_name[] = "HPCE1B";

struct owned_tail
{
    uint32_t first, end, calls;
    uint32_t test_stack_slot, test_stack_target;
};

static int stock_nibble(void *context, uint32_t address, uint8_t *value)
{
    const struct owned_tail *owned = context;
    /* Overlay one slot only for the live-reference refusal test. */
    if (owned && owned->test_stack_slot && address >= owned->test_stack_slot &&
        address - owned->test_stack_slot < 5u) {
        *value = (owned->test_stack_target >> (4u * (address - owned->test_stack_slot))) & 15u;
        return 1;
    }
    return hpclang_saturn_read_nibble(hpclang_saturn_address_make(address), value);
}

/* Test-only ownership proof: these records are appended during this invocation,
 * never exported as native references and never exposed to resumed Stock.
 * This is not the production live-reference collector needed by public edits. */
static int check_owned_tail(void *context, uint32_t first, uint32_t end)
{
    struct owned_tail *owned = context;
    ++owned->calls;
    if (first < owned->first || first >= end || end > owned->end)
        return HPCLANG_RPL_MUTATION_MALFORMED;
    /* These HP49g+/HP50g Stock 2.15 fixtures use the ROM ERAM switch at 0x26c39. */
    const struct hpclang_rpl_reader stock = {stock_nibble, owned};
    uint32_t selector = 0, home = 0;
    for (unsigned i = 0; i < 5; ++i) {
        uint8_t nibble;
        if (!stock_nibble(NULL, 0x26c39u + i, &nibble))
            return HPCLANG_RPL_MUTATION_IO;
        selector |= (uint32_t)nibble << (4 * i);
        if (!stock_nibble(NULL, 0x80711u + i, &nibble))
            return HPCLANG_RPL_MUTATION_IO;
        home |= (uint32_t)nibble << (4 * i);
    }
    if (selector != 0x23c21u)
        return HPCLANG_RPL_MUTATION_UNSUPPORTED;
    int overlap = hpclang_rpl_port1_catalog_overlap(&stock, 0x8611du, 0x100000u - 0x8611du,
                                                    selector, first, end);
    if (overlap < 0)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    int cache = hpclang_rpl_port1_home_overlap(&stock, home, 0x100000u - home, first, end);
    if (cache < 0)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    /* A direct address in the bank window may designate ERAM. */
    int stack = hpclang_rpl_stack_direct_overlap(&stock, 0x40000u + first, 0x40000u + end);
    if (stack < 0)
        return HPCLANG_RPL_MUTATION_UNAVAILABLE;
    return overlap || cache || stack ? HPCLANG_RPL_MUTATION_BUSY : HPCLANG_RPL_MUTATION_OK;
}

static int chain_end(const struct hpclang_rpl_reader *reader, uint32_t *end)
{
    struct hpclang_rpl_port_cursor cursor = {0, 0x40000u, 0};
    struct hpclang_rpl_port_entry entry;
    int result;
    while ((result = hpclang_rpl_port_next(reader, &cursor, &entry)) > 0)
        ;
    if (result)
        return 0;
    *end = cursor.address;
    return 1;
}

static int fingerprint(const struct hpclang_rpl_reader *reader, uint32_t end, uint32_t *hash)
{
    uint32_t value = 2166136261u;
    for (uint32_t at = 0; at < end; ++at) {
        uint8_t nibble;
        if (!reader->read_nibble(reader->context, at, &nibble))
            return 0;
        value = (value ^ nibble) * 16777619u;
    }
    *hash = value;
    return 1;
}

static int object_read(void *context, uint32_t at, uint8_t *value)
{
    return hpclang_object_snapshot_read_nibble(context, at, value);
}

static int encode(const uint8_t *name, struct hpclang_object *object, uint8_t **packed,
                  uint32_t *span)
{
    const struct hpclang_rpl_reader reader = {object_read, object};
    return hpclang_rpl_backup_encode(&reader, 0, object->nibble_count, name, 6, packed, span) == 0;
}

static int matches(const uint8_t *name, const struct hpclang_object *expected)
{
    struct hpclang_object *value = hpclang_port_backup_get(1, name, 6);
    int equal =
        value && value->kind == expected->kind && value->nibble_count == expected->nibble_count &&
        !memcmp(value->data, expected->data, value->nibble_count / 2u) &&
        (!(value->nibble_count & 1u) ||
         !((value->data[value->nibble_count / 2u] ^ expected->data[value->nibble_count / 2u]) &
           15u));
    hpclang_object_release(value);
    return equal;
}

uint32_t hpclang_e2e_port1_owned_edits(void)
{
    struct hpclang_rpl_mutation_io io;
    uint32_t limit, original_hash, hash, span, end;
    struct hpclang_object *value = NULL, *tail = NULL;
    uint8_t *packed = NULL;
    struct owned_tail owned = {0};
    const struct hpclang_rpl_port1_guard guard = {check_owned_tail, &owned};
    unsigned step = 0;
    const struct hpclang_rpl_reader stock = {stock_nibble, NULL};
    uint32_t level_one;
    /* The original calculator stack may be empty. Own one real slot for the probe. */
    if (hpclang_stack_push_boolean(1))
        return 0x1e0u;
    if (hpclang_rpl_stack_object_address(&stock, 1, &level_one) ||
        hpclang_rpl_stack_direct_overlap(&stock, level_one, level_one + 1u) != 1)
        goto done;
    if (!hpclang_rpl_stock_port1_mutation_io(&io, &limit))
        goto done;
    const struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
    if (!chain_end(&reader, &owned.first) || !fingerprint(&reader, owned.first, &original_hash))
        goto done;
    step = 1;
    if (hpclang_object_create_integer_i64(1, &value) ||
        !encode(first_name, value, &packed, &span) ||
        hpclang_rpl_port1_create_backup(&io, limit, packed, span))
        goto done;
    free(packed);
    packed = NULL;
    step = 2;
    if (hpclang_object_create_integer_i64(321, &tail) || !encode(tail_name, tail, &packed, &span) ||
        hpclang_rpl_port1_create_backup(&io, limit, packed, span))
        goto done;
    free(packed);
    packed = NULL;

    /* Same-size replacement, growth and contraction each preserve the tail. */
    for (unsigned variant = 0; variant < 3; ++variant) {
        static const uint8_t large[] = "A longer replacement moves the following owned backup";
        static const uint8_t small[] = "x";
        step = 3 + variant;
        hpclang_object_release(value);
        value = NULL;
        int result =
            variant == 0
                ? hpclang_object_create_integer_i64(2, &value)
                : hpclang_object_create_string(variant == 1 ? large : small,
                                               variant == 1 ? sizeof(large) - 1 : 1, &value);
        if (result || !encode(first_name, value, &packed, &span) || !chain_end(&reader, &owned.end))
            goto done;
        if (variant == 0) {
            uint32_t before;
            if (!fingerprint(&reader, owned.end + 6u, &before))
                goto done;
            for (unsigned i = 0; i < 5; ++i) {
                uint8_t nibble;
                if (!stock_nibble(NULL, 0x806f8u + i, &nibble))
                    goto done;
                owned.test_stack_slot |= (uint32_t)nibble << (4u * i);
            }
            owned.test_stack_target = 0x40000u + owned.first;
            result = hpclang_rpl_port1_replace_backup(&io, limit, packed, span, &guard);
            owned.test_stack_slot = 0;
            if (result != HPCLANG_RPL_MUTATION_BUSY ||
                !fingerprint(&reader, owned.end + 6u, &hash) || hash != before)
                goto done;
        }
        if (hpclang_rpl_port1_replace_backup(&io, limit, packed, span, &guard) ||
            !matches(first_name, value) || !matches(tail_name, tail))
            goto done;
        free(packed);
        packed = NULL;
    }
    step = 6;
    if (!chain_end(&reader, &owned.end) ||
        hpclang_rpl_port1_remove_backup(&io, limit, first_name, 6, &guard) ||
        !matches(tail_name, tail) || !chain_end(&reader, &owned.end) ||
        hpclang_rpl_port1_remove_backup(&io, limit, tail_name, 6, &guard))
        goto done;
    step = 7;
    if (owned.calls != 6 || !chain_end(&reader, &end) || end != owned.first ||
        !fingerprint(&reader, end, &hash) || hash != original_hash)
        goto done;
    free(packed);
    hpclang_object_release(value);
    hpclang_object_release(tail);
    return hpclang_stack_drop(1) ? 0x1e8u : 0;
done:
    free(packed);
    hpclang_object_release(value);
    hpclang_object_release(tail);
    hpclang_stack_drop(1);
    return 0x1e0u + step;
}
