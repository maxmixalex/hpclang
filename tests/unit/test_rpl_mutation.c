#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "object-snapshot.h"
#include "rpl-mutation.h"
#include "rpl-recovery-reservation.h"
#include "rpl-reference.h"
#include "rpl-object-store.h"
#include "rpl-port-mutation.h"
#include "rpl-port.h"
#include "rpl-backup.h"
#include "stage-stack.h"

enum
{
    NIBBLES = 0x100000u,
    TEMPTOP = 0x806eeu,
    RSKTOP = 0x806f3u,
    DSKTOP = 0x806f8u,
    EDITLINE = 0x806fdu,
    USEROB = 0x80711u,
    CONTEXT = 0x8071bu,
    TEMPENV = 0x80702u,
    TOUCHTAB = 0x8070cu,
    FSTVGERPTR = 0x8072fu,
    AVMEM = 0x80e9bu,
    DORRP = 0x02a96u
};

static uint8_t memory[NIBBLES];
static uint32_t rejected_read;
static uint32_t writes;
static uint32_t watched_field, watched_writes;
static uint32_t rejected_write, rejected_commit_read;
static uint32_t write_trace[8192];

#ifdef HPCLANG_TEST_WRAP_ALLOC
void *__real_malloc(size_t size);
void __real_free(void *pointer);
static int track_allocation, reject_allocation;
static unsigned allocation_attempts, allocation_frees;
static void *tracked_allocation;

void *__wrap_malloc(size_t size)
{
    if (track_allocation) {
        ++allocation_attempts;
        if (reject_allocation)
            return NULL;
    }
    void *pointer = __real_malloc(size);
    if (track_allocation)
        tracked_allocation = pointer;
    return pointer;
}

void __wrap_free(void *pointer)
{
    if (pointer && pointer == tracked_allocation) {
        ++allocation_frees;
        tracked_allocation = NULL;
    }
    __real_free(pointer);
}
#endif

static int read_nibble(void *context, uint32_t address, uint8_t *value)
{
    (void)context;
    if (value == NULL || address >= NIBBLES || address == rejected_read ||
        (writes && address == rejected_commit_read))
        return 0;
    *value = memory[address] & 0x0fu;
    return 1;
}

static int write_nibble(void *context, uint32_t address, uint8_t value)
{
    (void)context;
    if (address >= NIBBLES || value > 0x0fu || writes == rejected_write)
        return 0;
    memory[address] = value;
    if (writes < sizeof write_trace / sizeof write_trace[0])
        write_trace[writes] = address;
    ++writes;
    if (address == watched_field)
        ++watched_writes;
    return 1;
}

static void put_word(uint32_t address, uint32_t nibbles, uint32_t value)
{
    assert(address + nibbles <= NIBBLES);
    for (uint32_t i = 0u; i < nibbles; ++i)
        memory[address + i] = (uint8_t)((value >> (i * 4u)) & 0x0fu);
}

static uint32_t get_word(uint32_t address, uint32_t nibbles)
{
    assert(address + nibbles <= NIBBLES);
    uint32_t value = 0u;
    for (uint32_t i = 0u; i < nibbles; ++i)
        value |= (uint32_t)memory[address + i] << (i * 4u);
    return value;
}

static struct hpclang_object *make_string1(uint8_t ch)
{
    struct hpclang_object *object = malloc(sizeof(*object) + 6u);
    assert(object != NULL);
    object->kind = HPCLANG_OBJECT_STRING;
    object->nibble_count = 12u;
    object->native_prologue = 0x02a2cu;
    memset(object->data, 0, 6u);

    const uint32_t prologue = 0x02a2cu;
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint8_t nibble = (uint8_t)((prologue >> (i * 4u)) & 0x0fu);
        if ((i & 1u) == 0u)
            object->data[i >> 1] = nibble;
        else
            object->data[i >> 1] |= (uint8_t)(nibble << 4);
    }

    /* DOCSTR length counts [length field + payload], excluding prologue. */
    const uint32_t length = 7u;
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint32_t nibble_index = 5u + i;
        uint8_t nibble = (uint8_t)((length >> (i * 4u)) & 0x0fu);
        if ((nibble_index & 1u) == 0u)
            object->data[nibble_index >> 1] |= nibble;
        else
            object->data[nibble_index >> 1] |= (uint8_t)(nibble << 4);
    }

    object->data[5] = (uint8_t)((ch & 0x0fu) | (uint8_t)(ch & 0xf0u));
    return object;
}

static struct hpclang_object *make_char(uint8_t ch)
{
    struct hpclang_object *object = malloc(sizeof(*object) + 4u);
    assert(object != NULL);
    object->kind = HPCLANG_OBJECT_CHARACTER;
    object->nibble_count = 7u;
    object->native_prologue = 0x029bfu;
    memset(object->data, 0, 4u);

    const uint32_t prologue = 0x029bfu;
    for (uint32_t i = 0u; i < 5u; ++i) {
        uint8_t nibble = (uint8_t)((prologue >> (i * 4u)) & 0x0fu);
        if ((i & 1u) == 0u)
            object->data[i >> 1] = nibble;
        else
            object->data[i >> 1] |= (uint8_t)(nibble << 4);
    }
    object->data[2] |= (uint8_t)((ch & 0x0fu) << 4);
    object->data[3] = (uint8_t)(ch >> 4);
    return object;
}

static struct hpclang_rpl_mutation_io setup(uint32_t depth)
{
    memset(memory, 0, sizeof(memory));
    rejected_read = UINT32_MAX;
    rejected_write = rejected_commit_read = UINT32_MAX;
    writes = 0;

    const uint32_t ttop = 0x84000u;
    const uint32_t rstk = 0x85000u;
    const uint32_t dstk = 0x90000u;

    put_word(TEMPTOP, 5u, ttop);
    put_word(RSKTOP, 5u, rstk);
    put_word(DSKTOP, 5u, dstk);
    put_word(EDITLINE, 5u, dstk + (depth + 1u) * 5u);
    put_word(AVMEM, 5u, (dstk - rstk) / 5u);

    for (uint32_t i = ttop; i < rstk; ++i)
        memory[i] = (uint8_t)(i & 0x0fu);

    /* A zero return-stack sentinel keeps reference scanning structurally valid. */
    put_word(rstk - 5u, 5u, 0u);

    for (uint32_t level = 0u; level < depth; ++level)
        put_word(dstk + level * 5u, 5u, 0x82000u + level * 0x20u);

    const struct hpclang_rpl_mutation_io io = {
        .read_nibble = read_nibble,
        .write_nibble = write_nibble,
        .context = NULL,
        .temp_limit = 0x86000u,
        .user_limit = 0x88000u,
        .stack_reserve_nibbles = 0u,
    };
    return io;
}

static void assert_char_at(uint32_t address, uint8_t ch)
{
    assert(get_word(address, 5u) == 0x029bfu);
    assert(get_word(address + 5u, 2u) == ch);
}

static uint32_t prepare_named_char(uint8_t value)
{
    const uint32_t home = 0x92000u;
    const uint32_t chain = home + 8u;
    const uint32_t link = chain + 5u;
    const uint32_t item = link + 5u;
    const uint32_t object = item + 6u;

    put_word(USEROB, 5u, home);
    put_word(CONTEXT, 5u, home);
    put_word(home, 5u, DORRP);
    put_word(home + 5u, 3u, 0u);
    put_word(chain, 5u, item - chain);
    put_word(link, 5u, 0u);
    put_word(item, 2u, 1u);
    put_word(item + 2u, 2u, (uint32_t)'A');
    put_word(item + 4u, 2u, 1u);
    put_word(object, 5u, 0x029bfu);
    put_word(object + 5u, 2u, value);
    return object;
}

struct named_pair
{
    uint32_t home;
    uint32_t chain;
    uint32_t link_a;
    uint32_t item_a;
    uint32_t object_a;
    uint32_t link_b;
    uint32_t item_b;
    uint32_t object_b;
};

static struct named_pair prepare_two_named_chars(void)
{
    const struct named_pair pair = {
        .home = 0x92000u,
        .chain = 0x92008u,
        .link_b = 0x9200du,
        .item_b = 0x92012u,
        .object_b = 0x92018u,
        .link_a = 0x9201fu,
        .item_a = 0x92024u,
        .object_a = 0x9202au,
    };

    put_word(USEROB, 5u, pair.home);
    put_word(CONTEXT, 5u, pair.home);
    put_word(pair.home, 5u, DORRP);
    put_word(pair.home + 5u, 3u, 0u);

    put_word(pair.chain, 5u, pair.item_a - pair.chain);

    put_word(pair.link_b, 5u, 0u);
    put_word(pair.item_b, 2u, 1u);
    put_word(pair.item_b + 2u, 2u, (uint32_t)'B');
    put_word(pair.item_b + 4u, 2u, 1u);
    put_word(pair.object_b, 5u, 0x029bfu);
    put_word(pair.object_b + 5u, 2u, (uint32_t)'B');

    put_word(pair.link_a, 5u, pair.link_a - pair.item_b);
    put_word(pair.item_a, 2u, 1u);
    put_word(pair.item_a + 2u, 2u, (uint32_t)'A');
    put_word(pair.item_a + 4u, 2u, 1u);
    put_word(pair.object_a, 5u, 0x029bfu);
    put_word(pair.object_a + 5u, 2u, (uint32_t)'A');

    return pair;
}

static void test_userob_shift_roundtrip(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct named_pair pair = prepare_two_named_chars();

    uint32_t native_moving = pair.object_b;
    uint32_t native_fixed = pair.object_a;
    uint32_t *native_roots[] = {&native_moving, &native_fixed};
    io.native_roots = native_roots;
    io.native_root_count = 2u;

    uint32_t old_dstk = get_word(DSKTOP, 5u);
    uint32_t old_avmem = get_word(AVMEM, 5u);
    put_word(old_dstk, 5u, pair.object_b);

    /*
     * B ends exactly where A's backward-link word begins. Opening five
     * nibbles before that boundary requires two independent relative-link
     * corrections: HOME-chain->A and A-link->B.
     */
    assert(pair.object_b + 7u == pair.link_a);
    assert(hpclang_rpl_mutation_userob_shift_before(&io, pair.link_a, 5) ==
           HPCLANG_RPL_MUTATION_OK);

    assert(native_moving == pair.object_b - 5u);
    assert(native_fixed == pair.object_a);
    assert(get_word(DSKTOP, 5u) == old_dstk - 5u);
    assert(get_word(AVMEM, 5u) == old_avmem - 1u);
    assert(get_word(USEROB, 5u) == pair.home - 5u);
    assert(get_word(CONTEXT, 5u) == pair.home - 5u);
    assert(get_word(old_dstk - 5u, 5u) == pair.object_b - 5u);

    uint32_t moved_chain = pair.chain - 5u;
    assert(get_word(moved_chain, 5u) == pair.item_a - moved_chain);
    assert(get_word(pair.link_a, 5u) == pair.link_a - (pair.item_b - 5u));
    assert_char_at(pair.object_b - 5u, 'B');
    assert_char_at(pair.object_a, 'A');

    /*
     * The opened five-nibble interval is deliberately left to the higher
     * named-store operation. Closing that untouched interval must restore the
     * exact original geometry and pointer values.
     */
    assert(hpclang_rpl_mutation_userob_shift_before(&io, pair.link_a, -5) ==
           HPCLANG_RPL_MUTATION_OK);

    assert(native_moving == pair.object_b);
    assert(native_fixed == pair.object_a);
    assert(get_word(DSKTOP, 5u) == old_dstk);
    assert(get_word(AVMEM, 5u) == old_avmem);
    assert(get_word(USEROB, 5u) == pair.home);
    assert(get_word(CONTEXT, 5u) == pair.home);
    assert(get_word(old_dstk, 5u) == pair.object_b);
    assert(get_word(pair.chain, 5u) == pair.item_a - pair.chain);
    assert(get_word(pair.link_a, 5u) == pair.link_a - pair.item_b);
    assert_char_at(pair.object_b, 'B');
    assert_char_at(pair.object_a, 'A');
}

static void test_userob_shift_rejects_removed_reference(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct named_pair pair = prepare_two_named_chars();

    uint32_t old_dstk = get_word(DSKTOP, 5u);
    uint32_t old_avmem = get_word(AVMEM, 5u);
    uint32_t old_home = get_word(USEROB, 5u);
    uint32_t old_chain = get_word(pair.chain, 5u);
    uint32_t old_link = get_word(pair.link_a, 5u);

    put_word(old_dstk, 5u, pair.link_a - 1u);
    assert(hpclang_rpl_mutation_userob_shift_before(&io, pair.link_a, -2) ==
           HPCLANG_RPL_MUTATION_BUSY);

    /* An ARM-held launcher root must veto the same removal even when the
       ordinary data-stack reference points elsewhere. */
    put_word(old_dstk, 5u, pair.object_a);
    uint32_t native_root = pair.link_a - 1u;
    uint32_t *native_roots[] = {&native_root};
    io.native_roots = native_roots;
    io.native_root_count = 1u;
    assert(hpclang_rpl_mutation_userob_shift_before(&io, pair.link_a, -2) ==
           HPCLANG_RPL_MUTATION_BUSY);
    assert(native_root == pair.link_a - 1u);

    assert(get_word(DSKTOP, 5u) == old_dstk);
    assert(get_word(AVMEM, 5u) == old_avmem);
    assert(get_word(USEROB, 5u) == old_home);
    assert(get_word(pair.chain, 5u) == old_chain);
    assert(get_word(pair.link_a, 5u) == old_link);
}

static void test_retained_reference_does_not_hide_invalid_root(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct named_pair pair = prepare_two_named_chars();
    uint32_t retained = pair.object_b;
    uint32_t *roots[] = {&retained, NULL};
    io.native_roots = roots;
    io.native_root_count = 2u;
    uint8_t *before = malloc(sizeof(memory));
    assert(before != NULL);
    memcpy(before, memory, sizeof(memory));
    struct hpclang_object *value = make_string1('X');
    retained = pair.link_a - 1u;
    assert(hpclang_rpl_mutation_userob_shift_before(&io, pair.link_a, -2) ==
           HPCLANG_RPL_MUTATION_MALFORMED);
    retained = pair.object_b;
    assert(hpclang_rpl_mutation_store_replace(&io, pair.home, (const uint8_t *)"B", 1u, value) ==
           HPCLANG_RPL_MUTATION_MALFORMED);
    assert(hpclang_rpl_mutation_store_remove(&io, pair.home, (const uint8_t *)"B", 1u) ==
           HPCLANG_RPL_MUTATION_MALFORMED);
    assert(memcmp(before, memory, sizeof(memory)) == 0);
    assert(retained == pair.object_b);
    free(value);
    free(before);
}

static void test_userob_shift_nospace_is_non_mutating(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct named_pair pair = prepare_two_named_chars();

    uint32_t old_dstk = get_word(DSKTOP, 5u);
    uint32_t old_avmem = get_word(AVMEM, 5u);
    uint32_t old_home = get_word(USEROB, 5u);
    uint32_t old_chain = get_word(pair.chain, 5u);
    io.user_limit = old_dstk - 4u;

    assert(hpclang_rpl_mutation_userob_shift_before(&io, pair.link_a, 5) ==
           HPCLANG_RPL_MUTATION_NOSPACE);
    assert(get_word(DSKTOP, 5u) == old_dstk);
    assert(get_word(AVMEM, 5u) == old_avmem);
    assert(get_word(USEROB, 5u) == old_home);
    assert(get_word(pair.chain, 5u) == old_chain);
}

static void test_named_remove_oldest_preserves_reference(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct named_pair pair = prepare_two_named_chars();

    const uint32_t old_dstk = get_word(DSKTOP, 5u);
    put_word(old_dstk, 5u, pair.object_b);

    assert(hpclang_rpl_mutation_store_remove(&io, pair.home, (const uint8_t *)"B", 1u) ==
           HPCLANG_RPL_MUTATION_OK);

    const struct hpclang_rpl_reader reader = {
        .read_nibble = read_nibble,
        .context = NULL,
    };
    uint32_t new_home = get_word(USEROB, 5u);
    struct hpclang_rpl_directory_entry_view entry;
    assert(hpclang_rpl_directory_find(&reader, new_home, (const uint8_t *)"B", 1u, &entry) ==
           HPCLANG_RPL_STORE_NOT_FOUND);
    assert(hpclang_rpl_directory_find(&reader, new_home, (const uint8_t *)"A", 1u, &entry) ==
           HPCLANG_RPL_STORE_OK);
    assert_char_at(entry.object_address, 'A');

    const uint32_t removed_nibbles = (pair.object_b + 7u) - pair.link_b;
    const uint32_t new_dstk = old_dstk + removed_nibbles;
    assert(get_word(DSKTOP, 5u) == new_dstk);

    uint32_t preserved = get_word(new_dstk, 5u);
    assert(preserved != pair.object_b);
    assert_char_at(preserved, 'B');
}

/* One nested directory, with both ARM-held and Stock stack references. */
static void test_nested_mutation_preserves_context_and_references(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct named_pair pair = prepare_two_named_chars();
    const uint32_t home = pair.home;
    const uint32_t child = home + 24u;
    memmove(memory + child, memory + home, pair.object_a + 7u - home);
    memset(memory + home, 0, 24u);
    put_word(home, 5u, DORRP);
    put_word(home + 8u, 5u, 10u);
    put_word(home + 18u, 2u, 1u);
    put_word(home + 20u, 2u, 'D');
    put_word(home + 22u, 2u, 1u);
    put_word(CONTEXT, 5u, child);
    uint32_t retained = pair.object_b + 24u;
    uint32_t *roots[] = {&retained};
    io.native_roots = roots;
    io.native_root_count = 1u;
    put_word(get_word(DSKTOP, 5u), 5u, retained);
    const struct hpclang_rpl_reader reader = {.read_nibble = read_nibble};
    struct hpclang_rpl_directory_entry_view entry;
    struct hpclang_object *value = make_string1('X');

    assert(hpclang_rpl_mutation_store_remove(&io, child, (const uint8_t *)"B", 1u) ==
           HPCLANG_RPL_MUTATION_OK);
    assert_char_at(retained, 'B');
    assert(get_word(get_word(DSKTOP, 5u), 5u) == retained);

    for (unsigned step = 0u; step < 3u; ++step) {
        uint32_t current = get_word(CONTEXT, 5u);
        uint8_t path[8];
        uint32_t size = 0u, count = 0u;
        assert(hpclang_rpl_directory_path_to(&reader, current, path, sizeof(path), &size, &count) ==
               HPCLANG_RPL_STORE_OK);
        assert(count == 1u && size == 2u && path[0] == 1u && path[1] == 'D');
        assert(hpclang_rpl_directory_find(&reader, get_word(USEROB, 5u), (const uint8_t *)"D", 1u,
                                          &entry) == HPCLANG_RPL_STORE_OK);
        assert(entry.object_address == current);
        assert(hpclang_rpl_directory_find(&reader, current, (const uint8_t *)"B", 1u, &entry) ==
               HPCLANG_RPL_STORE_NOT_FOUND);
        if (step == 0u)
            assert(hpclang_rpl_mutation_store_replace(&io, current, (const uint8_t *)"A", 1u,
                                                      value) == HPCLANG_RPL_MUTATION_OK);
        else if (step == 1u)
            assert(hpclang_rpl_mutation_store_create(&io, current, (const uint8_t *)"C", 1u,
                                                     value) == HPCLANG_RPL_MUTATION_OK);
        else
            assert(hpclang_rpl_mutation_store_remove(&io, current, (const uint8_t *)"C", 1u) ==
                   HPCLANG_RPL_MUTATION_OK);
        assert_char_at(retained, 'B');
        assert(get_word(get_word(DSKTOP, 5u), 5u) == retained);
    }
    assert(hpclang_rpl_directory_find(&reader, get_word(CONTEXT, 5u), (const uint8_t *)"C", 1u,
                                      &entry) == HPCLANG_RPL_STORE_NOT_FOUND);
    assert(hpclang_rpl_directory_find(&reader, get_word(CONTEXT, 5u), (const uint8_t *)"A", 1u,
                                      &entry) == HPCLANG_RPL_STORE_OK);
    assert(get_word(entry.object_address, 5u) == 0x02a2cu);
    assert(get_word(entry.object_address + 10u, 2u) == 'X');
    free(value);
}

/* HPGCC3 sat3_dir.c __sat3_fixptrs2: exercise each root family, including
 * tables whose own
 * storage moves during TEMPOB preservation. */
static void prepare_reference_tables(const struct named_pair *pair)
{
    put_word(get_word(DSKTOP, 5u), 5u, pair->object_b);
    put_word(0x84ffbu, 5u, pair->object_b);
    put_word(0x84ff6u, 5u, 0u);
    memset(memory + 0x84400u, 0, 0x300u);
    put_word(TEMPENV, 5u, 0x84400u);
    put_word(0x84400u, 5u, 25u);
    put_word(0x8440au, 5u, pair->object_b);
    put_word(0x8440fu, 5u, pair->object_b + 5u);
    put_word(TOUCHTAB, 5u, 0x84500u);
    put_word(0x84503u, 5u, pair->object_b);
    put_word(0x84508u, 5u, pair->object_a);
    put_word(FSTVGERPTR, 5u, 0x84600u);
    put_word(0x84600u, 5u, 0x12345u); /* header is not a pointer cell */
    put_word(0x8460fu, 5u, 2u);
    put_word(0x84623u, 5u, pair->object_b);
    put_word(0x8463cu, 5u, pair->object_b + 5u);
    put_word(0x84641u, 5u, pair->object_a);
    put_word(0x80734u, 5u, pair->object_b);
}

