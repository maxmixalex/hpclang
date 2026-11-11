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

static void later_stores(void)
{
    overlapping_store();
    struct hpclang_rpl_code_store store = {
        .instruction = 0xb2ce, .nibbles = 5, .address = {.value = 0x80070}, .value = 0xabcde};
    for (unsigned i = 0; i < 5; ++i)
        store.origins[i] = 0x90001 + i;
    assert(!execution_store(&probe, &store));
    store.instruction = 0x2da2;
    store.value += 10;
    store.adjustment = 10;
    for (unsigned i = 0; i < 2; ++i) {
        store.address.value += 0x10;
        assert(!execution_store(&probe, &store));
        for (unsigned digit = 0; digit < 5; ++digit) {
            uint8_t value = 255;
            uint32_t token = 0;
            assert(!execution_shadow_value(&probe, store.address.value + digit, &value, &token));
            assert(value == ((store.value >> (4 * digit)) & 15));
            assert(token == 0x500001 + 16 * (probe.stores - 1) + digit);
        }
    }
    store.instruction = 0x20476;
    store.address.value = 0x800a0;
    store.value = 0xabcde;
    store.adjustment = 0;
    assert(!execution_store(&probe, &store));
    store.instruction = 0x20482;
    store.address.value = 0x800b0;
    store.nibbles = 1;
    store.first = 15;
    store.value = UINT64_C(4) << 60;
    memset(store.origins, 0, sizeof store.origins);
    store.origins[15] = 0x90006;
    struct hpclang_rpl_code_store invalid = store;
    invalid.first = 14;
    assert(execution_store(&probe, &invalid) < 0);
    assert(probe.stores == 10 && probe.stored_sources == 52);
    assert(!execution_store(&probe, &store));
    uint8_t value = 255;
    uint32_t token = 0;
    assert(!execution_shadow_value(&probe, store.address.value, &value, &token));
    assert(value == 4 && token == 0x90006);
    store.instruction = 0x153eb;
    store.address.value = 0x800c0;
    store.first = 0;
    store.nibbles = 5;
    store.value = 0xabcde;
    for (unsigned i = 0; i < 5; ++i)
        store.origins[i] = 0x90001 + i;
    assert(!execution_store(&probe, &store));
    store.instruction = 0x92f0;
    store.address.value = 0x800d0;
    store.first = 15;
    store.nibbles = 1;
    store.value = UINT64_C(4) << 60;
    assert(!execution_store(&probe, &store));
    assert(probe.stores == 13 && probe.stored_sources == 59);
    assert(!execution_shadow_value(&probe, store.address.value, &value, &token));
    assert(value == 4 && token == 0x90006);
    assert(execution_store(&probe, &store) < 0);
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
                                                   mode == 2 ? 32 : 1152));
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
        assert(probe.builder.count >= 792 && probe.builder.count <= 1152);
        assert(ram[0xb9c] == 3 && probe.native_after[0xb9c] == 35);
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
        assert(ram[0xb9c] == 3 && probe.native_after[0xb9c] == 7);
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

