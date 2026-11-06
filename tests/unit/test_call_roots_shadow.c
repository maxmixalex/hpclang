/* Exercise the actual maintained gate callbacks against owned SRAM and aliases.
 * Unused application/launcher entry sections are discarded by this host target. */
#include "../e2e/code-relocation/call-roots.c"
#include <assert.h>

static uint8_t ram[0x80000], before[0x80000];
static struct execution_probe probe;
static uint32_t section = 0x08000c1a, sections[2] = {0xc1a, 0x100c1a};

uint16_t hpclang_flash_read16(void *context, uint32_t address)
{
    (void)context;
    (void)address;
    return 0;
}

static void word(uint32_t at, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i)
        ram[at + i] = (uint8_t)(value >> (8 * i));
}

static struct hpclang_rpl_code_store fixture(void)
{
    memset(ram, 0x44, sizeof ram);
    memset(&probe, 0, sizeof probe);
    section = 0x08000c1a;
    word(0x80 * 4, 0x08010000);
    word(0x404 + 0x80 * 4, 0x08010000);
    word(0x404 + 0xa0 * 4, 0x08010000);
    word(0xba0, UINT32_MAX);
    word(0xbb0, UINT32_MAX);
    word(0x90 * 4, 0x08020000);
    word(0xa0 * 4, 0x08010000); /* A distinct logical page aliases the store. */
    ram[0x20000] = 0xde;
    ram[0x20001] = 0xbc;
    ram[0x20002] = 0x4a;
    assert(!hpclang_rpl_stock_view_capture(&probe.view, ram, 0x08000000, &section, sections));
    assert(!hpclang_rpl_reference_guards_begin(&probe.builder, probe.guards, 512));
    assert(!hpclang_rpl_reference_edits_begin(&probe.plan, probe.edits, 128));
    struct hpclang_rpl_code_store store = {
        .instruction = 0x2c3c,
        .nibbles = 5,
        .address = {.kind = HPCLANG_RPL_CODE_USE_STORE_ADDRESS, .value = 0x80002},
        .value = 0xabcde};
    for (unsigned digit = 0; digit < 5; ++digit) {
        store.origins[digit] = 0x90001 + digit;
        struct hpclang_rpl_code_origin_cell cell;
        assert(!execution_source(&probe, store.origins[digit], &cell));
        struct hpclang_rpl_reference_edit edit = {cell.mapping, cell.field, cell.expected,
                                                  cell.expected, 1};
        assert(!hpclang_rpl_reference_edits_add(&probe.plan, &edit));
    }
    memcpy(before, ram, sizeof ram);
    return store;
}

static struct hpclang_rpl_code_store payload_fixture(void)
{
    struct hpclang_rpl_code_store store = fixture();
    assert(!execution_store(&probe, &store));
    store.instruction = 0x2a1ba;
    store.address.value = 0x80020;
    store.nibbles = 8;
    store.value = 0x444abcde;
    for (unsigned i = 0; i < 8; ++i)
        store.origins[i] = 0x90001 + i;
    assert(!execution_store(&probe, &store));
    assert(probe.stores == 2 && probe.stored_sources == 13);
    for (unsigned i = 0; i < 3; ++i)
        ram[0x20008 + i] = 0;
    struct hpclang_rpl_execution_subtraction fact = {
        .left = {.value = 0xabcde}, .right = {.value = 0}, .nibbles = 5, .value = 0xabcde};
    for (unsigned i = 0; i < 5; ++i) {
        fact.left.origins[i] = 0x90001 + i;
        fact.right.origins[i] = 0x90011 + i;
    }
    uint32_t tokens[5];
    assert(!execution_derive(&probe, 0x2900, &fact, tokens));
    assert(!execution_derive(&probe, 0x2a19d, &fact, tokens));
    store.instruction = 0x2a1c1;
    store.address.value = 0x80030;
    store.nibbles = 4;
    store.value = 0xabcdf;
    store.adjustment = 1;
    memcpy(store.origins, tokens, sizeof tokens);
    memcpy(before, ram, sizeof ram);
    return store;
}

static void multiple_stores(void)
{
    struct hpclang_rpl_code_store store = payload_fixture();
    assert(!execution_store(&probe, &store));
    assert(probe.stores == 3 && probe.stored_sources == 17);
    for (unsigned i = 0; i < 4; ++i) {
        uint8_t value;
        uint32_t token;
        assert(!execution_shadow_value(&probe, 0xa0030 + i, &value, &token));
        assert(value == ((store.value >> (4 * i)) & 15));
        assert(token == 0x500021 + i);
        struct hpclang_rpl_code_origin_cell cell;
        assert(execution_source(&probe, token, &cell) < 0);
    }
    store.instruction = 0x2a1e0;
    store.address.value = 0x80040;
    store.nibbles = 5;
    store.value = 0xabcde;
    store.adjustment = 0;
    for (unsigned i = 0; i < 5; ++i)
        store.origins[i] = 0x90001 + i;
    assert(!execution_store(&probe, &store));
    assert(probe.stores == 4 && probe.stored_sources == 22);
    assert(execution_store(&probe, &store) < 0);
    assert(!memcmp(before, ram, sizeof ram));
    const struct hpclang_rpl_reference_reader guards = {execution_guard_read, &probe};
    assert(!hpclang_rpl_reference_guards_validate(&guards, probe.guards, probe.builder.count));

    for (unsigned mode = 0; mode < 6; ++mode) {
        store = payload_fixture();
        if (mode == 0)
            store.origins[4] = 0;
        if (mode == 1)
            store.value ^= 1;
        if (mode == 2)
            store.adjustment = 2;
        if (mode == 3)
            probe.payload_expression.decimal = 1;
        if (mode == 4)
            store.address.value = 0xa0002; /* Physical alias overlap. */
        if (mode == 5) {
            struct hpclang_rpl_reference_edit conflict = probe.edits[0];
            conflict.value ^= 1;
            probe.edits[0] = conflict; /* Subtraction's fixed operand conflicts. */
        }
        unsigned count = probe.stored_sources;
        unsigned char saved[sizeof probe.shadow];
        memcpy(saved, probe.shadow, sizeof saved);
        assert(execution_store(&probe, &store) < 0);
        assert(probe.stores == 2 && probe.stored_sources == count);
        assert(!memcmp(saved, probe.shadow, sizeof saved));
        assert(!memcmp(before, ram, sizeof ram));
    }
}