static void test_all_reference_families_survive_preservation(void)
{
    for (uint32_t remove = 0; remove < 2; ++remove) {
        struct hpclang_rpl_mutation_io io = setup(1u);
        struct named_pair pair = prepare_two_named_chars();
        prepare_reference_tables(&pair);
        uint32_t native = pair.object_b;
        uint32_t *roots[] = {&native};
        io.native_roots = roots;
        io.native_root_count = 1u;
        struct hpclang_object *value = make_string1('X');
        int result =
            remove ? hpclang_rpl_mutation_store_remove(&io, pair.home, (const uint8_t *)"B", 1u)
                   : hpclang_rpl_mutation_store_replace(&io, pair.home, (const uint8_t *)"B", 1u,
                                                        value);
        assert(result == HPCLANG_RPL_MUTATION_OK);
        const uint32_t preserved = 0x84001u;
        const uint32_t delta = 13u; /* seven-nibble CHAR plus TEMP block overhead */
        assert(native == preserved);
        assert_char_at(preserved, 'B');
        assert(get_word(get_word(DSKTOP, 5u), 5u) == preserved);
        assert(get_word(0x84ffbu + delta, 5u) == preserved);
        assert(get_word(TEMPENV, 5u) == 0x84400u + delta);
        assert(get_word(0x8440au + delta, 5u) == preserved);
        assert(get_word(0x8440fu + delta, 5u) == preserved + 5u);
        assert(get_word(TOUCHTAB, 5u) == 0x84500u + delta);
        assert(get_word(0x84503u + delta, 5u) == preserved);
        assert(get_word(0x84508u + delta, 5u) == pair.object_a);
        assert(get_word(FSTVGERPTR, 5u) == 0x84600u + delta);
        assert(get_word(0x84600u + delta, 5u) == 0x12345u);
        assert(get_word(0x84623u + delta, 5u) == preserved);
        assert(get_word(0x8463cu + delta, 5u) == preserved + 5u);
        assert(get_word(0x84641u + delta, 5u) == pair.object_a);
        assert(get_word(0x80734u, 5u) == preserved);
        hpclang_object_release(value);
    }
}

static void test_reference_preflight_rejects_before_writing(void)
{
    for (uint32_t fault = 0; fault < 4; ++fault) {
        struct hpclang_rpl_mutation_io io = setup(1u);
        struct named_pair pair = prepare_two_named_chars();
        prepare_reference_tables(&pair);
        int expected = HPCLANG_RPL_MUTATION_MALFORMED;
        switch (fault) {
        case 0:
            put_word(0x84400u, 5u, 0xfffffu);
            break;
        case 1:
            put_word(TOUCHTAB, 5u, NIBBLES - 2u);
            break;
        case 2:
            put_word(0x8460fu, 5u, 257u);
            break;
        default:
            rejected_read = 0x84646u; /* late terminator after several retained roots */
            expected = HPCLANG_RPL_MUTATION_IO;
            break;
        }
        uint8_t *before = malloc(sizeof(memory));
        assert(before != NULL);
        memcpy(before, memory, sizeof(memory));
        struct hpclang_object *value = make_string1('X');
        assert(hpclang_rpl_mutation_store_replace(&io, pair.home, (const uint8_t *)"B", 1u,
                                                  value) == expected);
        assert(hpclang_rpl_mutation_store_remove(&io, pair.home, (const uint8_t *)"B", 1u) ==
               expected);
        assert(hpclang_rpl_mutation_store_create(&io, pair.home, (const uint8_t *)"C", 1u, value) ==
               expected);
        assert(writes == 0u);
        assert(memcmp(before, memory, sizeof(memory)) == 0);
        free(before);
        hpclang_object_release(value);
    }
}

static void test_impossible_free_memory_is_rejected(void)
{
    for (uint32_t large = 0; large < 2; ++large) {
        struct hpclang_rpl_mutation_io io = setup(1u);
        struct named_pair pair = prepare_two_named_chars();
        uint32_t cells = (get_word(DSKTOP, 5u) - get_word(RSKTOP, 5u)) / 5u;
        put_word(AVMEM, 5u, large ? 0xfffffu : cells + 1u);
        uint8_t *before = malloc(sizeof(memory));
        assert(before != NULL);
        memcpy(before, memory, sizeof(memory));
        struct hpclang_object *value = make_string1('X');
        assert(hpclang_rpl_mutation_store_create(&io, pair.home, (const uint8_t *)"C", 1u, value) ==
               HPCLANG_RPL_MUTATION_MALFORMED);
        assert(hpclang_rpl_mutation_store_replace(&io, pair.home, (const uint8_t *)"B", 1u,
                                                  value) == HPCLANG_RPL_MUTATION_MALFORMED);
        assert(hpclang_rpl_mutation_store_remove(&io, pair.home, (const uint8_t *)"B", 1u) ==
               HPCLANG_RPL_MUTATION_MALFORMED);
        assert(hpclang_rpl_mutation_push_snapshot(&io, value) == HPCLANG_RPL_MUTATION_MALFORMED);
        assert(hpclang_rpl_mutation_replace(&io, 1u, value) == HPCLANG_RPL_MUTATION_MALFORMED);
        assert(hpclang_rpl_mutation_drop(&io, 1u) == HPCLANG_RPL_MUTATION_MALFORMED);
        assert(writes == 0u && memcmp(before, memory, sizeof(memory)) == 0);
        free(before);
        hpclang_object_release(value);
    }
}

static void test_named_remove_newest(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct named_pair pair = prepare_two_named_chars();

    assert(hpclang_rpl_mutation_store_remove(&io, pair.home, (const uint8_t *)"A", 1u) ==
           HPCLANG_RPL_MUTATION_OK);

    const struct hpclang_rpl_reader reader = {
        .read_nibble = read_nibble,
        .context = NULL,
    };
    uint32_t new_home = get_word(USEROB, 5u);
    struct hpclang_rpl_directory_entry_view entry;
    assert(hpclang_rpl_directory_find(&reader, new_home, (const uint8_t *)"A", 1u, &entry) ==
           HPCLANG_RPL_STORE_NOT_FOUND);
    assert(hpclang_rpl_directory_find(&reader, new_home, (const uint8_t *)"B", 1u, &entry) ==
           HPCLANG_RPL_STORE_OK);
    assert_char_at(entry.object_address, 'B');
}

static void test_named_remove_missing_is_non_mutating(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct named_pair pair = prepare_two_named_chars();

    uint32_t old_dstk = get_word(DSKTOP, 5u);
    uint32_t old_home = get_word(USEROB, 5u);
    assert(hpclang_rpl_mutation_store_remove(&io, pair.home, (const uint8_t *)"Z", 1u) ==
           HPCLANG_RPL_MUTATION_NOT_FOUND);
    assert(get_word(DSKTOP, 5u) == old_dstk);
    assert(get_word(USEROB, 5u) == old_home);
}

struct reservation_fault
{
    unsigned calls, cut, mode, address;
};

static void test_recovery_reservation_binding(void)
{
    setup(1u);
    uint8_t identity[48] = "HPCROOT1";
    identity[9] = 2; /* 512 nibbles, owned descriptor at nibble 16. */
    identity[12] = 16;
    for (unsigned i = 16; i < sizeof identity; ++i)
        identity[i] = (uint8_t)(i * 3);
    const struct hpclang_rpl_flash_installed_recovery installed = {0x180105, 0x160105, 0x161121,
                                                                   0x140000};
    uint8_t header[HPCLANG_RPL_RECOVERY_RESERVATION_HEADER_BYTES];
    assert(hpclang_rpl_recovery_reservation_header(identity, &installed, 0x12345678, 512, header));
    static const uint8_t coordinates[24] = {5, 1, 24, 0, 5,   1,  22, 0,  33, 17, 22, 0,
                                            0, 0, 20, 0, 120, 86, 52, 18, 0,  2,  0,  0};
    assert(sizeof header == 80 && !memcmp(header, "HPCMEM2", 8));
    assert(!memcmp(header + 8, identity, 48) && !memcmp(header + 56, coordinates, 24));
    uint8_t output[sizeof header];
    memset(output, 0xa5, sizeof output);
    const uint32_t invalid[] = {0, 79, UINT32_MAX};
    for (unsigned i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        assert(
            !hpclang_rpl_recovery_reservation_header(identity, &installed, 0, invalid[i], output));
        for (unsigned j = 0; j < sizeof output; ++j)
            assert(output[j] == 0xa5);
    }
    identity[12] = 1;
    assert(!hpclang_rpl_recovery_reservation_header(identity, &installed, 0, 512, output));
    identity[12] = 16;
    const struct hpclang_rpl_reader reader = {read_nibble, NULL};
    const unsigned object = 0x90000;
    put_word(object, 5, 0x2a2c);
    put_word(object + 5, 5, 1029);
    for (unsigned i = 0; i < sizeof header; ++i)
        put_word(object + 10 + 2 * i, 2, header[i]);
    assert(hpclang_rpl_recovery_reservation_match(&reader, object, header) == 1);
    /* Every nibble of identity, coordinates, transaction CRC and capacity matters. */
    for (unsigned i = 0; i < 2 * sizeof header; ++i) {
        memory[object + 10 + i] ^= 1;
        assert(hpclang_rpl_recovery_reservation_match(&reader, object, header) == 0);
        memory[object + 10 + i] ^= 1;
    }
    for (unsigned i = 0; i < 10 + 2 * sizeof header; ++i) {
        rejected_read = object + i;
        assert(hpclang_rpl_recovery_reservation_match(&reader, object, header) == -1);
    }
    rejected_read = UINT32_MAX;
    put_word(object, 5, 0x29bf);
    assert(hpclang_rpl_recovery_reservation_match(&reader, object, header) == 0);
    put_word(object, 5, 0x2a2c);
    put_word(object + 5, 5, 1031);
    assert(hpclang_rpl_recovery_reservation_match(&reader, object, header) == 0);
    put_word(object + 5, 5, 1029);
    uint8_t previous[sizeof header];
    assert(
        hpclang_rpl_recovery_reservation_header(identity, &installed, 0x12345679, 512, previous));
    assert(hpclang_rpl_recovery_reservation_match(&reader, object, previous) == 0);
    identity[16] ^= 1;
    assert(
        hpclang_rpl_recovery_reservation_header(identity, &installed, 0x12345678, 512, previous));
    assert(hpclang_rpl_recovery_reservation_match(&reader, object, previous) == 0);
    assert(hpclang_rpl_recovery_reservation_match(&reader, 0xfffff, header) == -1);
    header[8] ^= 1;
    assert(hpclang_rpl_recovery_reservation_match(&reader, object, header) == -1);
    assert(writes == 0);
}

static int reservation_write(void *context, uint32_t address, uint8_t value)
{
    struct reservation_fault *fault = context;
    int result = write_nibble(NULL, address, value);
    if (++fault->calls == fault->cut) {
        if (!fault->mode)
            return 0; /* Lost acknowledgement after the actual write. */
        if (fault->mode == 1)
            memory[fault->address] ^= 1u;
        else
            rejected_read = fault->address;
    }
    return result;
}

static struct named_pair prepare_reservation_home(void)
{
    struct named_pair pair = prepare_two_named_chars();
    /* Insert the native oldest null-named directory before the two variables. */
    memmove(memory + pair.link_b + 20u, memory + pair.link_b, pair.object_a + 7u - pair.link_b);
    memset(memory + pair.link_b, 0, 20u);
    pair.link_a += 20u;
    pair.item_a += 20u;
    pair.object_a += 20u;
    pair.link_b += 20u;
    pair.item_b += 20u;
    pair.object_b += 20u;
    put_word(pair.chain, 5u, pair.item_a - pair.chain);
    put_word(pair.link_b, 5u, pair.link_b - (pair.chain + 10u));
    put_word(pair.chain + 12u, 5u, DORRP);
    put_word(pair.chain + 17u, 3u, 0x7ffu);
    return pair;
}

static void test_recovery_reservation_create(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct named_pair pair = prepare_reservation_home();
    struct hpclang_object *snapshot = make_string1('R');
    const struct hpclang_rpl_recovery_reservation sentinel = {123u, 456u};
    struct hpclang_rpl_recovery_reservation created = sentinel;
    struct reservation_fault fault = {0};
    io.context = &fault;
    io.write_nibble = reservation_write;
    assert(hpclang_rpl_recovery_reservation_create(&io, (const uint8_t *)"C", 1, snapshot,
                                                   &created) == HPCLANG_RPL_MUTATION_OK);
    unsigned count = fault.calls, object = created.object;
    assert(count > 0 && created.capacity == 1u && get_word(object + 10u, 2) == 'R');
    assert(get_word(USEROB, 5) < pair.home);
    /* Both same-content and different-type existing objects remain conflicts. */
    created = sentinel;
    assert(hpclang_rpl_recovery_reservation_create(&io, (const uint8_t *)"C", 1, snapshot,
                                                   &created) == HPCLANG_RPL_MUTATION_EXISTS);
    assert(hpclang_rpl_recovery_reservation_create(&io, (const uint8_t *)"B", 1, snapshot,
                                                   &created) == HPCLANG_RPL_MUTATION_EXISTS);
    assert(fault.calls == count && !memcmp(&created, &sentinel, sizeof created));
    for (unsigned cut = 1; cut <= count; ++cut) {
        io = setup(1u);
        prepare_reservation_home();
        fault = (struct reservation_fault){.cut = cut};
        io.context = &fault;
        io.write_nibble = reservation_write;
        assert(hpclang_rpl_recovery_reservation_create(&io, (const uint8_t *)"C", 1, snapshot,
                                                       &created) ==
               HPCLANG_RPL_MUTATION_INCOMPLETE);
        assert(fault.calls == cut && !memcmp(&created, &sentinel, sizeof created));
    }
    /* Success acknowledgements cannot hide a bad payload or failed readback. */
    for (unsigned mode = 1; mode <= 2; ++mode) {
        io = setup(1u);
        prepare_reservation_home();
        fault = (struct reservation_fault){.cut = count, .mode = mode, .address = object + 10u};
        io.context = &fault;
        io.write_nibble = reservation_write;
        assert(hpclang_rpl_recovery_reservation_create(&io, (const uint8_t *)"C", 1, snapshot,
                                                       &created) ==
               HPCLANG_RPL_MUTATION_INCOMPLETE);
        assert(fault.calls == count && !memcmp(&created, &sentinel, sizeof created));
    }
    io = setup(1u);
    prepare_reservation_home();
    rejected_read = USEROB;
    assert(hpclang_rpl_recovery_reservation_create(&io, (const uint8_t *)"C", 1, snapshot,
                                                   &created) == HPCLANG_RPL_MUTATION_IO);
    assert(writes == 0 && !memcmp(&created, &sentinel, sizeof created));
    rejected_read = UINT32_MAX;
    snapshot->data[2] ^= 0x10u; /* The native length must agree with the snapshot. */
    assert(hpclang_rpl_recovery_reservation_create(&io, (const uint8_t *)"C", 1, snapshot,
                                                   &created) == HPCLANG_RPL_MUTATION_MALFORMED);
    assert(writes == 0 && !memcmp(&created, &sentinel, sizeof created));
    hpclang_object_release(snapshot);
}

static void test_recovery_reservation_discard_created(void)
{
    struct hpclang_object *snapshot = make_string1('R');
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct named_pair pair = prepare_reservation_home();
    struct hpclang_rpl_recovery_reservation created;
    assert(hpclang_rpl_recovery_reservation_create(&io, (const uint8_t *)"C", 1, snapshot,
                                                   &created) == HPCLANG_RPL_MUTATION_OK);
    static uint8_t original[NIBBLES];
    memcpy(original, memory, sizeof memory);
    unsigned before = writes;
    const struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
    const struct hpclang_rpl_recovery_reservation initial = created;
    assert(hpclang_rpl_recovery_reservation_check_created(
               &reader, (const uint8_t *)"C", 1, snapshot, &created) == HPCLANG_RPL_MUTATION_OK);
    assert(!memcmp(&created, &initial, sizeof created));
    assert(writes == before && !memcmp(original, memory, sizeof memory));
    for (unsigned fault = 0; fault < 6; ++fault) {
        struct hpclang_rpl_recovery_reservation receipt = created;
        const uint8_t *name = (const uint8_t *)"C";
        int expected = HPCLANG_RPL_MUTATION_EXISTS;
        if (fault == 0)
            --receipt.object;
        else if (fault == 1)
            ++receipt.capacity;
        else if (fault == 2)
            snapshot->data[5] ^= 1u;
        else if (fault == 3)
            name = (const uint8_t *)"B";
        else if (fault == 4) {
            rejected_read = created.object + 10u;
            expected = HPCLANG_RPL_MUTATION_IO;
        } else {
            name = (const uint8_t *)"D";
            expected = HPCLANG_RPL_MUTATION_NOT_FOUND;
        }
        const struct hpclang_rpl_recovery_reservation sentinel = receipt;
        assert(hpclang_rpl_recovery_reservation_check_created(&reader, name, 1, snapshot,
                                                              &receipt) == expected);
        assert(!memcmp(&receipt, &sentinel, sizeof receipt));
        assert(writes == before && !memcmp(original, memory, sizeof memory));
        assert(hpclang_rpl_recovery_reservation_discard_created(&io, name, 1, snapshot, &receipt) ==
               expected);
        assert(!memcmp(&receipt, &sentinel, sizeof receipt));
        assert(writes == before && !memcmp(original, memory, sizeof memory));
        if (fault == 2)
            snapshot->data[5] ^= 1u;
        rejected_read = UINT32_MAX;
    }
    struct reservation_fault fault = {0};
    io.context = &fault;
    io.write_nibble = reservation_write;
    assert(hpclang_rpl_recovery_reservation_discard_created(&io, (const uint8_t *)"C", 1, snapshot,
                                                            &created) == HPCLANG_RPL_MUTATION_OK);
    assert(!created.object && !created.capacity);
    assert(hpclang_rpl_recovery_reservation_check_created(&reader, (const uint8_t *)"C", 1,
                                                          snapshot, &created) ==
           HPCLANG_RPL_MUTATION_UNAVAILABLE);
    assert(get_word(USEROB, 5) == pair.home);
    unsigned count = fault.calls;
    assert(count > 0);
    assert(hpclang_rpl_recovery_reservation_discard_created(&io, (const uint8_t *)"C", 1, snapshot,
                                                            &created) ==
           HPCLANG_RPL_MUTATION_UNAVAILABLE);
    assert(fault.calls == count);
    for (unsigned cut = 1; cut <= count; ++cut) {
        io = setup(1u);
        prepare_reservation_home();
        assert(hpclang_rpl_recovery_reservation_create(&io, (const uint8_t *)"C", 1, snapshot,
                                                       &created) == HPCLANG_RPL_MUTATION_OK);
        const struct hpclang_rpl_recovery_reservation sentinel = created;
        fault = (struct reservation_fault){.cut = cut};
        io.context = &fault;
        io.write_nibble = reservation_write;
        assert(hpclang_rpl_recovery_reservation_discard_created(&io, (const uint8_t *)"C", 1,
                                                                snapshot, &created) ==
               HPCLANG_RPL_MUTATION_INCOMPLETE);
        assert(fault.calls == cut && !memcmp(&created, &sentinel, sizeof created));
    }
    io = setup(1u);
    prepare_reservation_home();
    assert(hpclang_rpl_recovery_reservation_create(&io, (const uint8_t *)"C", 1, snapshot,
                                                   &created) == HPCLANG_RPL_MUTATION_OK);
    const struct hpclang_rpl_recovery_reservation retained = created;
    fault = (struct reservation_fault){.cut = count, .mode = 2, .address = USEROB};
    io.context = &fault;
    io.write_nibble = reservation_write;
    assert(hpclang_rpl_recovery_reservation_discard_created(&io, (const uint8_t *)"C", 1, snapshot,
                                                            &created) ==
           HPCLANG_RPL_MUTATION_INCOMPLETE);
    assert(!memcmp(&created, &retained, sizeof created));
    hpclang_object_release(snapshot);
}

static uint8_t creation_flash[244];
static unsigned creation_programs, creation_failure;
static int creation_read(void *context, uint32_t at, uint16_t *value)
{
    assert(context == creation_flash && at >= 0x1e0104 &&
           at + 1 < 0x1e0104 + sizeof creation_flash);
    if (creation_failure == 1)
        return 0;
    unsigned offset = at - 0x1e0104;
    *value = creation_flash[offset] | (uint16_t)creation_flash[offset + 1] << 8;
    return 1;
}
static int creation_program(void *context, uint32_t at, uint16_t value)
{
    assert(context == creation_flash && at == 0x1e0114);
    ++creation_programs;
    if (creation_failure != 2) {
        creation_flash[at - 0x1e0104] &= value;
        creation_flash[at - 0x1e0104 + 1] &= value >> 8;
    }
    return creation_failure != 2 && creation_failure != 3;
}

