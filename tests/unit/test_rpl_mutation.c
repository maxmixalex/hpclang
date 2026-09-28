#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "object-snapshot.h"
#include "rpl-mutation.h"
#include "rpl-object-store.h"

enum
{
    NIBBLES = 0x100000u,
    TEMPTOP = 0x806eeu,
    RSKTOP = 0x806f3u,
    DSKTOP = 0x806f8u,
    EDITLINE = 0x806fdu,
    USEROB = 0x80711u,
    CONTEXT = 0x8071bu,
    AVMEM = 0x80e9bu,
    DORRP = 0x02a96u
};

static uint8_t memory[NIBBLES];

static int read_nibble(void *context, uint32_t address, uint8_t *value)
{
    (void)context;
    if (value == NULL || address >= NIBBLES)
        return 0;
    *value = memory[address] & 0x0fu;
    return 1;
}

static int write_nibble(void *context, uint32_t address, uint8_t value)
{
    (void)context;
    if (address >= NIBBLES || value > 0x0fu)
        return 0;
    memory[address] = value;
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

    assert(get_word(DSKTOP, 5u) == old_dstk);
    assert(get_word(AVMEM, 5u) == old_avmem);
    assert(get_word(USEROB, 5u) == old_home);
    assert(get_word(pair.chain, 5u) == old_chain);
    assert(get_word(pair.link_a, 5u) == old_link);
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

    assert(hpclang_rpl_mutation_store_replace_same_size(&io, 0x92000u, (const uint8_t *)"A", 1u,
                                                        char_b) == HPCLANG_RPL_MUTATION_OK);
    assert_char_at(object_address, 'B');

    const uint32_t dstk = get_word(DSKTOP, 5u);
    put_word(dstk, 5u, object_address);
    assert(hpclang_rpl_mutation_store_replace_same_size(&io, 0x92000u, (const uint8_t *)"A", 1u,
                                                        char_c) == HPCLANG_RPL_MUTATION_OK);
    assert_char_at(object_address, 'C');

    uint32_t preserved = get_word(dstk, 5u);
    assert(preserved != object_address);
    assert_char_at(preserved, 'B');

    assert(hpclang_rpl_mutation_store_replace_same_size(&io, 0x92000u, (const uint8_t *)"A", 1u,
                                                        char_c) == HPCLANG_RPL_MUTATION_OK);
    assert_char_at(object_address, 'C');
    assert(get_word(dstk, 5u) == preserved);
    assert_char_at(preserved, 'B');

    assert(hpclang_rpl_mutation_store_replace_same_size(&io, 0x92000u, (const uint8_t *)"NO", 2u,
                                                        char_b) == HPCLANG_RPL_MUTATION_NOT_FOUND);

    uint32_t original_size = char_c->nibble_count;
    char_c->nibble_count = original_size + 1u;
    assert(hpclang_rpl_mutation_store_replace_same_size(&io, 0x92000u, (const uint8_t *)"A", 1u,
                                                        char_c) ==
           HPCLANG_RPL_MUTATION_UNSUPPORTED);
    char_c->nibble_count = original_size;

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

    assert(hpclang_rpl_mutation_store_replace_same_size(&io, 0x92000u, (const uint8_t *)"A", 1u,
                                                        char_c) == HPCLANG_RPL_MUTATION_NOSPACE);
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

int main(void)
{
    test_push_transaction();
    test_temp_allocation_retargets_existing_reference();
    test_avmem_reservation_is_preserved();
    test_stack_reserve_blocks_last_cell();
    test_nospace_is_non_mutating();
    test_drop_and_replace();
    test_named_same_size_replace();
    test_named_reference_preservation_nospace_is_non_mutating();
    test_named_create();
    test_named_create_nospace_is_non_mutating();
    test_named_remove_oldest_preserves_reference();
    test_named_remove_newest();
    test_named_remove_missing_is_non_mutating();
    test_named_size_changing_replace();
    test_named_size_change_preserves_reference();
    test_userob_shift_roundtrip();
    test_userob_shift_rejects_removed_reference();
    test_userob_shift_nospace_is_non_mutating();
    return 0;
}
