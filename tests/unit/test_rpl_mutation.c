#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "object-snapshot.h"
#include "rpl-mutation.h"

enum {
    NIBBLES = 0x100000u,
    TEMPTOP = 0x806eeu,
    RSKTOP = 0x806f3u,
    DSKTOP = 0x806f8u,
    EDITLINE = 0x806fdu,
    AVMEM = 0x80e9bu
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
        memory[address + i] =
            (uint8_t)((value >> (i * 4u)) & 0x0fu);
}

static uint32_t get_word(uint32_t address, uint32_t nibbles)
{
    assert(address + nibbles <= NIBBLES);
    uint32_t value = 0u;
    for (uint32_t i = 0u; i < nibbles; ++i)
        value |= (uint32_t)memory[address + i] << (i * 4u);
    return value;
}

static struct hpclang_object *make_char(uint8_t ch)
{
    struct hpclang_object *object =
        malloc(sizeof(*object) + 4u);
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

static void test_push_transaction(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct hpclang_object *object = make_char('A');

    uint8_t original[0x1000];
    memcpy(original, &memory[0x84000u], sizeof(original));

    assert(hpclang_rpl_mutation_push_snapshot(&io, object) ==
           HPCLANG_RPL_MUTATION_OK);

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

    assert(get_word(AVMEM, 5u) ==
           (0x8fffbu - (0x85000u + delta)) / 5u);

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

    assert(hpclang_rpl_mutation_push_snapshot(&io, object) ==
           HPCLANG_RPL_MUTATION_OK);
    assert(get_word(AVMEM, 5u) ==
           old_avmem - temp_cost - 1u);

    io = setup(1u);
    old_avmem = get_word(AVMEM, 5u) - 9u;
    put_word(AVMEM, 5u, old_avmem);
    rstk = get_word(RSKTOP, 5u);
    dstk = get_word(DSKTOP, 5u);
    old_cells = (dstk - rstk) / 5u;
    new_cells = (dstk - (rstk + delta)) / 5u;
    temp_cost = old_cells - new_cells;

    assert(hpclang_rpl_mutation_replace(&io, 1u, object) ==
           HPCLANG_RPL_MUTATION_OK);
    assert(get_word(AVMEM, 5u) ==
           old_avmem - temp_cost);

    hpclang_object_release(object);
}

static void test_stack_reserve_blocks_last_cell(void)
{
    struct hpclang_rpl_mutation_io io = setup(1u);
    struct hpclang_object *object = make_char('S');

    uint32_t dstk = get_word(DSKTOP, 5u);
    io.user_limit = dstk - 10u;
    io.stack_reserve_nibbles = 5u;

    uint32_t before_ttop = get_word(TEMPTOP, 5u);
    uint32_t before_rstk = get_word(RSKTOP, 5u);
    uint32_t before_dstk = get_word(DSKTOP, 5u);
    assert(hpclang_rpl_mutation_push_snapshot(&io, object) ==
           HPCLANG_RPL_MUTATION_NOSPACE);
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

    assert(hpclang_rpl_mutation_push_snapshot(&io, object) ==
           HPCLANG_RPL_MUTATION_NOSPACE);
    assert(get_word(TEMPTOP, 5u) == ttop);
    assert(get_word(RSKTOP, 5u) == rstk);
    assert(get_word(DSKTOP, 5u) == dstk);
    assert(get_word(AVMEM, 5u) == avmem);

    io = setup(1u);
    io.user_limit = 0x8fffcu;
    ttop = get_word(TEMPTOP, 5u);
    rstk = get_word(RSKTOP, 5u);
    assert(hpclang_rpl_mutation_push_snapshot(&io, object) ==
           HPCLANG_RPL_MUTATION_NOSPACE);
    assert(get_word(TEMPTOP, 5u) == ttop);
    assert(get_word(RSKTOP, 5u) == rstk);

    hpclang_object_release(object);
}

static void test_drop_and_replace(void)
{
    struct hpclang_rpl_mutation_io io = setup(2u);
    struct hpclang_object *object = make_char('C');

    uint32_t old_top = get_word(DSKTOP, 5u);
    assert(hpclang_rpl_mutation_replace(&io, 2u, object) ==
           HPCLANG_RPL_MUTATION_OK);
    assert(get_word(DSKTOP, 5u) == old_top);

    uint32_t replacement = get_word(old_top + 5u, 5u);
    assert(replacement == 0x84001u);
    assert_char_at(replacement, 'C');

    uint32_t avmem = get_word(AVMEM, 5u);
    assert(hpclang_rpl_mutation_drop(&io, 2u) ==
           HPCLANG_RPL_MUTATION_OK);
    assert(get_word(DSKTOP, 5u) == old_top + 10u);
    assert(get_word(AVMEM, 5u) == avmem + 2u);

    uint32_t before = get_word(DSKTOP, 5u);
    assert(hpclang_rpl_mutation_drop(&io, 1u) ==
           HPCLANG_RPL_MUTATION_LEVEL);
    assert(get_word(DSKTOP, 5u) == before);

    hpclang_object_release(object);
}

int main(void)
{
    test_push_transaction();
    test_avmem_reservation_is_preserved();
    test_stack_reserve_blocks_last_cell();
    test_nospace_is_non_mutating();
    test_drop_and_replace();
    return 0;
}