static void test_recovery_creation_commit(void)
{
    uint8_t identity[48] = "HPCROOT1", header[80], intent[240], saved_flash[244];
    identity[9] = 2;
    identity[12] = 16;
    for (unsigned i = 16; i < sizeof identity; ++i)
        identity[i] = (uint8_t)(i * 3);
    const struct hpclang_rpl_flash_installed_recovery installed = {0x180105, 0x160105, 0x161121,
                                                                   0x140000};
    const struct hpclang_rpl_flash_io flash = {creation_flash, creation_read, creation_program,
                                               0x140000, 0x200000};
    static uint8_t saved_ram[NIBBLES];
    for (unsigned fault = 0; fault < 10; ++fault) {
        struct hpclang_rpl_mutation_io io = setup(1);
        prepare_reservation_home();
        assert(
            hpclang_rpl_recovery_reservation_header(identity, &installed, 0x12345678, 80, header));
        struct hpclang_object *snapshot = malloc(sizeof(*snapshot) + 85);
        assert(snapshot);
        snapshot->kind = HPCLANG_OBJECT_STRING;
        snapshot->native_prologue = 0x2a2c;
        snapshot->nibble_count = 170;
        memset(snapshot->data, 0, 85);
        for (unsigned i = 0; i < 10; ++i) {
            uint32_t value = i < 5 ? 0x2a2c : 165;
            snapshot->data[i / 2] |= ((value >> (4 * (i % 5))) & 15) << (4 * (i & 1));
        }
        memcpy(snapshot->data + 5, header, sizeof header);
        struct hpclang_rpl_recovery_reservation receipt;
        assert(!hpclang_rpl_recovery_reservation_create(&io, (const uint8_t *)"C", 1, snapshot,
                                                        &receipt));
        const uint8_t *name = (const uint8_t *)(fault == 3 ? "D" : "C");
        if (fault == 4)
            header[24] ^= 1;
        assert(!hpclang_rpl_flash_creation_encode(0x140000, 0x200000, 0x1e0105, header, name, 1,
                                                  intent, sizeof intent));
        memset(creation_flash, 255, sizeof creation_flash);
        memcpy(creation_flash + 1, intent, sizeof intent);
        if (fault == 1)
            ++receipt.capacity;
        if (fault == 2)
            snapshot->data[84] ^= 1;
        if (fault == 5)
            creation_flash[24] ^= 1;
        if (fault == 9)
            creation_flash[17] = 0x7e;
        creation_failure = fault >= 6 && fault <= 8 ? fault - 5 : 0;
        creation_programs = 0;
        memcpy(saved_flash, creation_flash, sizeof saved_flash);
        memcpy(saved_ram, memory, sizeof memory);
        const struct hpclang_rpl_recovery_reservation saved_receipt = receipt;
        const struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
        int result = hpclang_rpl_recovery_reservation_complete_creation(
            &flash, 0x1e0105, intent, sizeof intent, &reader, snapshot, &receipt);
        assert(result == (!fault || fault == 9 ? 0 : HPCLANG_RPL_FLASH_DIRTY));
        assert(!memcmp(saved_ram, memory, sizeof memory));
        assert(!memcmp(&saved_receipt, &receipt, sizeof receipt));
        if (!fault || fault >= 7) {
            assert(creation_programs == (fault == 9 ? 0 : 1));
            creation_failure = creation_programs = 0;
            assert(!hpclang_rpl_recovery_reservation_complete_creation(
                &flash, 0x1e0105, intent, sizeof intent, &reader, snapshot, &receipt));
            assert(creation_programs == (fault == 7 ? 1 : 0));
            assert(creation_flash[17] == 0x7e);
            saved_flash[17] = 0x7e;
        } else {
            assert(!creation_programs);
        }
        assert(!memcmp(saved_flash, creation_flash, sizeof saved_flash));
        hpclang_object_release(snapshot);
    }
}

static void test_recovery_reservation_hidden_preflight(void)
{
    struct hpclang_object *snapshot = make_string1('R');
    const struct hpclang_rpl_recovery_reservation sentinel = {123u, 456u};
    struct hpclang_rpl_recovery_reservation created = sentinel;
    static uint8_t original[NIBBLES];
    for (unsigned fault = 0; fault < 27; ++fault) {
        struct hpclang_rpl_mutation_io io = setup(1u);
        struct named_pair pair = prepare_reservation_home();
        int expected = HPCLANG_RPL_MUTATION_IO;
        if (fault < 3)
            rejected_read = pair.home + 5u + fault;
        else if (fault < 20)
            rejected_read = pair.chain + fault - 3u;
        else if (fault == 20) {
            put_word(pair.chain, 5u, 0u);
            expected = HPCLANG_RPL_MUTATION_UNSUPPORTED;
        } else if (fault == 21) {
            put_word(pair.chain + 10u, 2u, 1u);
            expected = HPCLANG_RPL_MUTATION_UNSUPPORTED;
        } else if (fault == 22) {
            put_word(pair.chain + 12u, 5u, 0x02a2cu);
            expected = HPCLANG_RPL_MUTATION_UNSUPPORTED;
        } else if (fault == 23) {
            put_word(pair.chain + 5u, 5u, 1u);
            expected = HPCLANG_RPL_MUTATION_MALFORMED;
        } else if (fault == 24) {
            put_word(pair.chain, 5u, 0xfffffu);
            expected = HPCLANG_RPL_MUTATION_MALFORMED;
        } else if (fault == 25) {
            put_word(USEROB, 5u, 0xff000u);
            put_word(0xff000u, 5u, DORRP);
            put_word(0xff005u, 3u, 0xfffu);
            expected = HPCLANG_RPL_MUTATION_MALFORMED;
        } else {
            put_word(USEROB, 5u, 0xffff0u);
            put_word(0xffff0u, 5u, DORRP);
            expected = HPCLANG_RPL_MUTATION_MALFORMED;
        }
        memcpy(original, memory, sizeof memory);
        assert(hpclang_rpl_recovery_reservation_create(&io, (const uint8_t *)"C", 1u, snapshot,
                                                       &created) == expected);
        assert(!writes && !memcmp(original, memory, sizeof memory));
        assert(!memcmp(&created, &sentinel, sizeof created));
    }
    hpclang_object_release(snapshot);
}

static void test_named_create(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct named_pair pair = prepare_two_named_chars();
    struct hpclang_object *char_c = make_char('C');

    const uint32_t old_dstk = get_word(DSKTOP, 5u);
    put_word(old_dstk, 5u, pair.object_b);

    const uint32_t entry_nibbles = 18u;
    assert(hpclang_rpl_mutation_store_create(&io, pair.home, (const uint8_t *)"C", 1u, char_c) ==
           HPCLANG_RPL_MUTATION_OK);

    const uint32_t new_home = get_word(USEROB, 5u);
    const uint32_t new_dstk = get_word(DSKTOP, 5u);
    assert(new_home == pair.home - entry_nibbles);
    assert(new_dstk == old_dstk - entry_nibbles);
    assert(get_word(new_dstk, 5u) == pair.object_b - entry_nibbles);

    const struct hpclang_rpl_reader reader = {
        .read_nibble = read_nibble,
        .context = NULL,
    };
    struct hpclang_rpl_directory_entry_view entry;
    assert(hpclang_rpl_directory_find(&reader, new_home, (const uint8_t *)"C", 1u, &entry) ==
           HPCLANG_RPL_STORE_OK);
    assert(entry.prologue == 0x029bfu);
    assert_char_at(entry.object_address, 'C');

    uint32_t committed_dstk = get_word(DSKTOP, 5u);
    assert(hpclang_rpl_mutation_store_create(&io, new_home, (const uint8_t *)"B", 1u, char_c) ==
           HPCLANG_RPL_MUTATION_EXISTS);
    assert(get_word(DSKTOP, 5u) == committed_dstk);

    hpclang_object_release(char_c);
}

static void test_named_create_nospace_is_non_mutating(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct named_pair pair = prepare_two_named_chars();
    struct hpclang_object *char_c = make_char('C');

    const uint32_t old_dstk = get_word(DSKTOP, 5u);
    const uint32_t old_home = get_word(USEROB, 5u);
    io.user_limit = old_dstk - 17u;

    assert(hpclang_rpl_mutation_store_create(&io, pair.home, (const uint8_t *)"C", 1u, char_c) ==
           HPCLANG_RPL_MUTATION_NOSPACE);
    assert(get_word(DSKTOP, 5u) == old_dstk);
    assert(get_word(USEROB, 5u) == old_home);

    hpclang_object_release(char_c);
}

static void test_named_size_changing_replace(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct named_pair pair = prepare_two_named_chars();
    struct hpclang_object *string_b = make_string1('B');
    struct hpclang_object *char_b = make_char('B');

    uint32_t old_dstk = get_word(DSKTOP, 5u);
    uint32_t old_avmem = get_word(AVMEM, 5u);

    assert(hpclang_rpl_mutation_store_replace(&io, pair.home, (const uint8_t *)"B", 1u, string_b) ==
           HPCLANG_RPL_MUTATION_OK);

    uint32_t expanded_home = get_word(USEROB, 5u);
    assert(expanded_home == pair.home - 5u);
    assert(get_word(DSKTOP, 5u) == old_dstk - 5u);
    assert(get_word(AVMEM, 5u) == old_avmem - 1u);

    const struct hpclang_rpl_reader reader = {
        .read_nibble = read_nibble,
        .context = NULL,
    };
    struct hpclang_rpl_directory_entry_view entry;
    assert(hpclang_rpl_directory_find(&reader, expanded_home, (const uint8_t *)"B", 1u, &entry) ==
           HPCLANG_RPL_STORE_OK);
    assert(entry.object_address == pair.object_b - 5u);
    assert(entry.prologue == 0x02a2cu);

    uint32_t span = 0u;
    assert(hpclang_rpl_object_span(&reader, entry.object_address, NIBBLES - entry.object_address,
                                   &span) == HPCLANG_RPL_SPAN_OK);
    assert(span == string_b->nibble_count);
    assert(entry.object_address + span == pair.link_a);

    assert(hpclang_rpl_mutation_store_replace(&io, expanded_home, (const uint8_t *)"B", 1u,
                                              char_b) == HPCLANG_RPL_MUTATION_OK);

    assert(get_word(USEROB, 5u) == pair.home);
    assert(get_word(DSKTOP, 5u) == old_dstk);
    assert(get_word(AVMEM, 5u) == old_avmem);
    assert_char_at(pair.object_b, 'B');

    hpclang_object_release(string_b);
    hpclang_object_release(char_b);
}

static void test_named_size_change_preserves_reference(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct named_pair pair = prepare_two_named_chars();
    struct hpclang_object *string_b = make_string1('B');

    uint32_t old_dstk = get_word(DSKTOP, 5u);
    put_word(old_dstk, 5u, pair.object_b);

    assert(hpclang_rpl_mutation_store_replace(&io, pair.home, (const uint8_t *)"B", 1u, string_b) ==
           HPCLANG_RPL_MUTATION_OK);

    uint32_t new_dstk = get_word(DSKTOP, 5u);
    assert(new_dstk == old_dstk - 5u);
    uint32_t preserved = get_word(new_dstk, 5u);
    assert(preserved != pair.object_b - 5u);
    assert_char_at(preserved, 'B');

    const struct hpclang_rpl_reader reader = {
        .read_nibble = read_nibble,
        .context = NULL,
    };
    uint32_t expanded_home = get_word(USEROB, 5u);
    struct hpclang_rpl_directory_entry_view entry;
    assert(hpclang_rpl_directory_find(&reader, expanded_home, (const uint8_t *)"B", 1u, &entry) ==
           HPCLANG_RPL_STORE_OK);
    assert(entry.prologue == 0x02a2cu);

    hpclang_object_release(string_b);
}

static void test_named_same_size_replace(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    uint32_t object_address = prepare_named_char('A');
    struct hpclang_object *char_b = make_char('B');
    struct hpclang_object *char_c = make_char('C');

    assert(hpclang_rpl_mutation_store_replace(&io, 0x92000u, (const uint8_t *)"A", 1u, char_b) ==
           HPCLANG_RPL_MUTATION_OK);
    assert_char_at(object_address, 'B');

    const uint32_t dstk = get_word(DSKTOP, 5u);
    put_word(dstk, 5u, object_address);
    assert(hpclang_rpl_mutation_store_replace(&io, 0x92000u, (const uint8_t *)"A", 1u, char_c) ==
           HPCLANG_RPL_MUTATION_OK);
    assert_char_at(object_address, 'C');

    uint32_t preserved = get_word(dstk, 5u);
    assert(preserved != object_address);
    assert_char_at(preserved, 'B');

    assert(hpclang_rpl_mutation_store_replace(&io, 0x92000u, (const uint8_t *)"A", 1u, char_c) ==
           HPCLANG_RPL_MUTATION_OK);
    assert_char_at(object_address, 'C');
    assert(get_word(dstk, 5u) == preserved);
    assert_char_at(preserved, 'B');

    assert(hpclang_rpl_mutation_store_replace(&io, 0x92000u, (const uint8_t *)"NO", 2u, char_b) ==
           HPCLANG_RPL_MUTATION_NOT_FOUND);

    hpclang_object_release(char_b);
    hpclang_object_release(char_c);
}

static void test_named_reference_preservation_nospace_is_non_mutating(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    uint32_t object_address = prepare_named_char('B');
    struct hpclang_object *char_c = make_char('C');

    const uint32_t dstk = get_word(DSKTOP, 5u);
    put_word(dstk, 5u, object_address);
    io.temp_limit = get_word(RSKTOP, 5u) + 17u;

    assert(hpclang_rpl_mutation_store_replace(&io, 0x92000u, (const uint8_t *)"A", 1u, char_c) ==
           HPCLANG_RPL_MUTATION_NOSPACE);
    assert_char_at(object_address, 'B');
    assert(get_word(dstk, 5u) == object_address);

    hpclang_object_release(char_c);
}

static void test_temp_allocation_retargets_existing_reference(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct hpclang_object *object = make_char('N');

    const uint32_t old_ttop = get_word(TEMPTOP, 5u);
    const uint32_t old_temp_object = old_ttop + 0x20u;
    put_word(old_temp_object, 5u, 0x029bfu);
    put_word(old_temp_object + 5u, 2u, (uint32_t)'T');

    const uint32_t dstk = get_word(DSKTOP, 5u);
    put_word(dstk, 5u, old_temp_object);

    const uint32_t delta = object->nibble_count + 6u;
    assert(hpclang_rpl_mutation_push_snapshot(&io, object) == HPCLANG_RPL_MUTATION_OK);

    const uint32_t new_dstk = get_word(DSKTOP, 5u);
    assert(new_dstk == dstk - 5u);
    /*
     * The pre-existing level moved from level 1 to level 2 when push inserted
     * the new object. Its pointer must follow the old TEMP object upward.
     */
    assert(get_word(new_dstk + 5u, 5u) == old_temp_object + delta);
    assert_char_at(old_temp_object + delta, 'T');

    hpclang_object_release(object);
}

static void test_push_transaction(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct hpclang_object *object = make_char('A');

    uint8_t original[0x1000];
    memcpy(original, &memory[0x84000u], sizeof(original));

    assert(hpclang_rpl_mutation_push_snapshot(&io, object) == HPCLANG_RPL_MUTATION_OK);

    const uint32_t delta = object->nibble_count + 6u;
    const uint32_t object_address = 0x84001u;
    assert(get_word(TEMPTOP, 5u) == 0x84000u + delta);
    assert(get_word(RSKTOP, 5u) == 0x85000u + delta);
    assert(get_word(DSKTOP, 5u) == 0x8fffbu);
    assert(get_word(0x8fffbu, 5u) == object_address);
    assert(get_word(0x84000u + 1u + object->nibble_count, 5u) == delta);
    assert_char_at(object_address, 'A');

    /* The old TEMPOB..RSKTOP region moved upward intact before object copy. */
    for (uint32_t i = 0u; i < sizeof(original); ++i)
        assert(memory[0x84000u + delta + i] == original[i]);

    assert(get_word(AVMEM, 5u) == (0x8fffbu - (0x85000u + delta)) / 5u);

    hpclang_object_release(object);
}

static void test_named_preservation_accounting(void)
{
    for (uint32_t residue = 0; residue < 5; ++residue) {
        for (uint32_t remove = 0; remove < 2; ++remove) {
            struct hpclang_rpl_mutation_io io = setup(1u);
            struct named_pair pair = prepare_two_named_chars();
            uint32_t dstk = get_word(DSKTOP, 5u) + residue;
            put_word(DSKTOP, 5u, dstk);
            put_word(EDITLINE, 5u, dstk + 10u);
            put_word(dstk, 5u, pair.object_b);
            const uint32_t reserved = 7u;
            put_word(AVMEM, 5u, (dstk - get_word(RSKTOP, 5u)) / 5u - reserved);
            struct hpclang_object *value = make_string1('X');
            int result =
                remove ? hpclang_rpl_mutation_store_remove(&io, pair.home, (const uint8_t *)"B", 1u)
                       : hpclang_rpl_mutation_store_replace(&io, pair.home, (const uint8_t *)"B",
                                                            1u, value);
            assert(result == HPCLANG_RPL_MUTATION_OK);
            uint32_t final_cells = (get_word(DSKTOP, 5u) - get_word(RSKTOP, 5u)) / 5u;
            assert(get_word(AVMEM, 5u) == final_cells - reserved);
            assert_char_at(get_word(get_word(DSKTOP, 5u), 5u), 'B');
            hpclang_object_release(value);
        }
    }
}

static void test_avmem_reservation_is_preserved(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct hpclang_object *object = make_char('R');

    uint32_t old_avmem = get_word(AVMEM, 5u) - 7u;
    put_word(AVMEM, 5u, old_avmem);

    uint32_t rstk = get_word(RSKTOP, 5u);
    uint32_t dstk = get_word(DSKTOP, 5u);
    uint32_t delta = object->nibble_count + 6u;
    uint32_t old_cells = (dstk - rstk) / 5u;
    uint32_t new_cells = (dstk - (rstk + delta)) / 5u;
    uint32_t temp_cost = old_cells - new_cells;

    assert(hpclang_rpl_mutation_push_snapshot(&io, object) == HPCLANG_RPL_MUTATION_OK);
    assert(get_word(AVMEM, 5u) == old_avmem - temp_cost - 1u);

    io = setup(1u);
    old_avmem = get_word(AVMEM, 5u) - 9u;
    put_word(AVMEM, 5u, old_avmem);
    rstk = get_word(RSKTOP, 5u);
    dstk = get_word(DSKTOP, 5u);
    old_cells = (dstk - rstk) / 5u;
    new_cells = (dstk - (rstk + delta)) / 5u;
    temp_cost = old_cells - new_cells;

    assert(hpclang_rpl_mutation_replace(&io, 1u, object) == HPCLANG_RPL_MUTATION_OK);
    assert(get_word(AVMEM, 5u) == old_avmem - temp_cost);

    hpclang_object_release(object);
}

static void test_stack_reserve_blocks_last_cell(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct hpclang_object *object = make_char('S');

    uint32_t dstk = get_word(DSKTOP, 5u);
    io.user_limit = dstk - 5u;
    io.stack_reserve_nibbles = 5u;

    uint32_t before_ttop = get_word(TEMPTOP, 5u);
    uint32_t before_rstk = get_word(RSKTOP, 5u);
    uint32_t before_dstk = get_word(DSKTOP, 5u);
    assert(hpclang_rpl_mutation_push_snapshot(&io, object) == HPCLANG_RPL_MUTATION_NOSPACE);
    assert(get_word(TEMPTOP, 5u) == before_ttop);
    assert(get_word(RSKTOP, 5u) == before_rstk);
    assert(get_word(DSKTOP, 5u) == before_dstk);

    hpclang_object_release(object);
}

static void test_nospace_is_non_mutating(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct hpclang_object *object = make_char('B');

    io.temp_limit = 0x85010u;
    uint32_t ttop = get_word(TEMPTOP, 5u);
    uint32_t rstk = get_word(RSKTOP, 5u);
    uint32_t dstk = get_word(DSKTOP, 5u);
    uint32_t avmem = get_word(AVMEM, 5u);

    assert(hpclang_rpl_mutation_push_snapshot(&io, object) == HPCLANG_RPL_MUTATION_NOSPACE);
    assert(get_word(TEMPTOP, 5u) == ttop);
    assert(get_word(RSKTOP, 5u) == rstk);
    assert(get_word(DSKTOP, 5u) == dstk);
    assert(get_word(AVMEM, 5u) == avmem);

    io = setup(1u);
    io.user_limit = 0x8fffcu;
    ttop = get_word(TEMPTOP, 5u);
    rstk = get_word(RSKTOP, 5u);
    assert(hpclang_rpl_mutation_push_snapshot(&io, object) == HPCLANG_RPL_MUTATION_NOSPACE);
    assert(get_word(TEMPTOP, 5u) == ttop);
    assert(get_word(RSKTOP, 5u) == rstk);

    hpclang_object_release(object);
}

static void test_drop_and_replace(void)
{
    struct hpclang_rpl_mutation_io io = setup(2u);
    struct hpclang_object *object = make_char('C');

    uint32_t old_top = get_word(DSKTOP, 5u);
    assert(hpclang_rpl_mutation_replace(&io, 2u, object) == HPCLANG_RPL_MUTATION_OK);
    assert(get_word(DSKTOP, 5u) == old_top);

    uint32_t replacement = get_word(old_top + 5u, 5u);
    assert(replacement == 0x84001u);
    assert_char_at(replacement, 'C');

    uint32_t avmem = get_word(AVMEM, 5u);
    assert(hpclang_rpl_mutation_drop(&io, 2u) == HPCLANG_RPL_MUTATION_OK);
    assert(get_word(DSKTOP, 5u) == old_top + 10u);
    assert(get_word(AVMEM, 5u) == avmem + 2u);

    uint32_t before = get_word(DSKTOP, 5u);
    assert(hpclang_rpl_mutation_drop(&io, 1u) == HPCLANG_RPL_MUTATION_LEVEL);
    assert(get_word(DSKTOP, 5u) == before);

    hpclang_object_release(object);
}