static void boundary_address(void)
{
    fixture();
    probe.move_first = 0x86120;
    probe.move_end = 0x88dad;
    struct hpclang_rpl_code_address_use use = {.value = probe.move_end};
    uint32_t replacement = 123;
    assert(!execution_address_replacement(&probe, &use, &replacement));
    assert(replacement == probe.move_end); /* Numeric equality is insufficient. */
    for (unsigned digit = 0; digit < 5; ++digit)
        use.origins[digit] = 0x806f4 + digit;
    const uint32_t adjustments[] = {0, 5, 0xffffb, 0xffff6, 0xfffec};
    for (unsigned i = 0; i < sizeof adjustments / sizeof adjustments[0]; ++i) {
        use.adjustment = adjustments[i];
        use.value = (probe.move_end + use.adjustment) & 0xfffff;
        assert(!execution_address_replacement(&probe, &use, &replacement));
        assert(replacement == ((probe.move_end + 16 + use.adjustment) & 0xfffff));
    }
    use.adjustment = 0;
    use.value = probe.move_end;
    for (unsigned digit = 0; digit < 5; ++digit) {
        uint32_t saved = use.origins[digit];
        use.origins[digit] = 0;
        assert(!execution_address_replacement(&probe, &use, &replacement));
        assert(replacement == probe.move_end);
        use.origins[digit] = saved;
    }
    for (unsigned mode = 0; mode < 4; ++mode) {
        struct hpclang_rpl_code_address_use invalid = use;
        replacement = 123;
        if (mode == 0)
            invalid.value++;
        if (mode == 1)
            invalid.generated = 1;
        if (mode == 2)
            invalid.adjustment = 0x100000;
        if (mode == 3)
            probe.move_end = invalid.value = 0xffff1;
        assert(execution_address_replacement(&probe, &invalid, &replacement) < 0);
        assert(replacement == 123);
    }
}

static void unconfiguration(void)
{
    for (unsigned mode = 0; mode < 8; ++mode) {
        struct hpclang_rpl_code_store store = fixture();
        assert(!hpclang_rpl_reference_guards_begin(&probe.builder, probe.guards, 1152));
        word(0xb9c, 3);
        word(0xbcc, 1);
        word(0xbd0, 2);
        word(0x7f * 4, 0x08010000);
        word(0x404 + 0x7f * 4, 0x08010000);
        struct hpclang_rpl_code_configuration config = {.instruction = 0xb247,
                                                        .value = (uint32_t)store.value};
        memcpy(config.origins, store.origins, sizeof config.origins);
        assert(!execution_config(&probe, &config));
        ram[0x20008] = 0x24;
        ram[0x20009] = 0xf0;
        ram[0x2000a] = 0x47;
        config.instruction = 0xb2d1;
        config.value = 0x7f024;
        for (unsigned i = 0; i < 5; ++i)
            config.origins[i] = 0x90011 + i;
        assert(!execution_config(&probe, &config));
        struct hpclang_rpl_code_store bank = {
            .instruction = 0xb2d7, .nibbles = 2, .address = {.value = config.value}};
        memcpy(bank.address.origins, config.origins, sizeof config.origins);
        assert(!execution_bank_write(&probe, &bank));
        config.instruction = 0xb2da;
        if (mode == 1)
            config.instruction++;
        if (mode == 2)
            probe.bank_writes = 0;
        if (mode == 3)
            probe.configurations = 1;
        if (mode == 4)
            config.origins[0] = 0;
        if (mode == 5)
            config.value = 0x80000;
        if (mode == 6) {
            /* RAM takes precedence and requires an unqualified map rebuild. */
            memset(probe.native_after + 0xba8, 0, 4);
            probe.native_after[0xba8] = 0x7f;
        }
        if (mode == 7)
            probe.native_after[0xb9c] = 3;
        uint8_t simulated[sizeof probe.native_after];
        memcpy(simulated, probe.native_after, sizeof simulated);
        memcpy(before, ram, sizeof ram);
        int result = execution_unconfig(&probe, &config);
        assert(!memcmp(before, ram, sizeof ram));
        if (mode) {
            assert(result < 0 && !probe.unconfigurations);
            assert(!memcmp(simulated, probe.native_after, sizeof simulated));
            continue;
        }
        assert(!result && probe.unconfigurations == 1);
        simulated[0xb9c] = 3;
        memset(simulated + 0xbb0, 255, 4);
        assert(!memcmp(simulated, probe.native_after, sizeof simulated));
        uint8_t value;
        uint32_t token;
        struct hpclang_rpl_code_origin_cell cell;
        assert(!execution_shadow_value(&probe, 0x7f000, &value, &token));
        assert(!execution_write_cell(&probe, 0x7f000, &cell));
        const struct hpclang_rpl_reference_reader guards = {execution_guard_read, &probe};
        assert(!hpclang_rpl_reference_guards_validate(&guards, probe.guards, probe.builder.count));
        ram[0xbb0] ^= 1;
        assert(hpclang_rpl_reference_guards_validate(&guards, probe.guards, probe.builder.count) <
               0);
        assert(execution_unconfig(&probe, &config) < 0);
    }
}