struct body_test_memory
{
    uint8_t data[32];
    unsigned writes;
};
static int body_test_read(void *context, uint32_t at, uint8_t *out)
{
    struct body_test_memory *m = context;
    if (at >= sizeof m->data)
        return 0;
    *out = m->data[at];
    return 1;
}
static int body_test_write(void *context, uint32_t at, uint8_t value)
{
    struct body_test_memory *m = context;
    if (at >= sizeof m->data)
        return 0;
    m->data[at] = value;
    ++m->writes;
    return 1;
}
static void protected_body(void)
{
    struct body_test_memory memory = {{0}, 0};
    struct hpclang_rpl_mutation_io io = {
        .read_nibble = body_test_read, .write_nibble = body_test_write, .context = &memory};
    struct body_guard guard = {&io, 8, 24, 0, 0};
    for (unsigned at = 0; at < 32; ++at)
        memory.data[at] = at & 15;
    for (unsigned at = 0; at < 32; ++at) {
        uint8_t value = memory.data[at];
        assert(body_guard_write(&guard, at, value));
        unsigned before = memory.writes;
        int result = body_guard_write(&guard, at, value ^ 1);
        if (at >= 8 && at < 24) {
            assert(!result && memory.data[at] == value && memory.writes == before);
        } else
            assert(result && memory.data[at] == (value ^ 1));
    }
    assert(guard.rejected == 16 && guard.writes == 48);
    guard.first = 32;
    guard.end = 33;
    assert(!body_guard_write(&guard, 32, 0) && guard.rejected == 17);
}

int main(void)
{
    protected_body();
    multiple_stores();
    struct hpclang_rpl_code_store store = fixture();
    assert(!execution_store(&probe, &store));
    assert(probe.stores == 1 && probe.stored_sources == 5);
    for (unsigned page = 0; page < 2; ++page)
        for (unsigned digit = 0; digit < 5; ++digit) {
            uint32_t at = (page ? 0xa0002 : 0x80002) + digit, token = 0;
            uint8_t value = 99, original;
            assert(execution_shadow_read(&probe, at, &value));
            assert(value == ((store.value >> (4 * digit)) & 15));
            assert(!execution_token(&probe, 1, at, value, &token));
            assert(token == store.origins[digit]);
            assert(!execution_collect_observe(&probe, HPCLANG_RPL_EXECUTION_CODE, at, value));
            assert(!execution_collect_observe(&probe, HPCLANG_RPL_EXECUTION_DATA, at, value));
            assert(hpclang_rpl_stock_view_saturn(&probe.view, at, &original) && original == 4);
            token = 0xdeadbeef;
            assert(execution_token(&probe, 1, at, value ^ 1, &token) < 0 && token == 0xdeadbeef);
        }
    const struct hpclang_rpl_reference_reader guards = {execution_guard_read, &probe};
    assert(!hpclang_rpl_reference_guards_validate(&guards, probe.guards, probe.builder.count));
    assert(!memcmp(before, ram, sizeof ram));
    struct hpclang_rpl_code_store saved = probe.store;
    assert(execution_store(&probe, &store) < 0 && !memcmp(&saved, &probe.store, sizeof saved));
    uint32_t guard_count = probe.builder.count;
    word(0xa0 * 4, 0x08018000); /* A stale alias must not reuse its old shadow. */
    uint8_t unread = 99;
    assert(!execution_shadow_read(&probe, 0xa0002, &unread) && unread == 99);
    assert(hpclang_rpl_reference_guards_validate(&guards, probe.guards, guard_count) < 0);

    for (unsigned mode = 0; mode < 9; ++mode) {
        store = fixture();
        if (mode == 0)
            store.adjustment = 1;
        if (mode == 1)
            store.origins[4] = 0;
        if (mode == 2)
            store.address.value = 0xffffc;
        if (mode == 3)
            word(0x80 * 4, 0x08000000); /* Native CPU state. */
        if (mode == 4)
            word(0x80 * 4, 0); /* Immutable ROM. */
        if (mode == 5)
            word(0x404 + 0x80 * 4, 0x08018000); /* Different write alias. */
        if (mode == 6)
            word(0x404 + 0x80 * 4, 0); /* Write-disabled page. */
        if (mode == 7)
            word(0xba0, store.address.value >> 6); /* I/O nibble window. */
        if (mode == 8)
            word(0xbb0, store.address.value >> 12); /* I/O page window. */
        assert(execution_store(&probe, &store) < 0 && !probe.stores);
        for (unsigned i = 0; i < 5; ++i)
            assert(!probe.shadow[i].field && !probe.shadow[i].value && !probe.shadow[i].origin);
    }
    return 0;
}