static void test_native_arena_pins_prefix_without_overwriting_payload(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    io.temp_limit = 0x8f000u;
    const uint32_t first = 0x84000u - 13u;
    put_word(0x806e9u, 5u, first - 5u);
    put_word(first, 1u, 0u);
    put_word(first + 1u, 5u, 0x029bfu);
    put_word(first + 6u, 2u, 'A');
    put_word(first + 8u, 5u, 13u);
    memory[0x87000u] = 11u;
    uint32_t roots[] = {0x82000u, 0x82005u, 0x8200au};
    uint32_t count = 0u;
    uint32_t old_avmem = get_word(AVMEM, 5u);
    assert(hpclang_rpl_mutation_pin_arena(&io, 0x86000u, 0x88000u, roots, 3u, &count) == 0);
    assert(count == 5u);
    assert(get_word(TEMPTOP, 5u) == 0x88000u);
    assert(get_word(RSKTOP, 5u) == 0x89000u);
    assert(get_word(DSKTOP, 5u) == 0x90000u - 25u);
    assert(get_word(0x90000u, 5u) == 0x82000u); /* incoming stack untouched */
    for (uint32_t i = 0u; i < 3u; ++i)
        assert(get_word(0x90000u - (i + 1u) * 5u, 5u) == roots[i]);
    assert(get_word(0x90000u - 20u, 5u) == 0x84001u);
    assert(get_word(0x90000u - 25u, 5u) == first + 1u);
    assert(get_word(0x84001u, 5u) == 0x02a2cu);
    assert(get_word(0x87ffbu, 5u) == 0x4000u);
    assert(memory[0x87000u] == 11u);
    uint32_t cost = (0x90000u - 0x85000u) / 5u - (0x90000u - 0x89000u) / 5u;
    assert(get_word(AVMEM, 5u) == old_avmem - cost - count);

    io = setup(1u);
    put_word(0x806e9u, 5u, first - 5u);
    put_word(0x83ffbu, 5u, 0u); /* broken prefix chain */
    writes = 0u;
    assert(hpclang_rpl_mutation_pin_arena(&io, 0x86000u, 0x88000u, roots, 3u, &count) != 0);
    assert(writes == 0u);

    io = setup(1u);
    put_word(0x806e9u, 5u, 0x83ffbu); /* empty prefix, insufficient native space */
    writes = 0u;
    assert(hpclang_rpl_mutation_pin_arena(&io, 0x86000u, 0x8f000u, roots, 3u, &count) ==
           HPCLANG_RPL_MUTATION_NOSPACE);
    assert(writes == 0u);
}

static void test_stack_reorder(void)
{
    struct hpclang_rpl_mutation_io io = setup(3u);
    uint32_t top = get_word(DSKTOP, 5u), avmem = get_word(AVMEM, 5u) - 7u;
    put_word(AVMEM, 5u, avmem); /* Preserve an existing native reservation. */
    assert(hpclang_rpl_mutation_reorder(&io, 0) == HPCLANG_RPL_MUTATION_OK);
    assert(get_word(top, 5u) == 0x82020u);
    assert(get_word(top + 5u, 5u) == 0x82000u);
    assert(get_word(top + 10u, 5u) == 0x82040u);
    assert(get_word(AVMEM, 5u) == avmem);
    assert(get_word(DSKTOP, 5u) == top);
    assert(get_word(TEMPTOP, 5u) == 0x84000u);

    assert(hpclang_rpl_mutation_reorder(&io, 1) == HPCLANG_RPL_MUTATION_OK);
    assert(get_word(DSKTOP, 5u) == top + 5u);
    assert(get_word(top + 5u, 5u) == 0x82020u);
    assert(get_word(top + 10u, 5u) == 0x82040u);
    assert(get_word(AVMEM, 5u) == avmem + 1u);
    assert(get_word(TEMPTOP, 5u) == 0x84000u);

    for (int nip = 0; nip <= 1; ++nip) {
        io = setup(1u);
        assert(hpclang_rpl_mutation_reorder(&io, nip) == HPCLANG_RPL_MUTATION_LEVEL);
        assert(writes == 0u);
        io = setup(2u);
        rejected_read = get_word(DSKTOP, 5u) + 9u;
        assert(hpclang_rpl_mutation_reorder(&io, nip) == HPCLANG_RPL_MUTATION_IO);
        assert(writes == 0u);
        rejected_read = UINT32_MAX;
        put_word(AVMEM, 5u, 0xfffffu);
        assert(hpclang_rpl_mutation_reorder(&io, nip) == HPCLANG_RPL_MUTATION_MALFORMED);
        assert(writes == 0u);
        io = setup(2u);
        put_word(get_word(DSKTOP, 5u) + 5u, 5u, 0u);
        assert(hpclang_rpl_mutation_reorder(&io, nip) == HPCLANG_RPL_MUTATION_MALFORMED);
        assert(writes == 0u);
    }
}

static void test_backup_overwrite_preserves_old_value_and_crc(void)
{
    struct hpclang_rpl_mutation_io io = setup(1);
    uint32_t home_value = prepare_named_char('H');
    put_word(0x80716, 5, home_value + 7);
    const struct hpclang_rpl_reader reader = {read_nibble, NULL};
    put_word(0x82000, 5, 0x029bf);
    put_word(0x82005, 2, 'A');
    uint8_t *packed = NULL;
    uint32_t span;
    assert(hpclang_rpl_backup_encode(&reader, 0x82000, 7, (const uint8_t *)"DATA", 4, &packed,
                                     &span) == 0);
    assert(hpclang_rpl_port0_create_backup(&io, packed, span) == HPCLANG_RPL_MUTATION_OK);
    free(packed);
    uint32_t port = get_word(0x80716, 5);
    struct hpclang_rpl_backup_view old;
    assert(hpclang_rpl_backup_view(&reader, port, NIBBLES - port, &old) == 0);
    uint32_t root = old.value_address;
    uint32_t *roots[] = {&root};
    io.native_roots = roots;
    io.native_root_count = 1;
    put_word(get_word(DSKTOP, 5), 5, root);
    put_word(0x82005, 2, 'B');
    assert(hpclang_rpl_backup_encode(&reader, 0x82000, 7, (const uint8_t *)"DATA", 4, &packed,
                                     &span) == 0);
    uint32_t home = get_word(USEROB, 5);
    assert(hpclang_rpl_port0_replace_backup(&io, packed, span) == HPCLANG_RPL_MUTATION_OK);
    struct hpclang_rpl_backup_view updated;
    assert(hpclang_rpl_backup_view(&reader, port, NIBBLES - port, &updated) == 0);
    assert(updated.value_address == old.value_address && updated.checksum != old.checksum);
    assert(get_word(USEROB, 5) == home && get_word(0x80716, 5) == port);
    assert_char_at(updated.value_address, 'B');
    assert(root != old.value_address && root < io.temp_limit);
    assert_char_at(root, 'A');
    assert(get_word(get_word(DSKTOP, 5), 5) == root);
    uint32_t before = writes;
    packed[(span - 1) / 2] ^= 1u << (4 * ((span - 1) & 1));
    assert(hpclang_rpl_port0_replace_backup(&io, packed, span) == HPCLANG_RPL_MUTATION_MALFORMED);
    assert(writes == before);
    free(packed);
    assert(hpclang_rpl_backup_encode(&reader, 0x82000, 7, (const uint8_t *)"MISS", 4, &packed,
                                     &span) == 0);
    assert(hpclang_rpl_port0_replace_backup(&io, packed, span) == HPCLANG_RPL_MUTATION_NOT_FOUND);
    assert(writes == before);
    free(packed);
    put_word(0x82000, 5, 0x02a2c);
    put_word(0x82005, 5, 5);
    assert(hpclang_rpl_backup_encode(&reader, 0x82000, 10, (const uint8_t *)"DATA", 4, &packed,
                                     &span) == 0);
    uint32_t saved_limit = io.user_limit;
    io.user_limit = get_word(DSKTOP, 5);
    assert(hpclang_rpl_port0_replace_backup(&io, packed, span) == HPCLANG_RPL_MUTATION_NOSPACE);
    assert(writes == before && get_word(0x80716, 5) == port);
    io.user_limit = saved_limit;
    assert(hpclang_rpl_port0_replace_backup(&io, packed, span) == HPCLANG_RPL_MUTATION_OK);
    assert(get_word(0x80716, 5) == port - 3 && get_word(USEROB, 5) == home - 3);
    assert(hpclang_rpl_backup_view(&reader, port - 3, NIBBLES - port + 3, &updated) == 0);
    assert(updated.value_span == 10 && get_word(updated.value_address, 5) == 0x02a2c);
    assert_char_at(root, 'A');
    before = writes;
    assert(hpclang_rpl_port0_replace_backup(&io, packed, span) == 0 && writes == before);
    free(packed);
    /* Shrinking preserves both a root to the complete backup and a stack
     * reference to its enclosed string, including the backup CRC. */
    uint32_t backup_root = port - 3;
    uint32_t value_root = updated.value_address;
    uint32_t *shrink_roots[] = {&root, &backup_root, &value_root};
    io.native_roots = shrink_roots;
    io.native_root_count = 3;
    put_word(get_word(DSKTOP, 5), 5, value_root);
    put_word(0x82000, 5, 0x029bf);
    put_word(0x82005, 2, 'C');
    assert(hpclang_rpl_backup_encode(&reader, 0x82000, 7, (const uint8_t *)"DATA", 4, &packed,
                                     &span) == 0);
    assert(hpclang_rpl_port0_replace_backup(&io, packed, span) == HPCLANG_RPL_MUTATION_OK);
    assert(get_word(0x80716, 5) == port && get_word(USEROB, 5) == home);
    assert(hpclang_rpl_backup_view(&reader, port, NIBBLES - port, &updated) == 0);
    assert_char_at(updated.value_address, 'C');
    assert(backup_root < io.temp_limit && value_root < io.temp_limit);
    struct hpclang_rpl_backup_view preserved;
    assert(hpclang_rpl_backup_view(&reader, backup_root, NIBBLES - backup_root, &preserved) == 0);
    assert(preserved.value_address == value_root && get_word(value_root, 5) == 0x02a2c);
    assert(get_word(get_word(DSKTOP, 5), 5) == value_root);
    free(packed);
}

static void test_port_remove_preserves_backup_references(void)
{
    for (int variant = 0; variant < 6; ++variant) {
        int failure = variant % 3;
        struct hpclang_rpl_mutation_io io = setup(1);
        uint32_t home_value = prepare_named_char('H');
        uint32_t original_end = home_value + 7;
        put_word(0x80716, 5, original_end);
        const struct hpclang_rpl_reader reader = {read_nibble, NULL};
        put_word(0x82000, 5, 0x029bf);
        put_word(0x82005, 2, 'A');
        uint8_t *packed;
        uint32_t span;
        assert(hpclang_rpl_backup_encode(&reader, 0x82000, 7, (const uint8_t *)"DATA", 4, &packed,
                                         &span) == 0);
        assert(hpclang_rpl_port0_create_backup(&io, packed, span) == 0);
        free(packed);
        uint32_t old_port = get_word(0x80716, 5);
        struct hpclang_rpl_backup_view old;
        assert(hpclang_rpl_backup_view(&reader, old_port, NIBBLES - old_port, &old) == 0);
        uint32_t backup_root = old_port, value_root = old.value_address;
        uint32_t prefix_span = 0, prefix_root = 0;
        if (variant >= 3) {
            assert(hpclang_rpl_backup_encode(&reader, 0x82000, 7, (const uint8_t *)"PREFIX", 6,
                                             &packed, &prefix_span) == 0);
            assert(hpclang_rpl_port0_create_backup(&io, packed, prefix_span) == 0);
            free(packed);
            prefix_root = get_word(0x80716, 5);
        }
        uint32_t *roots[] = {&backup_root, &value_root, &prefix_root};
        io.native_roots = roots;
        io.native_root_count = 3;
        put_word(get_word(DSKTOP, 5), 5, value_root);
        uint32_t saved_writes = writes;
        if (failure == 1)
            io.temp_limit = get_word(RSKTOP, 5);
        if (failure == 2)
            memory[original_end - 1] ^= 1;
        int result = hpclang_rpl_port0_remove_backup(&io, (const uint8_t *)"DATA", 4);
        if (failure) {
            assert(result != 0 && writes == saved_writes);
            assert(backup_root == old_port && value_root == old.value_address);
            assert(get_word(0x80716, 5) == old_port - prefix_span);
            continue;
        }
        assert(result == 0 && get_word(0x80716, 5) == original_end - prefix_span);
        assert(backup_root != old_port && backup_root < io.temp_limit);
        struct hpclang_rpl_backup_view preserved;
        assert(hpclang_rpl_backup_view(&reader, backup_root, NIBBLES - backup_root, &preserved) ==
               0);
        assert(preserved.value_address == value_root);
        assert_char_at(value_root, 'A');
        assert(get_word(get_word(DSKTOP, 5), 5) == value_root);
        assert_char_at(home_value - prefix_span, 'H');
        if (prefix_span) {
            assert(prefix_root == original_end - prefix_span);
            struct hpclang_rpl_backup_view prefix;
            assert(hpclang_rpl_backup_view(&reader, prefix_root, NIBBLES - prefix_root, &prefix) ==
                   0);
            assert_char_at(prefix.value_address, 'A');
        }
        saved_writes = writes;
        assert(hpclang_rpl_port0_remove_backup(&io, (const uint8_t *)"DATA", 4) ==
               HPCLANG_RPL_MUTATION_NOT_FOUND);
        assert(writes == saved_writes);
    }
}

static void test_library_table_reference_preservation(void)
{
    for (unsigned failure = 0; failure < 3; ++failure) {
        struct hpclang_rpl_mutation_io io = setup(1);
        uint32_t object = prepare_named_char('A');
        io.library_table = 0x8611du;
        put_word(io.library_table, 3, 2);
        const uint32_t record = io.library_table + 3;
        put_word(record, 3, 123);
        put_word(record + 3, 5, object);
        put_word(record + 8, 5, 0);
        put_word(record + 16, 3, 124);
        put_word(record + 19, 5, object);
        put_word(record + 24, 5, 0x22f18);
        if (failure == 1)
            rejected_read = record + 24;
        if (failure == 2)
            io.library_table = NIBBLES - 2;
        struct hpclang_object *replacement = make_char('B');
        int result = hpclang_rpl_mutation_overwrite_value(&io, object, replacement);
        free(replacement);
        if (failure) {
            assert(result != 0 && writes == 0);
            assert_char_at(object, 'A');
            assert(get_word(record + 3, 5) == object);
            continue;
        }
        assert(result == 0);
        uint32_t retained = get_word(record + 3, 5);
        assert(retained != object && retained < io.temp_limit);
        assert_char_at(retained, 'A');
        assert_char_at(object, 'B');
        assert(get_word(record + 19, 5) == object);
        assert(get_word(record, 3) == 123 && get_word(record + 16, 3) == 124);
    }
}

static void test_overwrite_port_value_preserves_references(void)
{
    struct hpclang_rpl_mutation_io io = setup(1);
    prepare_named_char('H');
    const uint32_t address = 0xf0000;
    put_word(address, 5, 0x029bf);
    put_word(address + 5, 2, 'A');
    uint32_t root = address;
    uint32_t *roots[] = {&root};
    io.native_roots = roots;
    io.native_root_count = 1;
    put_word(get_word(DSKTOP, 5), 5, address);
    struct hpclang_object *value = make_char('B');
    assert(hpclang_rpl_mutation_overwrite_value(&io, address, value) == HPCLANG_RPL_MUTATION_OK);
    assert_char_at(address, 'B');
    assert(root != address && root < io.temp_limit);
    assert(get_word(get_word(DSKTOP, 5), 5) == root);
    assert_char_at(root, 'A');
    uint32_t before = writes;
    assert(hpclang_rpl_mutation_overwrite_value(&io, address, value) == HPCLANG_RPL_MUTATION_OK);
    assert(writes == before);
    assert(hpclang_rpl_mutation_overwrite_value(&io, NIBBLES - 2, value) ==
           HPCLANG_RPL_MUTATION_MALFORMED);
    assert(writes == before);
    free(value);
}

static void test_port1_append_respects_native_pool(void)
{
    const char *backup = "26b20130008005f42545a55425f48041620900003210119200f9e3";
    const char *library = "04b2062000108510b7000000000000000000000d5c2";
    uint8_t packed[27] = {0};
    for (uint32_t i = 0; i < 54; ++i) {
        uint8_t n = backup[i] <= '9' ? backup[i] - '0' : backup[i] - 'a' + 10;
        packed[i / 2] |= n << (4 * (i & 1));
    }
    for (int failure = 0; failure < 5; ++failure) {
        struct hpclang_rpl_mutation_io io = setup(1);
        for (uint32_t i = 0; i < 43; ++i)
            memory[i] = library[i] <= '9' ? library[i] - '0' : library[i] - 'a' + 10;
        uint8_t saved[43];
        memcpy(saved, memory, sizeof(saved));
        uint32_t limit = 103;
        memory[limit] = 11; /* First nibble of ARM-owned storage. */
        if (failure == 1)
            limit = 102;
        else if (failure == 2)
            limit = 0;
        else if (failure == 3)
            memory[42] ^= 1;
        else if (failure == 4)
            rejected_read = 100;
        int result = hpclang_rpl_port1_create_backup(&io, limit, packed, 54);
        if (failure) {
            assert(result != HPCLANG_RPL_MUTATION_OK && writes == 0);
            assert(memory[103] == 11);
            continue;
        }
        assert(result == HPCLANG_RPL_MUTATION_OK);
        assert(!memcmp(saved, memory, sizeof(saved)) && memory[103] == 11);
        const struct hpclang_rpl_reader reader = {read_nibble, NULL};
        struct hpclang_rpl_port_cursor cursor = {0, 0x40000u, 0};
        struct hpclang_rpl_port_entry entry;
        assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1 && entry.library_number == 123);
        assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1 && entry.address == 43);
        assert(entry.name_length == 8 && !memcmp(entry.name, "PORTZERO", 8));
        assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 0);
        uint32_t before = writes;
        assert(hpclang_rpl_port1_create_backup(&io, limit, packed, 54) ==
               HPCLANG_RPL_MUTATION_EXISTS);
        assert(writes == before);
    }
}

static void test_port1_library_append(void)
{
    const char *backup = "26b20130008005f42545a55425f48041620900003210119200f9e3";
    const char *library = "04b2062000108510b7000000000000000000000d5c2";
    for (unsigned failure = 0; failure < 6; ++failure) {
        struct hpclang_rpl_mutation_io io = setup(1);
        uint8_t packed[22] = {0};
        for (uint32_t i = 0; i < 43; ++i) {
            uint8_t n = library[i] <= '9' ? library[i] - '0' : library[i] - 'a' + 10;
            packed[i / 2] |= n << (4 * (i & 1));
        }
        for (uint32_t i = 0; i < 54; ++i)
            memory[i] = backup[i] <= '9' ? backup[i] - '0' : backup[i] - 'a' + 10;
        uint8_t saved[54];
        memcpy(saved, memory, sizeof saved);
        uint32_t limit = 103;
        memory[limit] = 11;
        if (failure == 1)
            --limit;
        else if (failure == 2)
            packed[21] ^= 1;
        else if (failure == 3)
            memory[53] ^= 1;
        else if (failure == 4)
            rejected_read = 102;
        else if (failure == 5)
            packed[0] = 0x62;
        int result = hpclang_rpl_port1_create_library(&io, limit, packed, 43);
        if (failure) {
            assert(result != HPCLANG_RPL_MUTATION_OK && writes == 0);
            assert(memory[103] == 11);
            continue;
        }
        assert(result == HPCLANG_RPL_MUTATION_OK && memory[103] == 11);
        assert(!memcmp(saved, memory, sizeof saved));
        const struct hpclang_rpl_reader reader = {read_nibble, NULL};
        struct hpclang_rpl_port_cursor cursor = {0, 0x40000u, 0};
        struct hpclang_rpl_port_entry entry;
        assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1 && entry.address == 0);
        assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1 && entry.address == 54);
        assert(entry.library_number == 123 && entry.name_length == 1 && entry.name[0] == 'X');
        assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 0);
        uint32_t before = writes;
        assert(hpclang_rpl_port1_create_library(&io, limit, packed, 43) ==
               HPCLANG_RPL_MUTATION_EXISTS);
        assert(writes == before);
    }
}

static void test_port1_append_publication_failures(void)
{
    const char *encodings[] = {"26b20130008005f42545a55425f48041620900003210119200f9e3",
                               "04b2062000108510b7000000000000000000000d5c2"};
    for (unsigned library = 0; library < 2; ++library) {
        uint32_t span = (uint32_t)strlen(encodings[library]);
        uint32_t prefix = (uint32_t)strlen(encodings[!library]);
        uint8_t packed[27] = {0};
        for (uint32_t i = 0; i < span; ++i) {
            char ch = encodings[library][i];
            packed[i / 2] |= (uint8_t)(ch <= '9' ? ch - '0' : ch - 'a' + 10) << (4 * (i & 1));
        }
        for (uint32_t fail = 0; fail <= span + 6; ++fail) {
            struct hpclang_rpl_mutation_io io = setup(1);
            for (uint32_t i = 0; i < prefix; ++i) {
                char ch = encodings[!library][i];
                memory[i] = (uint8_t)(ch <= '9' ? ch - '0' : ch - 'a' + 10);
            }
            uint8_t saved[54];
            memcpy(saved, memory, prefix);
            uint32_t limit = prefix + span + 6;
            memory[limit] = 11;
            rejected_write = fail;
            int result = library ? hpclang_rpl_port1_create_library(&io, limit, packed, span)
                                 : hpclang_rpl_port1_create_backup(&io, limit, packed, span);
            assert(writes == fail && memory[limit] == 11);
            assert(!memcmp(saved, memory, prefix));
            if (fail < span + 1) {
                assert(result == HPCLANG_RPL_MUTATION_IO);
                assert(get_word(prefix, 5) == 0); /* Old chain still terminates here. */
            } else if (fail < span + 6) {
                assert(result == HPCLANG_RPL_MUTATION_INCOMPLETE);
            } else {
                assert(result == HPCLANG_RPL_MUTATION_OK);
                for (uint32_t i = 0; i < span; ++i)
                    assert(memory[prefix + i] == ((packed[i / 2] >> (4 * (i & 1))) & 15));
                for (uint32_t i = 0; i < 6; ++i)
                    assert(memory[prefix + span + i] == 0);
            }
        }
    }
}