static void reconfiguration_cycle(void)
{
    for (unsigned mode = 0; mode < 9; ++mode) {
        struct hpclang_rpl_code_store source = fixture();
        word(0xb9c, 3);
        word(0xbcc, 1);
        word(0xbd0, 2);
        word(0x7f * 4, 0x08010000);
        word(0x404 + 0x7f * 4, 0x08010000);
        word(0x20008, 0x7f024);
        assert(!hpclang_rpl_reference_guards_begin(&probe.builder, probe.guards, 1152));
        struct hpclang_rpl_code_configuration config = {.instruction = 0xb247,
                                                        .value = (uint32_t)source.value};
        memcpy(config.origins, source.origins, sizeof config.origins);
        assert(!execution_config(&probe, &config));
        config.instruction = 0xb2d1;
        config.value = 0x7f024;
        for (unsigned i = 0; i < 5; ++i)
            config.origins[i] = 0x90011 + i;
        assert(!execution_config(&probe, &config));
        struct hpclang_rpl_code_store bank = {
            .instruction = 0xb2d7, .nibbles = 2, .address = {.value = config.value}};
        memcpy(bank.address.origins, config.origins, sizeof config.origins);
        assert(!execution_bank_write(&probe, &bank));
        config.instruction = 0xb2da;
        assert(!execution_unconfig(&probe, &config));
        probe.stores = 15;
        config.instruction = 0xb371;
        if (mode == 1)
            probe.unconfigurations = 0;
        if (mode == 2)
            probe.stores = 14;
        if (mode == 3)
            probe.bank_writes = 0;
        if (mode == 4)
            config.instruction++;
        if (mode == 5)
            config.origins[0] = 0;
        if (mode == 6)
            probe.native_after[0x123] ^= 1;
        if (mode == 7)
            probe.native_after[0xb9c] = 7;
        if (mode == 8)
            probe.builder.capacity = 0;
        memcpy(before, ram, sizeof ram);
        int result = execution_config(&probe, &config);
        assert(!memcmp(before, ram, sizeof ram));
        if (mode) {
            assert(result < 0 && probe.configurations == 2);
            continue;
        }
        assert(!result && probe.configurations == 3 && probe.native_after[0xb9c] == 35);
        config.instruction = 0xb376;
        assert(!execution_config(&probe, &config) && probe.configurations == 4);
        uint8_t value;
        uint32_t token;
        struct hpclang_rpl_code_origin_cell cell;
        assert(execution_shadow_value(&probe, 0x7f000, &value, &token) < 0);
        assert(execution_write_cell(&probe, 0x7f000, &cell) < 0);
        bank.instruction = 0xb37c;
        assert(!execution_bank_write(&probe, &bank) && probe.bank_writes == 2);
        assert(execution_bank_write(&probe, &bank) < 0);
        config.instruction = 0xb37f;
        assert(!execution_unconfig(&probe, &config) && probe.unconfigurations == 2);
        assert(!memcmp(probe.native_after, ram, sizeof probe.native_after));
        assert(!execution_shadow_value(&probe, 0x7f000, &value, &token));
        assert(!execution_write_cell(&probe, 0x7f000, &cell));
        assert(execution_unconfig(&probe, &config) < 0);
        assert(execution_config(&probe, &config) < 0);
        assert(!memcmp(before, ram, sizeof ram));
    }
}

