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

static void overlapping_store(void)
{
    struct hpclang_rpl_code_store store = payload_fixture();
    assert(!execution_store(&probe, &store));
    store.instruction = 0x2a1e0;
    store.address.value = 0x80040;
    store.nibbles = 5;
    store.value = 0xabcde;
    store.adjustment = 0;
    for (unsigned i = 0; i < 5; ++i)
        store.origins[i] = 0x90001 + i;
    assert(!execution_store(&probe, &store));
    store.instruction = 0x2da2;
    store.address.value = 0x80050;
    store.value = 0xabce3;
    store.adjustment = 5;
    assert(!execution_store(&probe, &store));
    assert(probe.stored_sources == 27);
    store.instruction = 0xb265;
    store.nibbles = 10;
    store.adjustment = 0;
    store.value = UINT64_C(0xabce3abcde);
    for (unsigned i = 0; i < 5; ++i)
        store.origins[i + 5] = 0x500041 + i;
    unsigned char saved[sizeof probe.shadow];
    memcpy(saved, probe.shadow, sizeof saved);
    store.origins[5] = 0x500051; /* A future producer must not resolve recursively. */
    assert(execution_store(&probe, &store) < 0);
    assert(probe.stores == 5 && probe.stored_sources == 27 && !probe.payload_depth);
    assert(!memcmp(saved, probe.shadow, sizeof saved));
    assert(!hpclang_rpl_reference_edits_begin(&probe.plan, probe.edits, 192));
    store.origins[5] = 0x500041;
    assert(!execution_store(&probe, &store));
    assert(probe.stores == 6 && probe.stored_sources == 32);
    for (unsigned i = 0; i < 10; ++i) {
        uint8_t value;
        uint32_t token;
        assert(!execution_shadow_value(&probe, 0xa0050 + i, &value, &token));
        assert(value == ((store.value >> (4 * i)) & 15) && token == store.origins[i]);
    }
    assert(!memcmp(before, ram, sizeof ram));
}

static void configuration(void)
{
    for (unsigned mode = 0; mode < 4; ++mode) {
        struct hpclang_rpl_code_store store = fixture();
        word(0xb9c, mode == 1 ? 0 : 3);
        memcpy(before, ram, sizeof ram);
        struct hpclang_rpl_code_configuration config = {.instruction = 0xb247,
                                                        .value = (uint32_t)store.value};
        memcpy(config.origins, store.origins, sizeof config.origins);
        assert(!hpclang_rpl_reference_guards_begin(&probe.builder, probe.guards,
                                                   mode == 2 ? 32 : 1536));
        if (mode == 3)
            config.origins[0] = 0;
        int result = execution_config(&probe, &config);
        assert(!memcmp(before, ram, sizeof ram));
        if (mode) {
            assert(result < 0 && !probe.configurations);
            if (mode == 2)
                assert(probe.builder.error && !probe.builder.count);
            continue;
        }
        assert(!result && probe.configurations == 1);
        assert(probe.builder.count > 1200 && probe.builder.count <= 1536);
        assert(probe.native_before[0xb9c] == 3 && probe.native_after[0xb9c] == 35);
        const struct hpclang_rpl_reference_reader guards = {execution_guard_read, &probe};
        assert(!hpclang_rpl_reference_guards_validate(&guards, probe.guards, probe.builder.count));
        ram[0x123] ^= 1;
        assert(hpclang_rpl_reference_guards_validate(&guards, probe.guards, probe.builder.count) <
               0);
        assert(execution_config(&probe, &config) < 0); /* No unmodelled second CONFIG. */
        ram[0x123] ^= 1;
        ram[0x20008] = 0x24;
        ram[0x20009] = 0xf0;
        ram[0x2000a] = 0x47;
        config.instruction = 0xb2d1;
        config.value = 0x7f024;
        for (unsigned i = 0; i < 5; ++i)
            config.origins[i] = 0x90011 + i;
        memcpy(before, ram, sizeof ram);
        assert(!execution_config(&probe, &config) && probe.configurations == 2);
        assert(!memcmp(before, ram, sizeof ram));
        assert(probe.native_before[0xb9c] == 35 && probe.native_after[0xb9c] == 7);
        assert(probe.native_after[0xbb0] == 0x7f && !probe.native_after[0xbb1]);
        uint8_t value = 17;
        uint32_t token = 123;
        struct hpclang_rpl_code_origin_cell cell;
        assert(execution_shadow_value(&probe, 0x7f000, &value, &token) ==
               HPCLANG_RPL_SPAN_UNSUPPORTED);
        assert(value == 17 && token == 123);
        assert(execution_write_cell(&probe, 0x7ffff, &cell) == HPCLANG_RPL_SPAN_UNSUPPORTED);
        assert(!hpclang_rpl_reference_guards_validate(&guards, probe.guards, probe.builder.count));
        assert(execution_config(&probe, &config) < 0);
    }
}