struct port1_guard_fixture
{
    uint32_t start, end, calls;
    int result;
};

static int check_port1_fixture(void *context, uint32_t start, uint32_t end)
{
    struct port1_guard_fixture *fixture = context;
    fixture->start = start;
    fixture->end = end;
    ++fixture->calls;
    return fixture->result;
}

static void test_port1_edit_owned_suffix(void)
{
    struct port1_guard_fixture fixture = {0};
    const struct hpclang_rpl_port1_guard guard = {check_port1_fixture, &fixture};
    struct hpclang_rpl_mutation_io io = setup(1);
    const struct hpclang_rpl_reader reader = {read_nibble, NULL};
    put_word(0x82000, 5, 0x029bf);
    put_word(0x82005, 2, 'A');
    uint8_t *small = NULL, *same = NULL, *tail = NULL, *large = NULL;
    uint32_t small_span, same_span, tail_span, large_span;
    assert(hpclang_rpl_backup_encode(&reader, 0x82000, 7, (const uint8_t *)"DATA", 4, &small,
                                     &small_span) == 0);
    put_word(0x82005, 2, 'B');
    assert(hpclang_rpl_backup_encode(&reader, 0x82000, 7, (const uint8_t *)"DATA", 4, &same,
                                     &same_span) == 0);
    assert(same_span == small_span);
    put_word(0x82005, 2, 'T');
    assert(hpclang_rpl_backup_encode(&reader, 0x82000, 7, (const uint8_t *)"TAIL", 4, &tail,
                                     &tail_span) == 0);
    put_word(0x82000, 5, 0x02a2c);
    put_word(0x82005, 5, 5);
    assert(hpclang_rpl_backup_encode(&reader, 0x82000, 10, (const uint8_t *)"DATA", 4, &large,
                                     &large_span) == 0);
    uint32_t limit = large_span + tail_span + 6;
    assert(hpclang_rpl_port1_create_backup(&io, limit, small, small_span) == 0);
    assert(hpclang_rpl_port1_create_backup(&io, limit, tail, tail_span) == 0);
    memory[limit] = 11;
    uint32_t before = writes;
    assert(hpclang_rpl_port1_replace_backup(&io, limit, large, large_span, NULL) ==
           HPCLANG_RPL_MUTATION_UNAVAILABLE);
    const struct hpclang_rpl_port1_guard missing = {0};
    assert(hpclang_rpl_port1_remove_backup(&io, limit, (const uint8_t *)"DATA", 4, &missing) ==
           HPCLANG_RPL_MUTATION_UNAVAILABLE);
    fixture.result = HPCLANG_RPL_MUTATION_BUSY;
    assert(hpclang_rpl_port1_replace_backup(&io, limit, large, large_span, &guard) ==
           HPCLANG_RPL_MUTATION_BUSY);
    assert(fixture.start == 0 && fixture.end == small_span + tail_span && fixture.calls == 1);
    assert(hpclang_rpl_port1_remove_backup(&io, limit, (const uint8_t *)"DATA", 4, &guard) ==
           HPCLANG_RPL_MUTATION_BUSY);
    fixture.result = HPCLANG_RPL_MUTATION_IO;
    assert(hpclang_rpl_port1_replace_backup(&io, limit, same, same_span, &guard) ==
           HPCLANG_RPL_MUTATION_IO);
    assert(fixture.start == 0 && fixture.end == small_span && fixture.calls == 3);
    assert(writes == before);
    fixture.result = HPCLANG_RPL_MUTATION_BUSY;
    assert(hpclang_rpl_port1_replace_backup(&io, limit, tail, tail_span, &guard) ==
           HPCLANG_RPL_MUTATION_BUSY);
    assert(fixture.start == small_span && fixture.end == small_span + tail_span);
    assert(writes == before);
    fixture.result = HPCLANG_RPL_MUTATION_OK;
    assert(hpclang_rpl_port1_replace_backup(&io, limit - 1, large, large_span, &guard) ==
           HPCLANG_RPL_MUTATION_NOSPACE);
    assert(writes == before);
    rejected_read = limit - 1;
    assert(hpclang_rpl_port1_replace_backup(&io, limit, large, large_span, &guard) ==
           HPCLANG_RPL_MUTATION_IO);
    assert(writes == before);
    rejected_read = UINT32_MAX;
    assert(hpclang_rpl_port1_replace_backup(&io, limit, large, large_span, &guard) == 0);
    assert(memory[limit] == 11);
    struct hpclang_rpl_port_cursor cursor = {0, 0x40000u, 0};
    struct hpclang_rpl_port_entry entry;
    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1 && entry.value_span == 10);
    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1 && entry.address == large_span);
    assert(!memcmp(entry.name, "TAIL", 4));
    assert_char_at(entry.value_address, 'T');
    assert(hpclang_rpl_port1_replace_backup(&io, limit, small, small_span, &guard) == 0);
    cursor = (struct hpclang_rpl_port_cursor){0, 0x40000u, 0};
    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1);
    assert_char_at(entry.value_address, 'A');
    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1 && entry.address == small_span);
    assert_char_at(entry.value_address, 'T');
    before = writes;
    small[0] ^= 1;
    assert(hpclang_rpl_port1_replace_backup(&io, limit, small, small_span, &guard) ==
           HPCLANG_RPL_MUTATION_MALFORMED);
    small[0] ^= 1;
    assert(writes == before);
    assert(hpclang_rpl_port1_remove_backup(&io, limit, (const uint8_t *)"MISS", 4, &guard) ==
           HPCLANG_RPL_MUTATION_NOT_FOUND);
    assert(writes == before);
    assert(hpclang_rpl_port1_remove_backup(&io, limit, (const uint8_t *)"DATA", 4, &guard) == 0);
    cursor = (struct hpclang_rpl_port_cursor){0, 0x40000u, 0};
    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1 && entry.address == 0);
    assert_char_at(entry.value_address, 'T');
    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 0);
    assert(hpclang_rpl_port1_remove_backup(&io, limit, (const uint8_t *)"TAIL", 4, &guard) == 0);
    assert(get_word(0, 5) == 0 && memory[limit] == 11);

    // A following library must not move; same-size replacement may leave it fixed.
    io = setup(1);
    assert(hpclang_rpl_port1_create_backup(&io, 256, small, small_span) == 0);
    const char *library = "04b2062000108510b7000000000000000000000d5c2";
    for (uint32_t i = 0; i < 43; ++i)
        memory[small_span + i] = library[i] <= '9' ? library[i] - '0' : library[i] - 'a' + 10;
    before = writes;
    assert(hpclang_rpl_port1_remove_backup(&io, 256, (const uint8_t *)"DATA", 4, &guard) ==
           HPCLANG_RPL_MUTATION_UNSUPPORTED);
    assert(hpclang_rpl_port1_replace_backup(&io, 256, large, large_span, &guard) ==
           HPCLANG_RPL_MUTATION_UNSUPPORTED);
    assert(writes == before);
    assert(hpclang_rpl_port1_replace_backup(&io, 256, same, same_span, &guard) == 0);
    assert(fixture.start == 0 && fixture.end == small_span);
    cursor = (struct hpclang_rpl_port_cursor){0, 0x40000u, 0};
    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1);
    assert_char_at(entry.value_address, 'B');
    assert(get_word(small_span, 5) == 0x02b40);
    /* Growth, shrinkage, same-size replacement and deletion must distinguish
     * interrupted commits from preflight I/O errors at every write boundary. */
    for (unsigned mode = 0; mode < 4; ++mode) {
        const uint8_t *old = mode == 1 ? large : small;
        uint32_t old_span = mode == 1 ? large_span : small_span;
        const uint8_t *next = mode == 0 ? large : mode == 1 ? small : same;
        uint32_t next_span = mode == 0 ? large_span : mode == 3 ? 0 : small_span;
        uint32_t expected_writes = next_span + (mode == 2 ? 0 : tail_span + 6);
        for (uint32_t fault = 0; fault <= expected_writes; ++fault) {
            io = setup(1);
            fixture.result = 0;
            assert(!hpclang_rpl_port1_create_backup(&io, 256, old, old_span));
            assert(!hpclang_rpl_port1_create_backup(&io, 256, tail, tail_span));
            memory[256] = 11;
            writes = 0;
            rejected_write = fault;
            int result =
                mode == 3
                    ? hpclang_rpl_port1_remove_backup(&io, 256, (const uint8_t *)"DATA", 4, &guard)
                    : hpclang_rpl_port1_replace_backup(&io, 256, next, next_span, &guard);
            assert(result == (fault == expected_writes ? HPCLANG_RPL_MUTATION_OK
                                                       : HPCLANG_RPL_MUTATION_INCOMPLETE));
            assert(writes == fault && memory[256] == 11);
            if (fault == expected_writes) {
                cursor = (struct hpclang_rpl_port_cursor){0, 0x40000u, 0};
                if (mode != 3)
                    assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1 &&
                           entry.span == next_span);
                assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1);
                assert_char_at(entry.value_address, 'T');
                assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 0);
            }
        }
        if (mode == 2)
            continue;
        io = setup(1);
        assert(!hpclang_rpl_port1_create_backup(&io, 256, old, old_span));
        assert(!hpclang_rpl_port1_create_backup(&io, 256, tail, tail_span));
        writes = 0;
        rejected_commit_read = old_span + (mode == 0 ? tail_span + 4 : 1);
        int result =
            mode == 3
                ? hpclang_rpl_port1_remove_backup(&io, 256, (const uint8_t *)"DATA", 4, &guard)
                : hpclang_rpl_port1_replace_backup(&io, 256, next, next_span, &guard);
        assert(result == HPCLANG_RPL_MUTATION_INCOMPLETE && writes == 1);
    }
    free(small);
    free(same);
    free(tail);
    free(large);
}

static void test_port0_create_library(void)
{
    const char *library = "04b2062000108510b7000000000000000000000d5c2";
    for (int failure_case = 0; failure_case < 5; ++failure_case) {
        struct hpclang_rpl_mutation_io io = setup(1);
        uint32_t object = prepare_named_char('A');
        uint32_t port = object + 7;
        put_word(0x80716, 5, port);
        uint8_t packed[22] = {0};
        for (uint32_t i = 0; i < 43; ++i) {
            uint8_t nibble = library[i] <= '9' ? library[i] - '0' : library[i] - 'a' + 10;
            packed[i / 2] |= nibble << (4 * (i & 1));
        }
        uint32_t root = object;
        uint32_t *roots[] = {&root};
        io.native_roots = roots;
        io.native_root_count = 1;
        uint32_t dstk = get_word(DSKTOP, 5);
        put_word(dstk, 5, object);
        if (failure_case == 1)
            packed[21] ^= 1; /* Invalid CRC. */
        else if (failure_case == 2)
            io.user_limit = dstk;
        else if (failure_case == 3)
            rejected_read = port - 1;
        else if (failure_case == 4)
            packed[0] = 0x62; /* Backup prologue cannot enter the library path. */
        int result = hpclang_rpl_port0_create_library(&io, packed, 43);
        if (failure_case) {
            assert(result != HPCLANG_RPL_MUTATION_OK && writes == 0);
            assert(root == object && get_word(DSKTOP, 5) == dstk);
            assert(get_word(0x80716, 5) == port);
            continue;
        }
        assert(result == HPCLANG_RPL_MUTATION_OK);
        assert(root == object - 43 && get_word(dstk - 43, 5) == root);
        assert_char_at(root, 'A');
        const struct hpclang_rpl_reader reader = {read_nibble, NULL};
        struct hpclang_rpl_port_cursor cursor = {port - 43, NIBBLES, 0};
        struct hpclang_rpl_port_entry entry;
        assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1);
        assert(entry.prologue == 0x02b40 && entry.library_number == 123);
        assert(entry.name_length == 1 && entry.name[0] == 'X');
        assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 0);
        uint32_t before = writes;
        assert(hpclang_rpl_port0_create_library(&io, packed, 43) == HPCLANG_RPL_MUTATION_EXISTS);
        assert(writes == before);
        /* A different title does not make the same library ID unique. */
        packed[6] = 'Y';
        uint32_t crc = 0;
        for (uint32_t i = 5; i < 39; ++i) {
            uint8_t nibble = (packed[i / 2] >> (4 * (i & 1))) & 15;
            crc = (crc >> 4) ^ (((crc ^ nibble) & 15) * 0x1081);
        }
        for (uint32_t i = 0; i < 4; ++i) {
            uint32_t shift = 4 * ((39 + i) & 1);
            packed[(39 + i) / 2] =
                (packed[(39 + i) / 2] & ~(15u << shift)) | (((crc >> (4 * i)) & 15) << shift);
        }
        assert(hpclang_rpl_port0_create_library(&io, packed, 43) == HPCLANG_RPL_MUTATION_EXISTS);
        assert(writes == before);
    }
}

static void test_port0_create_preserves_home_and_library(void)
{
    const char *backup = "26b20130008005f42545a55425f48041620900003210119200f9e3";
    const char *library = "04b2062000108510b7000000000000000000000d5c2";
    uint8_t packed[27] = {0};
    for (uint32_t i = 0; i < 54; ++i) {
        uint8_t n = backup[i] <= '9' ? backup[i] - '0' : backup[i] - 'a' + 10;
        packed[i / 2] |= n << (4 * (i & 1));
    }
    for (int failure_case = 0; failure_case < 4; ++failure_case) {
        struct hpclang_rpl_mutation_io io = setup(1);
        uint32_t object = prepare_named_char('A');
        uint32_t port = object + 7;
        put_word(0x80716, 5, port);
        for (uint32_t i = 0; i < 43; ++i)
            memory[port + i] = library[i] <= '9' ? library[i] - '0' : library[i] - 'a' + 10;
        uint8_t saved_library[48];
        memcpy(saved_library, memory + port, sizeof(saved_library));
        uint32_t root = object;
        uint32_t *roots[] = {&root};
        io.native_roots = roots;
        io.native_root_count = 1;
        uint32_t dstk = get_word(DSKTOP, 5);
        put_word(dstk, 5, object);
        if (failure_case == 1)
            io.user_limit = dstk;
        else if (failure_case == 2)
            memory[port + 42] ^= 1;
        else if (failure_case == 3)
            rejected_read = port - 1;
        int result = hpclang_rpl_port0_create_backup(&io, packed, 54);
        if (failure_case) {
            assert(result != HPCLANG_RPL_MUTATION_OK && writes == 0);
            assert(get_word(DSKTOP, 5) == dstk && get_word(0x80716, 5) == port);
            continue;
        }
        assert(result == HPCLANG_RPL_MUTATION_OK);
        assert(get_word(0x80716, 5) == port - 54);
        assert(get_word(DSKTOP, 5) == dstk - 54);
        assert(get_word(dstk - 54, 5) == object - 54 && root == object - 54);
        assert_char_at(root, 'A');
        assert(memcmp(saved_library, memory + port, sizeof(saved_library)) == 0);
        const struct hpclang_rpl_reader reader = {read_nibble, NULL};
        struct hpclang_rpl_port_cursor cursor = {port - 54, NIBBLES, 0};
        struct hpclang_rpl_port_entry entry;
        assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1);
        assert(entry.prologue == 0x02b62 && entry.name_length == 8);
        assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 1);
        assert(entry.library_number == 123 && entry.address == port);
        assert(hpclang_rpl_port_next(&reader, &cursor, &entry) == 0);
        uint32_t saved_writes = writes;
        assert(hpclang_rpl_port0_create_backup(&io, packed, 54) == HPCLANG_RPL_MUTATION_EXISTS);
        assert(writes == saved_writes);

        /* HOME mutation keeps its old fence. Port contraction must update the
         * structural port boundary without treating it as a live value root. */
        assert(hpclang_rpl_mutation_userob_shift_before(&io, port, -54) ==
               HPCLANG_RPL_MUTATION_MALFORMED);
        assert(hpclang_rpl_port0_shift_backups(&io, port - 1, -53) ==
               HPCLANG_RPL_MUTATION_MALFORMED);
        assert(hpclang_rpl_port0_shift_backups(&io, port + 43, -97) ==
               HPCLANG_RPL_MUTATION_UNSUPPORTED);
        assert(writes == saved_writes);
        uint32_t backup_root = port - 54;
        uint32_t *live_roots[] = {&root, &backup_root};
        io.native_roots = live_roots;
        io.native_root_count = 2;
        assert(hpclang_rpl_port0_shift_backups(&io, port, -54) == HPCLANG_RPL_MUTATION_BUSY);
        assert(writes == saved_writes);
        io.native_roots = roots;
        io.native_root_count = 1;
        assert(hpclang_rpl_port0_shift_backups(&io, port, -54) == HPCLANG_RPL_MUTATION_OK);
        assert(get_word(0x80716, 5) == port && get_word(DSKTOP, 5) == dstk);
        assert(root == object && get_word(dstk, 5) == object);
        assert_char_at(root, 'A');
        assert(memcmp(saved_library, memory + port, sizeof(saved_library)) == 0);
    }
}

struct root_visit_state
{
    uint32_t count;
    unsigned found;
    int stop;
};

struct source_visit_state
{
    uint32_t slots[512], values[512], count, index;
    unsigned counts[11];
    int stop;
};

static int collect_flat_root(void *context, uint32_t slot, uint32_t value)
{
    struct source_visit_state *state = context;
    assert(state->count < 512);
    state->slots[state->count] = slot;
    state->values[state->count++] = value;
    return 0;
}

static int visit_source_root(void *context, enum hpclang_rpl_root_source source, uint32_t slot,
                             uint32_t value)
{
    struct source_visit_state *state = context;
    assert(state->index < state->count);
    assert(slot == state->slots[state->index] && value == state->values[state->index]);
    ++state->index;
    enum hpclang_rpl_root_source expected;
    if (!slot)
        expected = HPCLANG_RPL_ROOT_NATIVE;
    else if (slot >= 0x90000 && slot <= 0x90005)
        expected = HPCLANG_RPL_ROOT_DATA_STACK;
    else if (slot == 0x84ffb)
        expected = HPCLANG_RPL_ROOT_RETURN_STACK;
    else if (slot == 0x8440a || slot == 0x8440f)
        expected = HPCLANG_RPL_ROOT_TEMPENV;
    else if (slot == 0x84503 || slot == 0x84508)
        expected = HPCLANG_RPL_ROOT_TOUCHTAB;
    else if (slot == 0x84623 || slot == 0x8463c || slot == 0x84641)
        expected = HPCLANG_RPL_ROOT_VGER;
    else if (slot == 0x86123)
        expected = HPCLANG_RPL_ROOT_LIBRARY;
    else if (slot == 0x86138)
        expected = HPCLANG_RPL_ROOT_LIBRARY_SWITCH;
    else if (slot == 0x8068d || slot == 0x80695 || slot == 0x806d0 || slot == 0x806d5)
        expected = HPCLANG_RPL_ROOT_DISPLAY;
    else {
        assert(slot >= 0x806da && slot < 0x806da + 396 * 5 && (slot - 0x806da) % 5 == 0);
        expected = HPCLANG_RPL_ROOT_SYSTEM;
    }
    assert(source == expected);
    ++state->counts[source];
    return state->stop;
}

static void test_attachment_relocation(void)
{
    struct hpclang_rpl_mutation_io io = setup(1);
    uint32_t home = 0x92000;
    put_word(USEROB, 5, home);
    put_word(home, 5, DORRP);
    put_word(home + 5, 3, 2);
    put_word(home + 8, 3, 171);
    put_word(home + 11, 5, 0x84003);
    put_word(home + 16, 5, 0x83000);
    put_word(home + 21, 3, 222);
    put_word(home + 24, 5, 0);
    put_word(home + 29, 5, 0x84ffe);
    put_word(home + 34, 5, 0); /* Empty HOME after the attachment records. */
    struct hpclang_object *object = make_char('L');
    for (unsigned offset = 0; offset < 5; ++offset) {
        rejected_read = home + 29 + offset;
        assert(hpclang_rpl_mutation_push_snapshot(&io, object) == HPCLANG_RPL_MUTATION_IO);
        assert(!writes && get_word(home + 11, 5) == 0x84003);
    }
    rejected_read = UINT32_MAX;
    assert(!hpclang_rpl_mutation_push_snapshot(&io, object));
    uint32_t delta = object->nibble_count + 6;
    assert(get_word(home + 11, 5) == 0x84003 + delta);
    assert(get_word(home + 29, 5) == 0x84ffe + delta);
    assert(get_word(home + 16, 5) == 0x83000 && !get_word(home + 24, 5));
    assert(get_word(home + 8, 3) == 171 && get_word(home + 21, 3) == 222);
    hpclang_object_release(object);

    /* HOME and one referenced object move together. Rewrite through the old
     * HOME root before SYSUPD changes it, then carry those fields with HOME. */
    io = setup(1);
    struct named_pair pair = prepare_two_named_chars();
    memmove(memory + pair.chain + 13, memory + pair.chain, pair.object_a + 7 - pair.chain);
    put_word(pair.home + 5, 3, 1);
    put_word(pair.home + 8, 3, 171);
    put_word(pair.home + 11, 5, pair.object_b + 13);
    put_word(pair.home + 16, 5, pair.object_a + 13);
    assert(!hpclang_rpl_mutation_userob_shift_before(&io, pair.link_a + 13, 5));
    home = get_word(USEROB, 5);
    assert(home == pair.home - 5 && get_word(home + 8, 3) == 171);
    assert(get_word(home + 11, 5) == pair.object_b + 13 - 5);
    assert(get_word(home + 16, 5) == pair.object_a + 13);
    assert(!hpclang_rpl_mutation_userob_shift_before(&io, pair.link_a + 13, -5));
    assert(get_word(USEROB, 5) == pair.home);
    assert(get_word(pair.home + 11, 5) == pair.object_b + 13);
    assert(get_word(pair.home + 16, 5) == pair.object_a + 13);
}