static void bank_write(void)
{
    for (unsigned mode = 0; mode < 6; ++mode) {
        fixture();
        word(0xbcc, 1);
        word(0xbd0, 2);
        ram[0x20008] = 0x24;
        ram[0x20009] = 0xf0;
        ram[0x2000a] = 0x47;
        memcpy(probe.native_after, ram, sizeof probe.native_after);
        memset(probe.native_after + 0xbb0, 0, 4);
        probe.native_after[0xbb0] = 0x7f;
        probe.configurations = 2;
        struct hpclang_rpl_code_store effect = {.instruction = 0xb2d7,
                                                .nibbles = 2,
                                                .address = {.value = 0x7f024},
                                                .value = UINT64_MAX};
        for (unsigned i = 0; i < 5; ++i)
            effect.address.origins[i] = 0x90011 + i;
        if (mode == 1)
            probe.native_after[0xbcc] ^= 1;
        if (mode == 2)
            effect.address.origins[0] = 0;
        if (mode == 3) {
            probe.move_first = effect.address.value;
            probe.move_end = effect.address.value + 1;
        }
        if (mode == 4)
            probe.native_after[0xbb0] = 0x80;
        if (mode == 5)
            effect.instruction++;
        memcpy(before, ram, sizeof ram);
        int result = execution_bank_write(&probe, &effect);
        assert(!memcmp(before, ram, sizeof ram) && !probe.stores && !probe.stored_sources);
        if (mode) {
            assert(result < 0 && !probe.bank_writes);
            continue;
        }
        assert(!result && probe.bank_writes == 1);
        const struct hpclang_rpl_reference_reader guards = {execution_guard_read, &probe};
        assert(!hpclang_rpl_reference_guards_validate(&guards, probe.guards, probe.builder.count));
        ram[0xbd0] ^= 1;
        assert(hpclang_rpl_reference_guards_validate(&guards, probe.guards, probe.builder.count) <
               0);
        assert(execution_bank_write(&probe, &effect) < 0);
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
    uint32_t tokens[16] = {0};
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
    uint32_t tokens[16] = {0};
    assert(!execution_unit(&probe, &fact, tokens) && tokens[0] == 0x700001);
    assert(!execution_payload(&probe, &probe.plan, tokens[0], 5, 5));
    assert(probe.unit_limit == 1);
    assert(execution_payload(&probe, &probe.plan, tokens[0], 5, 6) < 0);
    assert(execution_payload(&probe, &probe.plan, tokens[0] + 16, 5, 5) < 0);
    probe.units[0].origins[0] = tokens[0];
    assert(execution_payload(&probe, &probe.plan, tokens[0], 5, 5) < 0);
    assert(probe.unit_limit == 1 && probe.plan.error < 0 && !probe.plan.count);
}

static void dispatch_root_relations(void)
{
    static const unsigned chain[] = {1, 3, 5, 10, 12};
    static const int offsets[] = {1, 2, 2, -1, 0};
    static const uint32_t adjustments[] = {1, 0, 0xfffff, 0xffffc, 0};
    for (unsigned mode = 0; mode < 15; ++mode)
        for (unsigned link = 0; link < 5; ++link) {
            fixture();
            uint32_t rstk = 0x90080, available = 100;
            const uint32_t anchors[] = {0x806f3, 0x80e9b};
            const uint32_t values[] = {rstk, available + (mode == 5)};
            for (unsigned a = 0; a < 2; ++a)
                for (unsigned i = 0; i < 5; ++i) {
                    unsigned at = (anchors[a] + i) & 4095, shift = 4 * (at & 1);
                    ram[0x10000 + at / 2] = (uint8_t)((ram[0x10000 + at / 2] & ~(15u << shift)) |
                                                      (((values[a] >> (4 * i)) & 15) << shift));
                }
            probe.state.control.pc = 0x3130;
            probe.move_end = rstk;
            probe.unit_count = 3;
            probe.native_word_count = 13;
            struct hpclang_rpl_code_unit *unit = &probe.units[2];
            *unit = (struct hpclang_rpl_code_unit){.instruction = 0xb347,
                                                   .length = 6,
                                                   .reg = HPCLANG_RPL_CODE_B,
                                                   .nibbles = 5,
                                                   .decrement = 1,
                                                   .before = rstk - 5,
                                                   .after = rstk - 10,
                                                   .adjustment = 0xffffb};
            probe.unit_inputs[2] = unit->before + 16;
            probe.unit_outputs[2] = unit->after + 16;
            probe.state.values.registers[HPCLANG_RPL_CODE_B] = unit->after;
            probe.state.values.registers[HPCLANG_RPL_CODE_D] = available + 2;
            probe.state.origins.adjustment[HPCLANG_RPL_CODE_D] = 1;
            for (unsigned digit = 0; digit < 5; ++digit) {
                unit->origins[digit] = 0x806f4 + digit;
                probe.state.origins.registers[HPCLANG_RPL_CODE_B][digit] = 0x700021 + digit;
                probe.state.origins.registers[HPCLANG_RPL_CODE_D][digit] = 0xa000c1 + digit;
            }
            for (unsigned i = 0; i < 5; ++i) {
                struct hpclang_rpl_code_native_word *word = &probe.native_words[chain[i]];
                *word = (struct hpclang_rpl_code_native_word){
                    .instruction = 0x3130,
                    .reg = HPCLANG_RPL_CODE_D,
                    .before = (uint32_t)((int)available + offsets[i]),
                    .adjustment = adjustments[i],
                    .delta = 1};
                word->after = word->before + 1;
                for (unsigned digit = 0; digit < 5; ++digit)
                    word->origins[digit] = (i ? 0xa00001 + 16 * chain[i - 1] : 0x80e9c) + digit;
            }
            struct hpclang_rpl_code_native_word *word = &probe.native_words[chain[link]];
            if (mode == 1)
                unit->origins[0]++;
            if (mode == 2)
                probe.native_words[1].origins[0]++;
            if (mode == 3)
                word->origins[0] = 0xa00001 + 16 * chain[link];
            if (mode == 4)
                word->origins[1] = 0;
            if (mode == 6)
                probe.state.values.registers[HPCLANG_RPL_CODE_D]++;
            if (mode == 7)
                word->adjustment ^= 1;
            if (mode == 8)
                probe.unit_inputs[2]++;
            if (mode == 9)
                probe.state.control.pc++;
            if (mode == 10)
                probe.move_end--;
            if (mode == 11)
                word->delta = 2;
            if (mode == 12)
                unit->decrement = 2;
            if (mode == 13)
                word->reg = HPCLANG_RPL_CODE_B;
            if (mode == 14)
                probe.state.values.registers[HPCLANG_RPL_CODE_B]--;
            memcpy(before, ram, sizeof ram);
            int result = execution_dispatch_roots(&probe);
            assert(mode ? result < 0 : result == 0);
            assert(!memcmp(before, ram, sizeof ram));
        }
}

static void late_overlapping_stores(void)
{
    for (unsigned effect = 13; effect <= 15; ++effect)
        for (unsigned mode = 0; mode < 9; ++mode) {
            struct hpclang_rpl_code_store store = fixture();
            store.instruction = effect == 13 ? 0xb35d : effect == 14 ? 0xb367 : 0x291c;
            unsigned previous = effect == 13 ? 5 : effect == 14 ? 6 : 3;
            struct hpclang_rpl_code_store old = store;
            old.value = 0x12345;
            word(0x20010, (uint32_t)old.value);
            for (unsigned i = 0; i < 5; ++i)
                old.origins[i] = 0x90021 + i;
            assert(!hpclang_rpl_execution_plan_payload(
                &probe.plan, &old, old.value, execution_source, execution_payload, &probe));
            probe.store_records[previous] = old;
            probe.stores = effect;
            probe.stored_sources = 75;
            probe.native_save_count = 2;
            for (unsigned i = 0; i < 75; ++i)
                probe.shadow[i].field = 0x70000 + i;
            for (unsigned i = 0; i < 5; ++i) {
                struct hpclang_rpl_code_origin_cell cell;
                assert(!execution_write_cell(&probe, store.address.value + i, &cell));
                probe.shadow[i].field = cell.field;
                probe.shadow[i].value = (old.value >> (4 * i)) & 15;
                probe.shadow[i].origin = old.origins[i];
            }
            if (mode == 1)
                ++store.address.value;
            if (mode == 2)
                ++probe.shadow[0].origin;
            if (mode == 3)
                probe.shadow[0].value ^= 1;
            if (mode == 4)
                ++probe.shadow[0].field;
            if (mode == 5)
                probe.native_save_count = 1;
            if (mode == 6)
                probe.stored_sources = 74;
            if (mode == 7)
                store.origins[0] = 0;
            if (mode == 8)
                ++store.instruction;
            uint8_t saved[sizeof probe.shadow];
            memcpy(saved, probe.shadow, sizeof saved);
            memcpy(before, ram, sizeof ram);
            int result = execution_store(&probe, &store);
            assert(!memcmp(before, ram, sizeof ram));
            if (mode) {
                assert(result < 0 && probe.stores == effect &&
                       !memcmp(saved, probe.shadow, sizeof saved));
                continue;
            }
            assert(!result && probe.stores == effect + 1 && probe.stored_sources == 75);
            assert(!memcmp(&probe.store_records[previous], &old, sizeof old));
            for (unsigned i = 0; i < 5; ++i) {
                assert(probe.shadow[i].value == ((store.value >> (4 * i)) & 15));
                assert(probe.shadow[i].origin == store.origins[i]);
            }
            assert(!memcmp(saved + 5 * sizeof probe.shadow[0], probe.shadow + 5,
                           sizeof saved - 5 * sizeof probe.shadow[0]));
            const struct hpclang_rpl_reference_reader reader = {execution_guard_read, &probe};
            assert(
                !hpclang_rpl_reference_guards_validate(&reader, probe.guards, probe.builder.count));
            ram[0x20010] ^= 1;
            assert(hpclang_rpl_reference_guards_validate(&reader, probe.guards,
                                                         probe.builder.count) < 0);
        }
}

static void immediate_unit_source(void)
{
    for (unsigned mode = 0; mode < 8; ++mode) {
        fixture();
        word(11 * 4, 0x08030000);
        const uint8_t code[] = {8, 1, 8, 15, 9, 4};
        for (unsigned i = 0; i < 6; ++i) {
            unsigned at = 0x347 + i, shift = 4 * (at & 1);
            ram[0x30000 + at / 2] =
                (uint8_t)((ram[0x30000 + at / 2] & ~(15u << shift)) | (code[i] << shift));
        }
        word(0x20000, 0x90040);
        assert(!hpclang_rpl_reference_guards_begin(&probe.builder, probe.guards, 512));
        assert(!hpclang_rpl_reference_edits_begin(&probe.plan, probe.edits, 128));
        probe.move_first = 0x90030;
        probe.move_end = 0x90080;
        struct hpclang_rpl_code_unit fact = {.instruction = 0xb347,
                                             .length = 6,
                                             .reg = HPCLANG_RPL_CODE_B,
                                             .nibbles = 5,
                                             .before = 0x90040,
                                             .after = 0x9003b,
                                             .decrement = 1};
        for (unsigned i = 0; i < 5; ++i)
            fact.origins[i] = 0x90001 + i;
        if (mode == 1)
            fact.origins[0] = 0;
        if (mode == 2)
            fact.instruction++;
        if (mode == 3)
            fact.reg = HPCLANG_RPL_CODE_C;
        if (mode == 4)
            fact.first = 1;
        if (mode == 5)
            fact.carry = 1;
        if (mode == 6)
            probe.unit_count = 3;
        if (mode == 7)
            probe.plan.capacity = 0;
        memcpy(before, ram, sizeof ram);
        uint32_t tokens[16] = {0};
        int result = execution_unit(&probe, &fact, tokens);
        assert(!memcmp(before, ram, sizeof ram));
        if (mode) {
            assert(result < 0 && probe.unit_count == (mode == 6 ? 3u : 0u));
            continue;
        }
        assert(!result && probe.unit_count == 1 && probe.unit_inputs[0] == 0x90050 &&
               probe.unit_outputs[0] == 0x9004b);
        for (unsigned i = 0; i < 5; ++i)
            assert(!execution_payload(&probe, &probe.plan, tokens[i], (0x9003b >> (4 * i)) & 15,
                                      (0x9004b >> (4 * i)) & 15));
        assert(execution_payload(&probe, &probe.plan, tokens[1], 3, 3) < 0);
        probe.units[0].origins[0] = tokens[0];
        assert(execution_payload(&probe, &probe.plan, tokens[0], 11, 11) < 0);
        assert(probe.unit_limit == 1 && !memcmp(before, ram, sizeof ram));
    }
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
    uint32_t tokens[16] = {0};
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

static void native_word_payload(void)
{
    for (unsigned mode = 0; mode < 6; ++mode) {
        (void)fixture();
        word(3 * 4, 0x08020000);
        ram[0x20098] = 0x18;
        ram[0x20099] = 0xdb;
        struct hpclang_rpl_code_native_word fact = {.instruction = 0x3130,
                                                    .reg = HPCLANG_RPL_CODE_B,
                                                    .before = 0x444abcde,
                                                    .after = 0x444abcd9,
                                                    .delta = -5};
        for (unsigned i = 0; i < 8; ++i)
            fact.origins[i] = 0x90001 + i;
        if (mode == 2)
            fact.origins[0] = 0;
        uint32_t tokens[8] = {0};
        assert(!execution_native_word(&probe, &fact, tokens));
        assert(probe.native_word_count == 1 && tokens[5] == 0xa00006);
        if (mode == 1)
            ram[0x20003] ^= 0x10;
        if (mode == 3)
            ram[0x20099] = 0xeb;
        if (mode == 5)
            probe.native_word_limit = 0;
        memcpy(before, ram, sizeof ram);
        int result = execution_payload(&probe, &probe.plan, tokens[5], 4, mode == 4 ? 5 : 4);
        assert(!memcmp(before, ram, sizeof ram));
        if (!mode)
            assert(!result && probe.plan.count == 12);
        else
            assert(result < 0);
    }
}

static void native_scan_saves(void)
{
    for (unsigned mode = 0; mode < 12; ++mode) {
        (void)fixture();
        word(3 * 4, 0x08020000);
        word(0xa1 * 4, 0x08000800);
        ram[0x2000e] = 0x18;
        ram[0x2000f] = 0xab;
        ram[0x20000] = 0;
        ram[0x20001] = 0;
        ram[0x20002] = 8; /* read5(D0) == 0x80000. */
        ram[0x10000] = ram[0x10001] = 0x11;
        ram[0x10002] = 1;
        word(0x954, 0x90000);
        word(0x958, 0xabcde);
        word(0x914, 0x444abcde);
        word(0x95c, 0);
        assert(!hpclang_rpl_reference_guards_begin(&probe.builder, probe.guards, 512));
        assert(!hpclang_rpl_reference_edits_begin(&probe.plan, probe.edits, 128));
        struct hpclang_rpl_code_execution state = {0};
        state.control.pc = 0x301c;
        state.control.stack[7] = 0x4ec7;
        const unsigned regs[] = {HPCLANG_RPL_CODE_D0, HPCLANG_RPL_CODE_D1, HPCLANG_RPL_CODE_B};
        const unsigned offsets[] = {0x954, 0x958, 0x914};
        const uint32_t values[] = {0x90000, 0xabcde, 0x444abcde};
        for (unsigned i = 0; i < 3; ++i) {
            state.values.registers[regs[i]] = values[i];
            for (unsigned digit = 0; digit < (i == 2 ? 8u : 5u); ++digit)
                state.origins.registers[regs[i]][digit] = 0x200000 + offsets[i] * 2 + digit;
        }
        state.origins.registers[HPCLANG_RPL_CODE_P][0] = 0x200000 + 0x95c * 2;
        probe.stores = 13;
        probe.stored_sources = 59;
        const struct hpclang_rpl_reader reader = {execution_shadow_read, &probe};
        struct hpclang_rpl_code_native_scan effect;
        assert(!hpclang_rpl_code_native_scan_inspect(&reader, &reader, 4, &state, 2, &effect));
        if (mode == 1)
            state.values.registers[HPCLANG_RPL_CODE_P] = 1;
        if (mode == 2)
            state.status = 2;
        if (mode == 3)
            effect.reads = 3;
        if (mode == 4) {
            ++state.values.registers[HPCLANG_RPL_CODE_D0];
            ++effect.d0;
        }
        if (mode == 5) {
            state.values.registers[HPCLANG_RPL_CODE_D0] = 0xa1960;
            effect.d0 = 0xa1965;
        }
        if (mode == 6)
            section ^= 0x1000;
        if (mode == 7)
            assert(!hpclang_rpl_reference_guards_begin(&probe.builder, probe.guards, 0));
        if (mode == 8)
            state.origins.registers[HPCLANG_RPL_CODE_B][7] = 0;
        if (mode == 9) {
            state.values.registers[HPCLANG_RPL_CODE_D1] = effect.d1 = 0x100000;
            effect.saved_d1 = 0x100000;
        }
        if (mode == 10)
            probe.shadow[0].field = 0xcb0 * 2;
        if (mode == 11)
            probe.view.state_offset = 0x80000 - 0xcb8 + 1;
        memcpy(before, ram, sizeof ram);
        struct hpclang_rpl_code_scan_origins origins = {0};
        int result = execution_scan(&probe, &effect, &state, &origins);
        assert(!memcmp(before, ram, sizeof ram));
        if (mode) {
            assert(result < 0 && !probe.native_save_count && probe.stored_sources == 59);
            continue;
        }
        assert(!result && probe.native_save_count == 2 && probe.scan_reads == 2);
        assert(probe.stored_sources == 75 && probe.uses == 2 && probe.planned_uses == 2);
        for (unsigned word = 0; word < 2; ++word)
            for (unsigned digit = 0; digit < 8; ++digit) {
                uint8_t value;
                uint32_t origin;
                assert(
                    !execution_shadow_value(&probe, 0xa1960 + 8 * word + digit, &value, &origin));
                assert(value == ((values[word ? 2 : 1] >> (4 * digit)) & 15));
                assert(origin == 0x900001 + 16 * word + digit);
                assert(!execution_payload(&probe, &probe.plan, origin, value, value));
            }
        assert(execution_scan(&probe, &effect, &state, &origins) < 0);
        const struct hpclang_rpl_reference_reader guards = {execution_guard_read, &probe};
        assert(!hpclang_rpl_reference_guards_validate(&guards, probe.guards, probe.builder.count));
        ram[0xcb7] ^= 0x10;
        assert(hpclang_rpl_reference_guards_validate(&guards, probe.guards, probe.builder.count) <
               0);
    }
}

int main(void)
{
    native_scan_saves();
    native_word_payload();
    later_stores();
    boundary_address();
    unconfiguration();
    reconfiguration_cycle();
    bank_write();
    constant_source();
    unit_source();
    immediate_unit_source();
    late_overlapping_stores();
    dispatch_root_relations();
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