static void constant_source(void)
{
    fixture();
    ram[0x20018] = 0x8a;
    ram[0x20019] = 0x42; /* C.P=0 at logical 0x90030. */
    ram[0x20008] = 0x47;
    struct hpclang_rpl_code_constant fact = {.assignment = {.instruction = 0x90030,
                                                            .length = 3,
                                                            .source = HPCLANG_RPL_CODE_CONSTANT,
                                                            .destination = HPCLANG_RPL_CODE_C,
                                                            .destination_first = 7,
                                                            .nibbles = 1},
                                             .p = 7,
                                             .p_origin = 0x90011};
    uint32_t tokens[16];
    assert(!execution_constant(&probe, &fact, tokens) && tokens[0] == 0x600001);
    assert(!execution_payload(&probe, &probe.plan, tokens[0], 0, 0));
    assert(probe.constant_limit == 1);
    assert(execution_payload(&probe, &probe.plan, 0x600011, 0, 0) < 0);
    probe.constants[0].p_origin = tokens[0]; /* A cyclic producer cannot certify P. */
    assert(execution_payload(&probe, &probe.plan, tokens[0], 0, 0) < 0);
    assert(probe.constant_limit == 1);
}

static void unit_source(void)
{
    fixture();
    ram[0x20018] = 0x0b;
    ram[0x20019] = 0x46; /* C=C+1.P at logical 0x90030. */
    ram[0x20008] = 0x40;
    struct hpclang_rpl_code_unit fact = {.instruction = 0x90030,
                                         .length = 3,
                                         .reg = HPCLANG_RPL_CODE_C,
                                         .nibbles = 1,
                                         .before = 4,
                                         .after = 5,
                                         .p_origin = 0x90011};
    fact.origins[0] = 0x90012;
    uint32_t tokens[16];
    assert(!execution_unit(&probe, &fact, tokens) && tokens[0] == 0x700001);
    assert(!execution_payload(&probe, &probe.plan, tokens[0], 5, 5));
    assert(probe.unit_limit == 1);
    assert(execution_payload(&probe, &probe.plan, tokens[0], 5, 6) < 0);
    assert(execution_payload(&probe, &probe.plan, tokens[0] + 16, 5, 5) < 0);
    probe.units[0].origins[0] = tokens[0];
    assert(execution_payload(&probe, &probe.plan, tokens[0], 5, 5) < 0);
    assert(probe.unit_limit == 1 && probe.plan.error < 0 && !probe.plan.count);
}

static void sum_source(void)
{
    fixture();
    ram[0x20018] = 0x0a;
    ram[0x20019] = 0x42;
    ram[0x20020] = 0x0a;
    ram[0x20021] = 0x46;
    ram[0x20008] = 0x40;
    const struct hpclang_rpl_reader reader = {execution_plan_read, &probe};
    struct hpclang_rpl_code_values values = {0};
    values.registers[0] = values.registers[2] = 4;
    struct hpclang_rpl_code_sum_source fact = {.p_origin = 0x90011};
    assert(!hpclang_rpl_code_sum_inspect(&reader, 0x90030, 3, &values, &fact.sum));
    fact.origins[0][0] = fact.origins[1][0] = 0x90012;
    uint32_t tokens[16];
    assert(!execution_sum(&probe, &fact, tokens) && tokens[0] == 0x800001);
    values.registers[2] = 8;
    assert(!hpclang_rpl_code_sum_inspect(&reader, 0x90040, 3, &values, &fact.sum));
    fact.origins[0][0] = fact.origins[1][0] = tokens[0];
    assert(!execution_sum(&probe, &fact, tokens) && tokens[0] == 0x800011);
    assert(!execution_payload(&probe, &probe.plan, tokens[0], 0, 0));
    assert(probe.sum_limit == 2);
    assert(execution_payload(&probe, &probe.plan, tokens[0], 0, 1) < 0);
    assert(execution_payload(&probe, &probe.plan, 0x800021, 0, 0) < 0);
    probe.sums[0].origins[0][0] = tokens[0];
    assert(execution_payload(&probe, &probe.plan, tokens[0], 0, 0) < 0);
    assert(probe.sum_limit == 2 && probe.plan.error < 0 && !probe.plan.count);
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
    constant_source();
    unit_source();
    sum_source();
    overlapping_store();
    configuration();
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