static void test_display_root_relocation(void)
{
    struct hpclang_rpl_mutation_io io = setup(1);
    const uint32_t slots[] = {0x8068d, 0x80695, 0x806d0, 0x806d5};
    io.display_roots = slots;
    io.display_root_count = 4;
    const uint32_t pointers[] = {0x84003, 0x84fff, 0x83000, 0};
    for (unsigned i = 0; i < 4; ++i)
        put_word(slots[i], 5, pointers[i]);
    struct hpclang_object *object = make_char('D');
    rejected_read = slots[3];
    assert(hpclang_rpl_mutation_push_snapshot(&io, object) == HPCLANG_RPL_MUTATION_IO);
    assert(!writes && get_word(TEMPTOP, 5) == 0x84000);
    rejected_read = UINT32_MAX;
    assert(hpclang_rpl_mutation_push_snapshot(&io, object) == 0);
    uint32_t delta = object->nibble_count + 6;
    assert(get_word(slots[0], 5) == pointers[0] + delta);
    assert(get_word(slots[1], 5) == pointers[1] + delta);
    assert(get_word(slots[2], 5) == pointers[2]);
    assert(get_word(slots[3], 5) == 0);
    hpclang_object_release(object);
}

static uint8_t root_reads[NIBBLES], root_dependencies[NIBBLES];
struct dependency_visit_state
{
    struct source_visit_state roots;
    unsigned calls, stop_at;
};

static int track_root_read(void *context, uint32_t address, uint8_t *value)
{
    if (!read_nibble(context, address, value))
        return 0;
    root_reads[address] = 1;
    return 1;
}

static int visit_root_dependency(void *context, uint32_t address, uint8_t value)
{
    struct dependency_visit_state *state = context;
    assert(address < NIBBLES && value == (memory[address] & 15));
    root_dependencies[address] = 1;
    return ++state->calls == state->stop_at ? 83 : 0;
}

static void check_guarded_roots(const struct hpclang_rpl_mutation_io *io,
                                const struct source_visit_state *baseline)
{
    struct hpclang_rpl_mutation_io tracked = *io;
    tracked.read_nibble = track_root_read;
    struct dependency_visit_state state = {.roots = *baseline};
    state.roots.index = 0;
    memset(root_reads, 0, sizeof root_reads);
    memset(root_dependencies, 0, sizeof root_dependencies);
    assert(!hpclang_rpl_mutation_visit_guarded_roots(&tracked, visit_source_root,
                                                     visit_root_dependency, &state));
    assert(state.roots.index == baseline->count && state.calls > 0);
    assert(!memcmp(root_reads, root_dependencies, sizeof root_reads));
    /* Structural fields as well as the pointers themselves must be guarded. */
    const uint32_t metadata[] = {DSKTOP,     EDITLINE, RSKTOP,  TEMPENV, TOUCHTAB,
                                 FSTVGERPTR, 0x84400,  0x8611d, 0x86128};
    for (unsigned i = 0; i < sizeof metadata / sizeof metadata[0]; ++i)
        assert(root_dependencies[metadata[i]]);
    unsigned total = state.calls;
    const unsigned stops[] = {1, total / 2, total};
    for (unsigned i = 0; i < sizeof stops / sizeof stops[0]; ++i) {
        state = (struct dependency_visit_state){.roots = *baseline, .stop_at = stops[i]};
        state.roots.index = 0;
        assert(hpclang_rpl_mutation_visit_guarded_roots(&tracked, visit_source_root,
                                                        visit_root_dependency, &state) == 83);
        assert(state.calls == stops[i]);
    }
    state = (struct dependency_visit_state){.roots = *baseline};
    state.roots.index = 0;
    state.roots.stop = 73;
    assert(hpclang_rpl_mutation_visit_guarded_roots(&tracked, visit_source_root,
                                                    visit_root_dependency, &state) == 73);
    state.roots.index = 0;
    state.roots.stop = 0;
    rejected_read = 0x86128;
    assert(hpclang_rpl_mutation_visit_guarded_roots(&tracked, visit_source_root,
                                                    visit_root_dependency,
                                                    &state) == HPCLANG_RPL_MUTATION_IO);
    rejected_read = UINT32_MAX;
    assert(hpclang_rpl_mutation_visit_guarded_roots(&tracked, visit_source_root, NULL, &state) ==
           HPCLANG_RPL_MUTATION_UNAVAILABLE);
    assert(!writes);
}

static void test_root_sources(void)
{
    struct hpclang_rpl_mutation_io io = setup(1);
    struct named_pair pair = prepare_two_named_chars();
    prepare_reference_tables(&pair);
    uint32_t native = pair.object_b;
    uint32_t *roots[] = {&native, &native, &native};
    io.native_roots = roots;
    io.native_root_count = sizeof roots / sizeof roots[0];
    io.library_table = 0x8611d;
    const uint32_t display[] = {0x8068d, 0x80695, 0x806d0, 0x806d5};
    io.display_roots = display;
    io.display_root_count = 4;
    for (unsigned i = 0; i < 4; ++i)
        put_word(display[i], 5, 0x86800 + i * 20);
    put_word(0x8611d, 3, 2);
    put_word(0x86123, 5, 0x81234);
    put_word(0x86128, 5, 0);
    put_word(0x86133, 5, 0x41234);
    put_word(0x86138, 5, 0x22f5a);
    struct source_visit_state state = {0};
    uint8_t *before = malloc(sizeof memory);
    assert(before);
    memcpy(before, memory, sizeof memory);
    assert(hpclang_rpl_mutation_visit_roots(&io, collect_flat_root, &state) == 0);
    assert(hpclang_rpl_mutation_visit_root_sources(&io, visit_source_root, &state) == 0);
    assert(state.index == state.count);
    const unsigned counts[] = {1, 2, 1, 2, 2, 3, 396, 1, 4, 0, 1};
    assert(memcmp(counts, state.counts, sizeof counts) == 0);
    check_guarded_roots(&io, &state);
    state.index = 0;
    state.stop = 73;
    assert(hpclang_rpl_mutation_visit_root_sources(&io, visit_source_root, &state) == 73);
    assert(state.index == 1);
    state.index = 0;
    state.stop = 0;
    rejected_read = 0x86128;
    assert(hpclang_rpl_mutation_visit_root_sources(&io, visit_source_root, &state) ==
           HPCLANG_RPL_MUTATION_IO);
    assert(writes == 0 && native == pair.object_b && memcmp(before, memory, sizeof memory) == 0);
    assert(hpclang_rpl_mutation_visit_root_sources(&io, NULL, &state) ==
           HPCLANG_RPL_MUTATION_UNAVAILABLE);
    for (unsigned i = 0; i < 4; ++i) {
        state.index = 0;
        rejected_read = display[i];
        assert(hpclang_rpl_mutation_visit_root_sources(&io, visit_source_root, &state) ==
               HPCLANG_RPL_MUTATION_IO);
    }
    rejected_read = UINT32_MAX;
    uint32_t invalid[] = {0x8068d, 0x8068f};
    io.display_roots = invalid;
    io.display_root_count = 2;
    state.index = 0;
    assert(hpclang_rpl_mutation_visit_root_sources(&io, visit_source_root, &state) ==
           HPCLANG_RPL_MUTATION_MALFORMED);
    assert(state.index == 0 && !writes);
    invalid[1] = 0x806da;
    assert(hpclang_rpl_mutation_visit_root_sources(&io, visit_source_root, &state) ==
           HPCLANG_RPL_MUTATION_MALFORMED);
    free(before);
}

static int visit_root(void *context, uint32_t slot, uint32_t value)
{
    struct root_visit_state *state = context;
    ++state->count;
    if (slot == 0 && value == 0x82345)
        state->found |= 1;
    if (slot == 0x90000 && value == 0x82000)
        state->found |= 2;
    if (slot == 0x84ffb && value == 0x1d91b)
        state->found |= 4;
    return state->stop;
}

static void test_read_only_root_visit(void)
{
    struct hpclang_rpl_mutation_io io = setup(1);
    uint32_t native = 0x82345;
    uint32_t *roots[] = {&native};
    io.native_roots = roots;
    io.native_root_count = 1;
    put_word(0x84ffb, 5, 0x1d91b);
    put_word(0x84ff6, 5, 0);
    uint8_t *before = malloc(sizeof(memory));
    assert(before);
    memcpy(before, memory, sizeof(memory));
    struct root_visit_state state = {0};
    assert(hpclang_rpl_mutation_visit_roots(&io, visit_root, &state) == 0);
    assert(state.found == 7);
    assert(writes == 0 && native == 0x82345);
    assert(memcmp(before, memory, sizeof(memory)) == 0);
    state = (struct root_visit_state){.stop = 79};
    assert(hpclang_rpl_mutation_visit_roots(&io, visit_root, &state) == 79);
    assert(state.count == 1 && writes == 0);
    rejected_read = DSKTOP;
    state = (struct root_visit_state){0};
    assert(hpclang_rpl_mutation_visit_roots(&io, visit_root, &state) != 0);
    assert(writes == 0 && memcmp(before, memory, sizeof(memory)) == 0);
    assert(hpclang_rpl_mutation_visit_roots(&io, NULL, &state) == HPCLANG_RPL_MUTATION_UNAVAILABLE);
    free(before);
}

struct temp_visit_state
{
    uint32_t count;
    int stop;
};

static int visit_temp(void *context, uint32_t first, uint32_t nibbles)
{
    struct temp_visit_state *state = context;
    assert(state->count < 2);
    assert(first == (state->count ? 0x83006u : 0x83013u));
    assert(nibbles == (state->count ? 7u : 11u));
    ++state->count;
    return state->stop;
}

static void test_temp_allocation_visit(void)
{
    struct hpclang_rpl_mutation_io io = setup(0);
    put_word(TEMPTOP - 5, 5, 0x83000);
    put_word(0x83000, 5, 0);
    put_word(TEMPTOP, 5, 0x83023);
    put_word(0x8300d, 5, 13);
    put_word(0x8301e, 5, 17);
    struct temp_visit_state state = {0};
    assert(hpclang_rpl_mutation_visit_temp(&io, visit_temp, &state) == 0);
    assert(state.count == 2 && writes == 0);
    state = (struct temp_visit_state){.stop = 81};
    assert(hpclang_rpl_mutation_visit_temp(&io, visit_temp, &state) == 81);
    assert(state.count == 1 && writes == 0);
    const uint32_t bad[] = {0, 10, 14, 0xfffff};
    for (uint32_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        put_word(0x8300d, 5, bad[i]);
        state = (struct temp_visit_state){0};
        assert(hpclang_rpl_mutation_visit_temp(&io, visit_temp, &state) ==
               HPCLANG_RPL_MUTATION_MALFORMED);
        assert(state.count == 0 && writes == 0);
    }
    put_word(0x8300d, 5, 13);
    rejected_read = 0x8300d;
    assert(hpclang_rpl_mutation_visit_temp(&io, visit_temp, &state) != 0);
    assert(state.count == 0 && writes == 0);
    rejected_read = UINT32_MAX;
    put_word(TEMPTOP, 5, 0x83005);
    assert(hpclang_rpl_mutation_visit_temp(&io, visit_temp, &state) == 0);
    assert(state.count == 0);
    put_word(0x83000, 5, 1);
    assert(hpclang_rpl_mutation_visit_temp(&io, visit_temp, &state) ==
           HPCLANG_RPL_MUTATION_MALFORMED);
}

static struct hpclang_rpl_mutation_io setup_catalog_growth(void)
{
    struct hpclang_rpl_mutation_io io = setup(1);
    io.library_table = 0x82000;
    put_word(USEROB, 5, 0x92000);
    put_word(0x92000, 5, DORRP);
    put_word(io.library_table, 3, 2);
    put_word(0x82003, 3, 10);
    put_word(0x82006, 5, 0x83001);
    put_word(0x82013, 3, 20);
    put_word(0x82016, 5, 0x83002);
    put_word(0x806e9, 5, 0x83000);
    put_word(TEMPTOP, 5, 0x83005);
    put_word(RSKTOP, 5, 0x8300f);
    put_word(0x83005, 5, 0);
    put_word(0x8300a, 5, 0x83001);
    put_word(0x90000, 5, 0x83002);
    put_word(AVMEM, 5, (0x90000 - 0x8300f) / 5 - 7);
    memory[0x82555] = 11;
    return io;
}

struct object_guard_state
{
    struct hpclang_rpl_reference_guard_builder builder;
    unsigned edges;
};

static int catalog_object_edge(void *context, const struct hpclang_rpl_reference *edge)
{
    struct object_guard_state *state = context;
    assert(edge->kind == HPCLANG_RPL_REFERENCE_INLINE || edge->kind == HPCLANG_RPL_REFERENCE_ACPTR);
    ++state->edges;
    return 0;
}

static int catalog_object_dependency(void *context, uint32_t address, uint8_t value)
{
    struct object_guard_state *state = context;
    return hpclang_rpl_reference_guards_observe(&state->builder, 7, address, value);
}

static int alias_catalog_location(void *context, const struct hpclang_rpl_library_binding *binding,
                                  struct hpclang_rpl_catalog_locations *locations)
{
    (void)context;
    (void)binding;
    *locations = (struct hpclang_rpl_catalog_locations){{7, 9}, 0x12580, 0};
    return 0;
}

static struct hpclang_rpl_mutation_io setup_home_attachments(unsigned count)
{
    struct hpclang_rpl_mutation_io io = setup(1);
    uint32_t home = 0x90040, chain = home + 8 + 13 * count, item = chain + 10;
    put_word(USEROB, 5, home);
    put_word(CONTEXT, 5, home);
    put_word(home, 5, DORRP);
    put_word(home + 5, 3, count);
    for (unsigned i = 0; i < count; ++i) {
        put_word(home + 8 + 13 * i, 3, 10 * (i + 1));
        put_word(home + 11 + 13 * i, 5, home);
        put_word(home + 16 + 13 * i, 5, 0x93000);
    }
    put_word(chain, 5, 10);
    put_word(chain + 5, 5, 0);
    put_word(item, 2, 1);
    put_word(item + 2, 2, 'A');
    put_word(item + 4, 2, 1);
    put_word(item + 6, 5, 0x029bf);
    put_word(item + 11, 2, 'Z');
    put_word(0x80716, 5, item + 13);
    put_word(0x90000, 5, item + 6);
    return io;
}

static void test_home_detach(void)
{
    uint32_t successful_writes = 0;
    for (unsigned selected = 0; selected < 4; ++selected) {
        unsigned count = selected == 3 ? 1 : 3, index = selected % 3;
        struct hpclang_rpl_mutation_io io = setup_home_attachments(count);
        uint32_t object = 0x90040 + 24 + 13 * count;
        uint32_t old_free = get_word(AVMEM, 5);
        assert(hpclang_rpl_mutation_home_detach(&io, 10 * (index + 1)) == 0);
        assert(get_word(USEROB, 5) == 0x9004d && get_word(CONTEXT, 5) == 0x9004d);
        assert(get_word(0x90052, 3) == count - 1 && get_word(DSKTOP, 5) == 0x9000d);
        assert(get_word(0x9000d, 5) == object && get_word(AVMEM, 5) == old_free + 2);
        assert_char_at(object, 'Z');
        struct hpclang_rpl_reader reader = {read_nibble, NULL};
        struct hpclang_rpl_directory_entry_view entry;
        assert(hpclang_rpl_directory_find(&reader, 0x9004d, (const uint8_t *)"A", 1, &entry) == 0);
        assert(entry.object_address == object);
        unsigned output = 0;
        for (unsigned i = 0; i < count; ++i) {
            if (i == index)
                continue;
            uint32_t at = 0x90055 + 13 * output++;
            assert(get_word(at, 3) == 10 * (i + 1));
            assert(get_word(at + 3, 5) == 0x9004d && get_word(at + 8, 5) == 0x93000);
        }
        assert(write_trace[writes - 3] == 0x90052 && write_trace[writes - 1] == 0x90054);
        if (!selected)
            successful_writes = writes;
    }
    for (unsigned scenario = 0; scenario < 5; ++scenario) {
        struct hpclang_rpl_mutation_io io = setup_home_attachments(3);
        int expected = HPCLANG_RPL_MUTATION_NOT_FOUND;
        uint32_t number = 123;
        if (scenario == 1) {
            number = 10;
            put_word(0x90062, 3, 10);
            expected = HPCLANG_RPL_MUTATION_MALFORMED;
        }
        if (scenario == 2) {
            number = 10;
            put_word(0x90000, 5, 0x90049);
            expected = HPCLANG_RPL_MUTATION_BUSY;
        }
        if (scenario == 3) {
            number = 10;
            rejected_read = 0x9006e;
            expected = HPCLANG_RPL_MUTATION_IO;
        }
        if (scenario == 4) {
            number = 10;
            rejected_read = 0x90020;
            expected = HPCLANG_RPL_MUTATION_IO;
        }
        uint8_t before[NIBBLES];
        memcpy(before, memory, sizeof before);
        assert(hpclang_rpl_mutation_home_detach(&io, number) == expected);
        assert(!writes && !memcmp(before, memory, sizeof before));
    }
    for (uint32_t cut = 0; cut < successful_writes; ++cut) {
        struct hpclang_rpl_mutation_io io = setup_home_attachments(3);
        rejected_write = cut;
        assert(hpclang_rpl_mutation_home_detach(&io, 10) == HPCLANG_RPL_MUTATION_INCOMPLETE);
        assert(writes == cut);
    }
    struct hpclang_rpl_mutation_io io = setup_home_attachments(3);
    rejected_commit_read = 0x90020;
    assert(hpclang_rpl_mutation_home_detach(&io, 10) == HPCLANG_RPL_MUTATION_INCOMPLETE);
    assert(writes);
}

static void test_catalog_attached_rejection(void)
{
    for (unsigned scenario = 0; scenario < 7; ++scenario) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
        struct hpclang_rpl_catalog_removal plan;
        assert(hpclang_rpl_catalog_plan_remove(&reader, io.library_table, 35, 10, &plan) == 1);
        put_word(0x92005, 3, 2);
        put_word(0x92008, 3, 10);
        put_word(0x9200b, 5, 0x93000);
        put_word(0x92010, 5, 0x94000);
        put_word(0x92015, 3, 20);
        int expected = HPCLANG_RPL_MUTATION_BUSY;
        switch (scenario) {
        case 0:
            break;
        case 1:
            rejected_read = 0x92021;
            expected = HPCLANG_RPL_MUTATION_IO;
            break;
        case 2:
            put_word(USEROB, 5, 0);
            expected = HPCLANG_RPL_MUTATION_MALFORMED;
            break;
        case 3:
            put_word(0x92000, 5, 0x2a2c);
            expected = HPCLANG_RPL_MUTATION_MALFORMED;
            break;
        case 4:
            rejected_read = USEROB;
            expected = HPCLANG_RPL_MUTATION_IO;
            break;
        case 5:
            put_word(0x92008, 3, 11);
            put_word(0x92015, 3, 10);
            break;
        case 6:
            put_word(USEROB, 5, 0xffff8);
            put_word(0xffff8, 5, DORRP);
            put_word(0xffffd, 3, 1);
            expected = HPCLANG_RPL_MUTATION_MALFORMED;
            break;
        }
        uint8_t before[NIBBLES];
        memcpy(before, memory, sizeof before);
        assert(hpclang_rpl_catalog_remove_repaired(&io, 10, &plan, NULL) == expected);
        assert(!writes && !memcmp(before, memory, sizeof before));
    }
}

static void test_catalog_remove_commit(void)
{
    uint32_t successful_writes = 0;
    for (unsigned index = 0; index < 2; ++index) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        uint32_t old_avmem = get_word(AVMEM, 5);
        struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
        struct hpclang_rpl_catalog_removal plan;
        assert(hpclang_rpl_catalog_plan_remove(&reader, io.library_table, 35, index ? 20 : 10,
                                               &plan) == 1);
        assert(hpclang_rpl_catalog_remove_repaired(&io, index ? 20 : 10, &plan, NULL) == 0);
        assert(get_word(io.library_table, 3) == 1);
        assert(get_word(0x82003, 3) == (index ? 10 : 20));
        assert(get_word(0x82006, 5) == (index ? 0x82ff1 : 0x82ff2));
        assert(get_word(0x806e9, 5) == 0x82ff0);
        assert(get_word(TEMPTOP, 5) == 0x82ff5 && get_word(RSKTOP, 5) == 0x82fff);
        assert(get_word(0x82ffa, 5) == 0x82ff1 && get_word(0x90000, 5) == 0x82ff2);
        assert(get_word(AVMEM, 5) == old_avmem + 3);
        assert(memory[0x82545] == 11);
        assert(write_trace[writes - 3] == io.library_table);
        assert(write_trace[writes - 1] == io.library_table + 2);
        if (!index)
            successful_writes = writes;
    }
    /* Middle/sole removal and caller-proven embedded/register repairs. */
    for (unsigned count = 1; count <= 3; count += 2) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        put_word(io.library_table, 3, count);
        if (count == 3) {
            put_word(0x82023, 3, 30);
            put_word(0x82026, 5, 0x83003);
        }
        put_word(0x82500, 5, 0x83004);
        uint32_t native = 0xabc83004;
        struct hpclang_rpl_register_repair reg = {&native, native};
        const struct hpclang_rpl_reference_edit edit = {7, 0x82500, 0x83004, 0x82ff4, 5};
        const struct hpclang_rpl_catalog_repairs repairs = {.main_mapping = 7,
                                                            .edits = &edit,
                                                            .edit_count = 1,
                                                            .registers = &reg,
                                                            .register_count = 1};
        struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
        struct hpclang_rpl_catalog_removal plan;
        uint32_t number = count == 1 ? 10 : 20;
        assert(hpclang_rpl_catalog_plan_remove(&reader, io.library_table, 51, number, &plan) == 1);
        assert(hpclang_rpl_catalog_remove_repaired(&io, number, &plan, &repairs) == 0);
        assert(get_word(io.library_table, 3) == count - 1);
        assert(get_word(0x824f0, 5) == 0x82ff4 && native == 0xabc82ff4);
        if (count == 3) {
            assert(get_word(0x82003, 3) == 10 && get_word(0x82013, 3) == 30);
            assert(get_word(0x82016, 5) == 0x82ff3);
        }
    }
    /* No write may precede plan/binding, root, guard or edit validation. */
    for (unsigned scenario = 0; scenario < 11; ++scenario) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
        struct hpclang_rpl_catalog_removal plan;
        assert(hpclang_rpl_catalog_plan_remove(&reader, io.library_table, 35, 10, &plan) == 1);
        struct hpclang_rpl_reference_edit edit = {0};
        struct hpclang_rpl_reference_guard guard = {0};
        uint32_t native = 0xabc82004;
        struct hpclang_rpl_register_repair reg = {&native, native};
        struct hpclang_rpl_catalog_repairs repairs = {.main_mapping = 7};
        switch (scenario) {
        case 0:
            ++plan.slot;
            break;
        case 1:
            ++plan.end;
            break;
        case 2:
            ++plan.count;
            break;
        case 3:
            ++plan.binding.address;
            break;
        case 4:
            ++plan.binding.bank_switch;
            break;
        case 5:
            ++plan.binding.flags;
            break;
        case 6:
            put_word(0x90000, 5, 0x82004);
            break;
        case 7:
            repairs.registers = &reg;
            repairs.register_count = 1;
            break;
        case 8:
            edit = (struct hpclang_rpl_reference_edit){7, 0x82004, 0, 0, 1};
            repairs.edits = &edit;
            repairs.edit_count = 1;
            break;
        case 9:
            guard = (struct hpclang_rpl_reference_guard){7, 0x82555, 12, 1};
            repairs.guards = &guard;
            repairs.guard_count = 1;
            break;
        case 10:
            rejected_read = 0x8300e;
            break;
        }
        uint8_t before[NIBBLES];
        memcpy(before, memory, sizeof before);
        assert(hpclang_rpl_catalog_remove_repaired(&io, 10, &plan, &repairs) < 0);
        assert(!writes && !memcmp(before, memory, sizeof before) && native == 0xabc82004);
    }
    /* A source read can fail after root edits have already committed. */
    {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
        struct hpclang_rpl_catalog_removal plan;
        assert(hpclang_rpl_catalog_plan_remove(&reader, io.library_table, 35, 10, &plan) == 1);
        rejected_commit_read = 0x82555;
        assert(hpclang_rpl_catalog_remove_repaired(&io, 10, &plan, NULL) ==
               HPCLANG_RPL_MUTATION_INCOMPLETE);
        assert(writes && get_word(io.library_table, 3) == 2);
    }
    /* Every write boundary, including final count publication, is non-replayable. */
    for (uint32_t cut = 0; cut < successful_writes; ++cut) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
        struct hpclang_rpl_catalog_removal plan;
        assert(hpclang_rpl_catalog_plan_remove(&reader, io.library_table, 35, 10, &plan) == 1);
        rejected_write = cut;
        assert(hpclang_rpl_catalog_remove_repaired(&io, 10, &plan, NULL) ==
               HPCLANG_RPL_MUTATION_INCOMPLETE);
        assert(writes == cut);
    }
}

static void test_catalog_alias_repairs(void)
{
    uint8_t *before = malloc(sizeof memory);
    assert(before);
    for (unsigned fault = 0; fault <= 38; ++fault) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        uint32_t native = 0x82500, *roots[] = {&native};
        io.native_roots = roots;
        io.native_root_count = 1;
        /* Controlled independent catalog: its native target encoding aliases
         * 82580 in the moving region. No actual ROM bank is written. */
        put_word(0x82500, 3, 1);
        put_word(0x82503, 3, 275);
        put_word(0x82506, 5, 0x70000);
        put_word(0x8250b, 5, 0);
        put_word(0x82510, 3, 7);
        const struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
        const struct hpclang_rpl_catalog_move move = {7, 7, 0x82013, 0x83005, 16};
        struct hpclang_rpl_reference_edit edits[2];
        struct hpclang_rpl_reference_guard guards[3];
        struct hpclang_rpl_catalog_reference_plan refs = {edits, 2, 0, guards, 3, 0};
        assert(!hpclang_rpl_catalog_plan_relocation_offsets(&reader, 0x82500, 19, &move, 0,
                                                            alias_catalog_location, NULL, &refs));
        struct hpclang_rpl_catalog_insertion insertion;
        assert(hpclang_rpl_catalog_plan_insert(&reader, io.library_table,
                                               0x100000 - io.library_table, 15, &insertion) == 1);
        const struct hpclang_rpl_library_binding binding = {0x83022, 0, 0};
        const struct hpclang_rpl_catalog_repairs repairs = {
            7, edits, refs.edit_count, guards, refs.guard_count, NULL, 0};
        if (fault < 19)
            memory[0x82500 + fault] ^= 1;
        else if (fault < 38)
            rejected_read = 0x82500 + fault - 19;
        memcpy(before, memory, sizeof memory);
        int result = hpclang_rpl_catalog_insert_repaired(&io, 15, &binding, &insertion, &repairs);
        if (fault < 38) {
            assert(result ==
                   (fault < 19 ? HPCLANG_RPL_MUTATION_MALFORMED : HPCLANG_RPL_MUTATION_IO));
            assert(!writes && native == 0x82500 && !memcmp(before, memory, sizeof memory));
        } else {
            assert(!result && writes && native == 0x82510);
            assert(get_word(0x82516, 5) == 0x70010 && get_word(0x8251b, 5) == 0);
            assert(get_word(io.library_table, 3) == 3);
        }
    }
    free(before);
}

static void test_catalog_observed_guards(void)
{
    uint8_t *before = malloc(sizeof memory);
    assert(before);
    /* Reject a changed field or a failed guard read at every observed nibble.
     * The object lives outside the catalog/root metadata, so those independent
     * preflights cannot substitute for validating the parser's observations. */
    for (unsigned fault = 0; fault <= 50; ++fault) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        uint32_t native = 0x82500, *roots[] = {&native};
        io.native_roots = roots;
        io.native_root_count = 1;
        put_word(0x82500, 5, 0x2a74);
        put_word(0x82505, 5, 0x2baa);
        put_word(0x8250a, 5, 0x12345);
        put_word(0x8250f, 5, 0);
        put_word(0x82514, 5, 0x312b);
        struct hpclang_rpl_reference_guard guards[8];
        struct object_guard_state state = {0};
        assert(!hpclang_rpl_reference_guards_begin(&state.builder, guards, 8));
        const struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
        const struct hpclang_rpl_reference_node object = {0x82500, 25, 0};
        struct hpclang_rpl_reference_node work[1];
        assert(!hpclang_rpl_reference_visit_guarded_nested(
            &reader, &object, work, 1, 2, catalog_object_edge, catalog_object_dependency, &state));
        assert(state.edges == 2 && state.builder.count == 5);
        struct hpclang_rpl_catalog_insertion insertion;
        assert(hpclang_rpl_catalog_plan_insert(&reader, io.library_table,
                                               0x100000 - io.library_table, 15, &insertion) == 1);
        const struct hpclang_rpl_library_binding binding = {0x83022, 0, 0};
        const struct hpclang_rpl_catalog_repairs repairs = {
            7, NULL, 0, guards, state.builder.count, NULL, 0};
        if (fault < 25)
            memory[0x82500 + fault] ^= 1;
        else if (fault < 50)
            rejected_read = 0x82500 + fault - 25;
        memcpy(before, memory, sizeof memory);
        int result = hpclang_rpl_catalog_insert_repaired(&io, 15, &binding, &insertion, &repairs);
        if (fault < 50) {
            assert(result ==
                   (fault < 25 ? HPCLANG_RPL_MUTATION_MALFORMED : HPCLANG_RPL_MUTATION_IO));
            assert(!writes && native == 0x82500 && !memcmp(before, memory, sizeof memory));
        } else {
            assert(!result && writes && native == 0x82510);
            assert(get_word(io.library_table, 3) == 3 && get_word(insertion.slot, 3) == 15);
            assert(!memcmp(memory + 0x82510, before + 0x82500, 25));
        }
    }
    free(before);
}

static void test_catalog_insert_commit(void)
{
    const struct hpclang_rpl_library_binding binding = {0x83022, 0x26c30, 7};
    uint32_t complete_writes = 0;
    for (unsigned number = 5; number <= 25; number += 10) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        const struct hpclang_rpl_reader reader = {io.read_nibble, io.context};
        struct hpclang_rpl_catalog_insertion plan;
        assert(hpclang_rpl_catalog_plan_insert(&reader, io.library_table,
                                               0x100000 - io.library_table, number, &plan) == 1);
        assert(!hpclang_rpl_catalog_insert_repaired(&io, number, &binding, &plan, NULL));
        assert(get_word(io.library_table, 3) == 3);
        assert(get_word(plan.slot, 3) == number);
        assert(get_word(plan.slot + 3, 5) == binding.address);
        assert(get_word(plan.slot + 8, 5) == binding.bank_switch);
        assert(get_word(plan.slot + 13, 3) == binding.flags);
        assert(writes < sizeof write_trace / sizeof write_trace[0]);
        for (unsigned i = 0; i < 3; ++i)
            assert(write_trace[writes - 3 + i] == io.library_table + i);
        for (unsigned i = 0; i < 16; ++i)
            assert(write_trace[writes - 19 + i] == plan.slot + i);
        struct hpclang_rpl_library_binding found;
        assert(hpclang_rpl_library_lookup(&reader, io.library_table, 0x100000 - io.library_table,
                                          number, &found) == 1);
        assert(found.address == binding.address && found.bank_switch == binding.bank_switch &&
               found.flags == binding.flags);
        if (number == 15)
            complete_writes = writes;
    }
    /* Commit failure at growth start/end and every record/count publication nibble. */
    for (unsigned fault = 0; fault < 21; ++fault) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        const struct hpclang_rpl_catalog_insertion plan = {0x82013, 0x82023, 2};
        rejected_write = fault == 0 ? 0 : complete_writes - 21 + fault;
        assert(hpclang_rpl_catalog_insert_repaired(&io, 15, &binding, &plan, NULL) ==
               HPCLANG_RPL_MUTATION_INCOMPLETE);
        assert(writes == rejected_write);
        if (fault <= 18)
            assert(get_word(io.library_table, 3) == 2);
    }
    for (unsigned fault = 0; fault < 8; ++fault) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        struct hpclang_rpl_catalog_insertion plan = {0x82013, 0x82023, 2};
        struct hpclang_rpl_library_binding added = binding;
        uint32_t number = 15;
        if (fault == 0)
            ++plan.slot;
        if (fault == 1)
            ++plan.end;
        if (fault == 2)
            ++plan.count;
        if (fault == 3)
            number = 20;
        if (fault == 4)
            added.address = 0x100000;
        if (fault == 5)
            added.bank_switch = 0x100000;
        if (fault == 6)
            added.flags = 0x1000;
        if (fault == 7)
            put_word(0x82013, 3, 9);
        int result = hpclang_rpl_catalog_insert_repaired(&io, number, &added, &plan, NULL);
        assert(result ==
               (fault == 3 ? HPCLANG_RPL_MUTATION_EXISTS : HPCLANG_RPL_MUTATION_MALFORMED));
        assert(!writes && get_word(io.library_table, 3) == 2);
    }
}

static int stage_read_word(void *context, uint32_t address, unsigned width, uint32_t *value)
{
    *value = 0;
    for (unsigned i = 0; i < width; ++i) {
        uint8_t nibble;
        if (!read_nibble(context, address + i, &nibble))
            return 0;
        *value |= (uint32_t)nibble << (4 * i);
    }
    return 1;
}

static void test_immediate_return_relocation(void)
{
    const struct hpclang_stage_stack_io stage = {NULL, stage_read_word, NULL};
    for (uint32_t instruction = 0; instruction <= 0x30; instruction += 0x10) {
        for (uint32_t cursor = 0; cursor < 8; ++cursor) {
            for (int fail = 0; fail < 2; ++fail) {
                struct hpclang_rpl_mutation_io io = setup_catalog_growth();
                uint32_t pc = 0x82500, stack[8];
                /* Equal numbers below the top deliberately look like roots. */
                for (unsigned i = 0; i < 8; ++i)
                    stack[i] = 0x82555;
                put_word(pc, 2, instruction);
                uint32_t *root = hpclang_stage_immediate_return_root(&stage, pc, stack, cursor);
                assert(root == &stack[(cursor + 7) & 7]);
                /* Independent callers may register the same cell again. */
                uint32_t *roots[] = {&pc, root, root};
                io.native_roots = roots;
                io.native_root_count = 3;
                if (fail)
                    rejected_read = 0x90000;
                int result = hpclang_rpl_mutation_catalog_grow(&io, 0x82013);
                assert(fail ? result < 0 : result == 0);
                uint32_t delta = fail ? 0 : 16;
                assert(pc == 0x82500 + delta && get_word(pc, 2) == instruction);
                for (unsigned i = 0; i < 8; ++i)
                    assert(stack[i] == 0x82555 + (i == ((cursor + 7) & 7) ? delta : 0));
                assert(memory[*root] == 11);
                if (fail)
                    assert(!writes);
                else
                    assert(hpclang_stage_immediate_return_root(&stage, pc, stack, cursor) == root);
            }
        }
    }
}

static void test_conditional_return_relocation(void)
{
    const struct hpclang_stage_stack_io stage = {NULL, stage_read_word, NULL};
    for (unsigned carry = 0; carry < 2; ++carry)
        for (unsigned cursor = 0; cursor < 8; ++cursor)
            for (unsigned fail = 0; fail < 2; ++fail) {
                struct hpclang_rpl_mutation_io io = setup_catalog_growth();
                uint32_t pc = 0x82400, stack[8], *roots[9] = {&pc};
                for (unsigned i = 0; i < 8; ++i)
                    stack[i] = 0x82555;
                stack[(cursor + 7) & 7] = 0x82520;
                put_word(pc, 2, carry ? 0x20 : 0x30);
                put_word(0x82520, 3, carry ? 4 : 5);
                assert(hpclang_stage_return_roots(&stage, pc, stack, cursor, roots + 1) == 2);
                assert(roots[1] == stack + ((cursor + 7) & 7));
                assert(roots[2] == stack + ((cursor + 6) & 7));
                io.native_roots = roots;
                io.native_root_count = 3;
                if (fail)
                    rejected_read = 0x90000;
                int result = hpclang_rpl_mutation_catalog_grow(&io, 0x82013);
                assert(fail ? result < 0 : result == 0);
                unsigned delta = fail ? 0 : 16;
                assert(pc == 0x82400 + delta);
                assert(stack[(cursor + 7) & 7] == 0x82520 + delta);
                for (unsigned i = 0; i < 8; ++i)
                    if (i != ((cursor + 7) & 7))
                        assert(stack[i] == 0x82555 + (i == ((cursor + 6) & 7) ? delta : 0));
                assert(get_word(pc, 2) == (carry ? 0x20u : 0x30u));
                assert(get_word(stack[(cursor + 7) & 7], 3) == (carry ? 4u : 5u));
                if (fail)
                    assert(!writes);
            }
}

static void test_return_chain_relocation(void)
{
    const struct hpclang_stage_stack_io stage = {NULL, stage_read_word, NULL};
    for (unsigned length = 1; length <= 8; ++length) {
        for (unsigned cursor = 0; cursor < 8; ++cursor) {
            for (unsigned fail = 0; fail < 2; ++fail) {
                struct hpclang_rpl_mutation_io io = setup_catalog_growth();
                uint32_t pc = 0x82400, stack[8], before[8];
                uint32_t *roots[9] = {&pc};
                for (unsigned i = 0; i < 8; ++i)
                    stack[i] = 0x82500;
                put_word(pc, 2, 0x10);
                for (unsigned depth = 0; depth < length; ++depth) {
                    stack[(cursor + 7 - depth) & 7] = 0x82500 + depth * 0x20;
                    put_word(0x82500 + depth * 0x20, 2,
                             depth + 1 < length ? 0x10 * (depth % 4) : 0xab);
                }
                memcpy(before, stack, sizeof stack);
                unsigned count = hpclang_stage_return_roots(&stage, pc, stack, cursor, roots + 1);
                assert(count == length);
                io.native_roots = roots;
                io.native_root_count = count + 1;
                if (fail)
                    rejected_read = 0x90000;
                int result = hpclang_rpl_mutation_catalog_grow(&io, 0x82013);
                assert(fail ? result < 0 : result == 0);
                unsigned delta = fail ? 0 : 16;
                assert(pc == 0x82400 + delta);
                for (unsigned depth = 0; depth < 8; ++depth) {
                    unsigned slot = (cursor + 7 - depth) & 7;
                    assert(stack[slot] == before[slot] + (depth < length ? delta : 0));
                }
                if (fail) {
                    assert(!writes);
                    continue;
                }
                /* Independently consume the actual relocated instructions and
                 * stack values, reaching the marker after exactly length pops. */
                uint32_t next = pc;
                for (unsigned depth = 0; depth < length; ++depth) {
                    uint32_t opcode = get_word(next, 2);
                    assert(opcode == 0 || opcode == 0x10 || opcode == 0x20 || opcode == 0x30);
                    next = stack[(cursor + 7 - depth) & 7];
                }
                assert(get_word(next, 2) == 0xab);
            }
        }
    }
}

static void test_catalog_switch_growth(void)
{
    for (unsigned fail = 0; fail < 3; ++fail) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        put_word(0x8200b, 5, 0x82555); /* Main-RAM executable switch. */
        put_word(0x8201b, 5, 0x22f5a); /* Fixed ROM executable switch. */
        put_word(0x82555, 2, 0x10);    /* RTN at the RAM function entry. */
        if (fail)
            rejected_read = fail == 1 ? 0x8200b : 0x8201b;
        int result = hpclang_rpl_mutation_catalog_grow(&io, 0x82013);
        if (fail) {
            assert(result == HPCLANG_RPL_MUTATION_IO && !writes);
            assert(get_word(0x8200b, 5) == 0x82555);
            assert(get_word(0x8201b, 5) == 0x22f5a);
            continue;
        }
        assert(!result);
        assert(get_word(0x8200b, 5) == 0x82565);
        assert(get_word(get_word(0x8200b, 5), 2) == 0x10);
        assert(get_word(0x8202b, 5) == 0x22f5a);
        /* Equal-looking payload addresses refer to the selected bank. Neither
         * follows the main-RAM move of the executable function or the record. */
        assert(get_word(0x82006, 5) == 0x83001);
        assert(get_word(0x82026, 5) == 0x83002);
    }
}

static void test_jump_return_relocation(void)
{
    const struct hpclang_stage_stack_io stage = {NULL, stage_read_word, NULL};
    for (unsigned cursor = 0; cursor < 8; ++cursor) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        uint32_t pc = 0x82400, stack[8];
        for (unsigned i = 0; i < 8; ++i)
            stack[i] = 0x82700;
        stack[(cursor + 7) & 7] = 0x82500;
        stack[(cursor + 6) & 7] = 0x82600;
        put_word(pc, 1, 6); /* GOTO back to RTN. */
        put_word(pc + 1, 3, (0x823e0 - pc - 1) & 0xfff);
        put_word(0x823e0, 2, 0x10);
        put_word(0x82500, 2, 0xc8); /* GOLONG back to RTNCC. */
        put_word(0x82502, 4, (0x824e0 - 0x82502) & 0xffff);
        put_word(0x824e0, 2, 0x30);
        put_word(0x82600, 2, 0xd8); /* GOVLNG to RTNSC needs an operand repair. */
        put_word(0x82602, 5, 0x82620);
        put_word(0x82620, 2, 0x20);
        put_word(0x82700, 2, 0xab);
        uint32_t *roots[9] = {&pc};
        assert(hpclang_stage_return_roots(&stage, pc, stack, cursor, roots + 1) == 3);
        io.native_roots = roots;
        io.native_root_count = 4;
        const struct hpclang_rpl_reference_edit edit = {1, 0x82602, 0x82620, 0x82630, 5};
        const struct hpclang_rpl_catalog_repairs repairs = {1, &edit, 1, NULL, 0, NULL, 0};
        assert(!hpclang_rpl_mutation_catalog_grow_repaired(&io, 0x82013, &repairs));
        assert(pc == 0x82410);
        for (unsigned depth = 0; depth < 8; ++depth) {
            uint32_t expected = depth == 0   ? 0x82510
                                : depth == 1 ? 0x82610
                                : depth == 2 ? 0x82710
                                             : 0x82700;
            assert(stack[(cursor + 7 - depth) & 7] == expected);
        }
        assert(get_word(pc + 1, 3) == ((0x823f0 - pc - 1) & 0xfff));
        assert(get_word(0x82512, 4) == ((0x824f0 - 0x82512) & 0xffff));
        assert(get_word(0x82612, 5) == 0x82630);
        assert(get_word(0x823f0, 2) == 0x10 && get_word(0x824f0, 2) == 0x30 &&
               get_word(0x82630, 2) == 0x20 && get_word(0x82710, 2) == 0xab);
        assert(hpclang_stage_return_roots(&stage, pc, stack, cursor, roots + 1) == 3);
    }
}

static void test_call_return_relocation(void)
{
    const struct hpclang_stage_stack_io stage = {NULL, stage_read_word, NULL};
    for (unsigned kind = 0; kind < 3; ++kind)
        for (unsigned cursor = 0; cursor < 8; ++cursor) {
            struct hpclang_rpl_mutation_io io = setup_catalog_growth();
            uint32_t pc = 0x82400, stack[8], *roots[9] = {&pc};
            for (unsigned i = 0; i < 8; ++i)
                stack[i] = 0x82600;
            unsigned prefix = kind ? 2 : 1;
            unsigned width = kind == 0 ? 3 : kind == 1 ? 4 : 5;
            put_word(pc, prefix, kind == 0 ? 7 : kind == 1 ? 0xe8 : 0xf8);
            put_word(pc + prefix, width, kind == 2 ? 0x82500 : 0x100 - prefix - width);
            put_word(pc + prefix + width, 2, 0x10);
            put_word(0x82500, 2, 0x10);
            put_word(0x82600, 2, 0xab);
            assert(hpclang_stage_return_roots(&stage, pc, stack, cursor, roots + 1) == 1);
            assert(roots[1] == stack + ((cursor + 7) & 7));
            io.native_roots = roots;
            io.native_root_count = 2;
            const struct hpclang_rpl_reference_edit edit = {1, pc + prefix, 0x82500, 0x82510, 5};
            const struct hpclang_rpl_catalog_repairs repairs = {1, &edit, kind == 2, NULL,
                                                                0, NULL,  0};
            assert(!hpclang_rpl_mutation_catalog_grow_repaired(&io, 0x82013, &repairs));
            assert(pc == 0x82410);
            for (unsigned i = 0; i < 8; ++i)
                assert(stack[i] == (i == ((cursor + 7) & 7) ? 0x82610 : 0x82600));
            assert(get_word(pc + prefix, width) == (kind == 2 ? 0x82510 : 0x100 - prefix - width));
            assert(hpclang_stage_return_roots(&stage, pc, stack, cursor, roots + 1) == 1);
        }
}

static void test_catalog_merged_repairs(void)
{
    for (unsigned width = 1; width <= 5; ++width) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        uint32_t first = 0x82555, second = first;
        uint32_t *roots[] = {&first, &second, &first};
        io.native_roots = roots;
        io.native_root_count = 3;
        put_word(0x8200b, 5, 0x82555);
        put_word(0x82500, width, 3);
        /* Two native discovery paths plus two caller discoveries name one cell. */
        put_word(TOUCHTAB, 5, 0x806d7);
        put_word(0x806da, 5, 0x82555);
        watched_field = 0x806da;
        watched_writes = 0;
        const struct hpclang_rpl_reference_edit edits[] = {
            {7, 0x8200b, 0x82555, 0x82565, 5},
            {7, 0x82500, 3, 4, width},
            {7, 0x806da, 0x82555, 0x82565, 5},
            {7, 0x806da, 0x82555, 0x82565, 5},
        };
        const struct hpclang_rpl_reference_guard guards[] = {{7, 0x82000, 2, 3},
                                                             {7, 0x82500, 3, width}};
        const struct hpclang_rpl_catalog_repairs repairs = {7, edits, 4, guards, 2, NULL, 0};
        assert(!hpclang_rpl_mutation_catalog_grow_repaired(&io, 0x82013, &repairs));
        assert(get_word(0x8200b, 5) == 0x82565);
        assert(get_word(0x806da, 5) == 0x82565 && watched_writes == 1);
        assert(get_word(0x82510, width) == 4);
        assert(first == 0x82565 && second == first);
        assert(get_word(RSKTOP, 5) == 0x8301f);
    }
    watched_field = UINT32_MAX;
    /* All faults must reject the complete operation, including native roots and
     * memory that would otherwise have been moved before the extra repair. */
    for (unsigned fault = 0; fault < 16; ++fault) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        uint32_t native = 0x82555, *roots[] = {&native};
        io.native_roots = roots;
        io.native_root_count = 1;
        put_word(0x8200b, 5, 0x82555);
        put_word(0x82500, 3, 3);
        struct hpclang_rpl_reference_edit edits[] = {{7, 0x8200b, 0x82555, 0x82565, 5},
                                                     {7, 0x82500, 3, 4, 3}};
        struct hpclang_rpl_reference_guard guard = {7, 0x82000, 2, 3};
        struct hpclang_rpl_catalog_repairs repairs = {7, edits, 2, &guard, 1, NULL, 0};
        switch (fault) {
        case 0:
            ++edits[0].value;
            break; /* Conflicting native switch repair. */
        case 1:
            --edits[0].width;
            break; /* Partial root overlap. */
        case 2:
            ++edits[1].expected;
            break;
        case 3:
            ++edits[1].mapping;
            break;
        case 4:
            ++guard.expected;
            break;
        case 5:
            ++guard.mapping;
            break;
        case 6:
            rejected_read = 0x82501;
            break;
        case 7:
            edits[1].field = 0x82012;
            break; /* Split by insertion. */
        case 8:
            edits[1].field = 0x8300f;
            break; /* Overwritten destination. */
        case 9:
            edits[1].field = 0x8300e;
            break; /* Split by movement end. */
        case 10:
            edits[1].field = 0x82000;
            break; /* Catalog count belongs to caller. */
        case 11:
            edits[1] = (struct hpclang_rpl_reference_edit){7, 0x806df, 0, 1, 5};
            break;
        case 12:
            edits[1] = (struct hpclang_rpl_reference_edit){7, RSKTOP, 0x8300f, 1, 5};
            break;
        case 13:
            repairs.edits = NULL;
            break;
        case 14:
            guard.width = 6;
            break;
        case 15:
            guard.field = rejected_read = 0x95000; /* Late guard outside the move. */
            guard.expected = 0;
            break;
        }
        uint8_t *before = malloc(sizeof memory);
        assert(before);
        memcpy(before, memory, sizeof memory);
        assert(hpclang_rpl_mutation_catalog_grow_repaired(&io, 0x82013, &repairs) < 0);
        assert(!writes && native == 0x82555 && !memcmp(before, memory, sizeof memory));
        free(before);
    }
}

static void test_catalog_register_repairs(void)
{
    const uint32_t addresses[] = {0, 0x82012, 0x82013, 0x82555, 0x8300e, 0x8300f, 0xfffff};
    const uint32_t tags[] = {0, 1, 0x800, 0xabc, 0xfff};
    for (unsigned a = 0; a < sizeof addresses / sizeof addresses[0]; ++a) {
        for (unsigned t = 0; t < sizeof tags / sizeof tags[0]; ++t) {
            struct hpclang_rpl_mutation_io io = setup_catalog_growth();
            uint32_t reg[2] = {(tags[t] << 20) | addresses[a], 0xdeadbeef};
            uint32_t scalar = reg[0];
            const struct hpclang_rpl_register_repair registers[] = {
                {reg, reg[0]},
                {reg, reg[0]},
            };
            const struct hpclang_rpl_catalog_repairs repairs = {.registers = registers,
                                                                .register_count = 2};
            uint32_t *roots[] = {reg};
            /* A proven full-word root and the low-address field may agree. */
            if (!tags[t]) {
                io.native_roots = roots;
                io.native_root_count = 1;
            }
            assert(!hpclang_rpl_mutation_catalog_grow_repaired(&io, 0x82013, &repairs));
            uint32_t moved = addresses[a] >= 0x82013 && addresses[a] < 0x8300f;
            assert(reg[0] == ((tags[t] << 20) | (addresses[a] + 16 * moved)));
            assert(reg[1] == 0xdeadbeef && scalar == registers[0].expected);
        }
    }
    for (unsigned fault = 0; fault < 7; ++fault) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        uint32_t reg[2] = {0xabc82555, 0xdeadbeef};
        uint32_t *roots[] = {reg};
        struct hpclang_rpl_register_repair registers[] = {
            {reg, reg[0]},
            {reg, reg[0]},
        };
        struct hpclang_rpl_catalog_repairs repairs = {.registers = registers, .register_count = 2};
        switch (fault) {
        case 0:
            registers[0].expected ^= 0x100000; /* Non-address bits are guarded. */
            break;
        case 1:
            registers[1].expected++; /* Conflicting discoveries of one word. */
            break;
        case 2:
            registers[0].word = NULL;
            break;
        case 3:
            repairs.registers = NULL;
            break;
        case 4:
            registers[0].word = (uint32_t *)((unsigned char *)reg + 1);
            break;
        case 5:
            /* Existing whole-word root remains unchanged; masked repair moves. */
            io.native_roots = roots;
            io.native_root_count = 1;
            break;
        case 6:
            registers[0].expected = registers[1].expected = 0xabc82554;
            break;
        }
        uint8_t *before = malloc(sizeof memory);
        assert(before);
        memcpy(before, memory, sizeof memory);
        assert(hpclang_rpl_mutation_catalog_grow_repaired(&io, 0x82013, &repairs) ==
               HPCLANG_RPL_MUTATION_MALFORMED);
        assert(!writes && reg[0] == 0xabc82555 && reg[1] == 0xdeadbeef);
        assert(!memcmp(before, memory, sizeof memory));
        free(before);
    }
    struct hpclang_rpl_mutation_io io = setup_catalog_growth();
    uint32_t reg[2] = {0xabc82555, 0xdeadbeef};
    const struct hpclang_rpl_register_repair entry = {reg, reg[0]};
    const struct hpclang_rpl_catalog_repairs repairs = {.registers = &entry, .register_count = 1};
    rejected_commit_read = 0x82555;
    assert(hpclang_rpl_mutation_catalog_grow_repaired(&io, 0x82013, &repairs) ==
           HPCLANG_RPL_MUTATION_INCOMPLETE);
    assert(reg[0] == 0xabc82565 && reg[1] == 0xdeadbeef);
}

static void test_catalog_commit_failures(void)
{
    struct hpclang_rpl_mutation_io io = setup_catalog_growth();
    uint32_t native = 0x82555, *roots[] = {&native};
    io.native_roots = roots;
    io.native_root_count = 1;
    assert(!hpclang_rpl_mutation_catalog_grow(&io, 0x82013));
    uint32_t total = writes;
    assert(total && total <= sizeof write_trace / sizeof write_trace[0]);
    uint32_t failures[7] = {0, total / 2, total - 1};
    const uint32_t publications[] = {0x806e9, TEMPTOP, RSKTOP, AVMEM};
    for (unsigned i = 0; i < 4; ++i) {
        unsigned at = 0;
        while (at < total && write_trace[at] != publications[i])
            ++at;
        assert(at < total);
        failures[3 + i] = at;
    }
    for (unsigned i = 0; i < sizeof failures / sizeof failures[0]; ++i) {
        io = setup_catalog_growth();
        native = 0x82555;
        io.native_roots = roots;
        io.native_root_count = 1;
        rejected_write = failures[i];
        assert(hpclang_rpl_mutation_catalog_grow(&io, 0x82013) == HPCLANG_RPL_MUTATION_INCOMPLETE);
        assert(writes == rejected_write);
        /* Even failing the first nibble write can follow an ARM-root update. */
        assert(native == 0x82565);
    }
    io = setup_catalog_growth();
    native = 0x82555;
    io.native_roots = roots;
    io.native_root_count = 1;
    rejected_commit_read = 0x82555;
    assert(hpclang_rpl_mutation_catalog_grow(&io, 0x82013) == HPCLANG_RPL_MUTATION_INCOMPLETE);
    assert(writes && native == 0x82565);
}

#ifdef HPCLANG_TEST_WRAP_ALLOC
static void test_catalog_allocation_lifetime(void)
{
    for (unsigned failure = 0; failure < 4; ++failure) {
        struct hpclang_rpl_mutation_io io = setup_catalog_growth();
        uint32_t native = 0x82555, *roots[] = {&native};
        io.native_roots = roots;
        io.native_root_count = 1;
        struct hpclang_rpl_reference_guard guard = {1, io.library_table, 2, 3};
        const struct hpclang_rpl_catalog_repairs repairs = {1, NULL, 0, &guard, 1, NULL, 0};
        if (failure == 1)
            ++guard.expected;
        if (failure == 3)
            rejected_write = 0;
        uint8_t *before = malloc(sizeof memory);
        assert(before);
        memcpy(before, memory, sizeof memory);
        assert(!tracked_allocation);
        reject_allocation = failure == 0;
        allocation_attempts = allocation_frees = 0;
        track_allocation = 1;
        int result = hpclang_rpl_mutation_catalog_grow_repaired(&io, 0x82013, &repairs);
        track_allocation = reject_allocation = 0;
        const int expected[] = {HPCLANG_RPL_MUTATION_NOMEM, HPCLANG_RPL_MUTATION_MALFORMED,
                                HPCLANG_RPL_MUTATION_OK, HPCLANG_RPL_MUTATION_INCOMPLETE};
        assert(result == expected[failure]);
        assert(allocation_attempts == 1 && allocation_frees == (failure != 0));
        assert(!tracked_allocation);
        if (failure < 2)
            assert(!writes && native == 0x82555 && !memcmp(before, memory, sizeof memory));
        else
            assert(native == 0x82565);
        free(before);
    }
}
#endif

static void test_catalog_growth(void)
{
    struct hpclang_rpl_mutation_io io = setup_catalog_growth();
    uint32_t native = 0x83003, *roots[] = {&native};
    io.native_roots = roots;
    io.native_root_count = 1;
    uint32_t before_avmem = get_word(AVMEM, 5);
    assert(hpclang_rpl_mutation_catalog_grow(&io, 0x82013) == 0);
    assert(get_word(io.library_table, 3) == 2); /* Caller publishes the new entry/count. */
    assert(get_word(0x82006, 5) == 0x83011);
    assert(get_word(0x82023, 3) == 20 && get_word(0x82026, 5) == 0x83012);
    assert(memory[0x82565] == 11);
    assert(get_word(0x83015, 5) == 0 && get_word(0x8301a, 5) == 0x83011);
    assert(get_word(0x90000, 5) == 0x83012 && native == 0x83013);
    assert(get_word(0x806e9, 5) == 0x83010 && get_word(TEMPTOP, 5) == 0x83015);
    assert(get_word(RSKTOP, 5) == 0x8301f);
    assert(get_word(AVMEM, 5) ==
           before_avmem - ((0x90000 - 0x8300f) / 5 - (0x90000 - 0x8301f) / 5));

    /* Independent discovery paths may name the same ARM-side cell. Move each
     * cell once, while retaining distinct cells pointing at the same object. */
    io = setup_catalog_growth();
    uint32_t first = 0x82555, second = first, fixed = 0x12345;
    uint32_t *repeated[] = {&first, &second, &first, &fixed, &second};
    io.native_roots = repeated;
    io.native_root_count = sizeof repeated / sizeof repeated[0];
    assert(hpclang_rpl_mutation_catalog_grow(&io, 0x82013) == 0);
    assert(first == 0x82565 && second == first && fixed == 0x12345);
    assert(memory[first] == 11 && get_word(0x90000, 5) == 0x83012);

    /* Interior unreadability, roots, native fences and bad record positions
     * must all fail before either RAM or ARM-side roots are modified. */
    uint32_t *invalid_roots[] = {&native, &native, NULL};
    for (unsigned failure = 0; failure < 9; ++failure) {
        io = setup_catalog_growth();
        native = 0x83003;
        io.native_roots = roots;
        io.native_root_count = 1;
        uint32_t position = 0x82013;
        if (failure == 0)
            rejected_read = 0x82555;
        if (failure == 1)
            rejected_read = 0x90000;
        if (failure == 2)
            io.temp_limit = 0x83023; /* Only 20 nibbles remain. */
        if (failure == 3)
            ++position;
        if (failure == 4)
            put_word(AVMEM, 5, 0);
        if (failure == 5)
            io.stack_reserve_nibbles = 0xd000;
        if (failure == 6)
            put_word(0x806e9, 5, 0x82022);
        if (failure == 7)
            put_word(io.library_table, 3, 0xfff);
        if (failure == 8) {
            io.native_roots = invalid_roots;
            io.native_root_count = sizeof invalid_roots / sizeof invalid_roots[0];
        }
        uint8_t *before = malloc(sizeof memory);
        assert(before);
        memcpy(before, memory, sizeof memory);
        assert(hpclang_rpl_mutation_catalog_grow(&io, position) < 0);
        assert(writes == 0 && native == 0x83003 && !memcmp(before, memory, sizeof memory));
        free(before);
    }
    io = setup_catalog_growth();
    put_word(0x8201b, 5, 1);      /* Bank-qualified binding is not a main-RAM address. */
    put_word(RSKTOP, 5, 0x83005); /* Empty RPL return stack: TEMPTOP == RSKTOP. */
    put_word(AVMEM, 5, (0x90000 - 0x83005) / 5);
    assert(hpclang_rpl_mutation_catalog_grow(&io, 0x82003) == 0);
    assert(get_word(TEMPTOP, 5) == 0x83015 && get_word(RSKTOP, 5) == 0x83015);
    assert(get_word(0x82026, 5) == 0x83002 && get_word(0x8202b, 5) == 1);
}

static void test_boolean_return_formats(void)
{
    int (*push[])(const struct hpclang_rpl_mutation_io *, int) = {
        hpclang_rpl_mutation_push_boolean, hpclang_rpl_mutation_push_real_boolean};
    const uint32_t references[2][2] = {{0x3ac0, 0x3a81}, {0x2f937, 0x2f94c}};
    for (unsigned format = 0; format < 2; ++format) {
        for (int value = 0; value <= 1; ++value) {
            struct hpclang_rpl_mutation_io io = setup(2);
            uint32_t top = get_word(DSKTOP, 5), free_cells = get_word(AVMEM, 5);
            uint32_t old_first = get_word(top, 5), old_second = get_word(top + 5, 5);
            uint32_t temp = get_word(TEMPTOP, 5);
            assert(push[format](&io, value) == HPCLANG_RPL_MUTATION_OK);
            assert(get_word(DSKTOP, 5) == top - 5);
            assert(get_word(top - 5, 5) == references[format][value]);
            assert(get_word(AVMEM, 5) == free_cells - 1);
            assert(get_word(top, 5) == old_first && get_word(top + 5, 5) == old_second);
            assert(get_word(TEMPTOP, 5) == temp && writes == 15);
        }
        for (int invalid = -1; invalid <= 2; invalid += 3) {
            struct hpclang_rpl_mutation_io io = setup(0);
            assert(push[format](&io, invalid) == HPCLANG_RPL_MUTATION_MALFORMED);
            assert(!writes);
        }
        struct hpclang_rpl_mutation_io io = setup(0);
        io.user_limit = get_word(DSKTOP, 5);
        assert(push[format](&io, 1) == HPCLANG_RPL_MUTATION_NOSPACE && !writes);
        io = setup(0);
        rejected_read = DSKTOP;
        assert(push[format](&io, 1) != HPCLANG_RPL_MUTATION_OK && !writes);
    }
}

int main(void)
{
    test_boolean_return_formats();
    test_jump_return_relocation();
    test_call_return_relocation();
    test_catalog_commit_failures();
#ifdef HPCLANG_TEST_WRAP_ALLOC
    test_catalog_allocation_lifetime();
#endif
    test_catalog_merged_repairs();
    test_catalog_register_repairs();
    test_immediate_return_relocation();
    test_conditional_return_relocation();
    test_return_chain_relocation();
    test_catalog_growth();
    test_catalog_observed_guards();
    test_catalog_alias_repairs();
    test_catalog_insert_commit();
    test_catalog_remove_commit();
    test_catalog_attached_rejection();
    test_home_detach();
    test_catalog_switch_growth();
    test_temp_allocation_visit();
    test_read_only_root_visit();
    test_root_sources();
    test_display_root_relocation();
    test_attachment_relocation();
    test_port1_append_respects_native_pool();
    test_port1_library_append();
    test_port1_append_publication_failures();
    test_port1_edit_owned_suffix();
    test_port0_create_library();
    test_library_table_reference_preservation();
    test_port_remove_preserves_backup_references();
    test_backup_overwrite_preserves_old_value_and_crc();
    test_overwrite_port_value_preserves_references();
    test_port0_create_preserves_home_and_library();
    test_stack_reorder();
    test_native_arena_pins_prefix_without_overwriting_payload();
    test_push_transaction();
    test_temp_allocation_retargets_existing_reference();
    test_avmem_reservation_is_preserved();
    test_named_preservation_accounting();
    test_stack_reserve_blocks_last_cell();
    test_nospace_is_non_mutating();
    test_drop_and_replace();
    test_named_same_size_replace();
    test_named_reference_preservation_nospace_is_non_mutating();
    test_named_create();
    test_recovery_reservation_create();
    test_recovery_reservation_discard_created();
    test_recovery_reservation_hidden_preflight();
    test_recovery_creation_commit();
    test_recovery_reservation_binding();
    test_named_create_nospace_is_non_mutating();
    test_named_remove_oldest_preserves_reference();
    test_named_remove_newest();
    test_all_reference_families_survive_preservation();
    test_reference_preflight_rejects_before_writing();
    test_impossible_free_memory_is_rejected();
    test_nested_mutation_preserves_context_and_references();
    test_named_remove_missing_is_non_mutating();
    test_named_size_changing_replace();
    test_named_size_change_preserves_reference();
    test_userob_shift_roundtrip();
    test_userob_shift_rejects_removed_reference();
    test_retained_reference_does_not_hide_invalid_root();
    test_userob_shift_nospace_is_non_mutating();
    return 0;
}
