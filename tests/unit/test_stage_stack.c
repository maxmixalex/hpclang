#include "stage-stack.h"
#include <assert.h>
#include <string.h>

static uint8_t memory[0x80000];
static uint32_t fault;
static unsigned writes;
static int fail_write;

static void put(uint32_t at, uint32_t value, unsigned width)
{
    assert(at >= 0x80000 && at + width <= 0x100000);
    for (unsigned i = 0; i < width; ++i)
        memory[at - 0x80000 + i] = (value >> (4 * i)) & 15;
}
static int read_word(void *context, uint32_t at, unsigned width, uint32_t *value)
{
    (void)context;
    if (at < 0x80000 || at > 0x100000 - width || (fault >= at && fault - at < width))
        return 0;
    *value = 0;
    for (unsigned i = 0; i < width; ++i)
        *value |= (uint32_t)memory[at - 0x80000 + i] << (4 * i);
    return 1;
}
static int write_word(void *context, uint32_t at, uint32_t value)
{
    (void)context;
    ++writes;
    if (fail_write)
        return 0;
    put(at, value, 5);
    return 1;
}
static uint32_t get(uint32_t at)
{
    uint32_t value;
    assert(read_word(NULL, at, 5, &value));
    return value;
}
static void setup(void)
{
    memset(memory, 0, sizeof memory);
    fault = UINT32_MAX;
    writes = 0;
    fail_write = 0;
    put(0x806f8, 0xf0000, 5);
    put(0xf0000, 0x90000, 5);
    put(0xf0005, 0x92000, 5); /* Caller input beneath the hidden descriptor. */
    put(0x90000, 0x2a4e, 5);
    put(0x90005, 45, 5);
    put(0x90012, 0x08012340, 8);
    put(0x90022, 0x08012340, 8);
    put(0x807ed, 0x12345, 5); /* An unrelated field must remain unchanged. */
}
int main(void)
{
    struct hpclang_stage_stack_io io = {NULL, read_word, write_word};
    struct stage_launcher_bias bias;
    setup();
    assert(hpclang_stage_stack_hide(&io, 0x08012340, &bias) == 1);
    assert(bias.active && bias.object == 0x90000 && get(0x806f8) == 0xf0005);
    assert(get(0xf0005) == 0x92000);
    /* Simulate application push and relocation of the hidden native object. */
    put(0x806f8, 0xf0000, 5);
    put(0xf0000, 0x93000, 5);
    bias.object = 0x90100;
    assert(hpclang_stage_stack_restore(&io, &bias));
    assert(get(0x806f8) == 0xefffb && get(0xefffb) == 0x90100);
    /* Native PrRUN drops exactly the restored cell, retaining output/input. */
    put(0x806f8, get(0x806f8) + 5, 5);
    assert(get(get(0x806f8)) == 0x93000 && get(0xf0005) == 0x92000);
    assert(get(0x807ed) == 0x12345);

    const uint32_t changed[] = {0x90000, 0x90005, 0x90012, 0x90022};
    for (unsigned i = 0; i < 4; ++i) {
        setup();
        memory[changed[i] - 0x80000] ^= 1;
        assert(!hpclang_stage_stack_hide(&io, 0x08012340, &bias));
        assert(!writes && !bias.active && get(0x806f8) == 0xf0000);
    }
    for (uint32_t at = 0x90012; at < 0x9002a; ++at) {
        if (at >= 0x9001a && at < 0x90022)
            continue;
        setup();
        fault = at;
        assert(!hpclang_stage_stack_hide(&io, 0x08012340, &bias) && !writes);
    }
    const uint32_t invalid_top[] = {0x7ffff, 0xffffc, 0xfffff};
    for (unsigned i = 0; i < 3; ++i) {
        setup();
        put(0x806f8, invalid_top[i], 5);
        assert(!hpclang_stage_stack_hide(&io, 0x08012340, &bias) && !writes);
    }
    setup();
    put(0xf0000, 0xfffd0, 5);
    put(0xfffd0, 0x2a4e, 5);
    put(0xfffd5, 45, 5);
    assert(!hpclang_stage_stack_hide(&io, 0x08012340, &bias) && !writes);
    setup();
    assert(!hpclang_stage_stack_hide(&io, 0, &bias));
    assert(!hpclang_stage_stack_hide(&io, 0x08012341, &bias));
    assert(!writes);
    fail_write = 1;
    assert(hpclang_stage_stack_hide(&io, 0x08012340, &bias) == -1 && !bias.active);
    fail_write = 0;
    assert(hpclang_stage_stack_hide(&io, 0x08012340, &bias) == 1);
    fail_write = 1;
    assert(!hpclang_stage_stack_restore(&io, &bias));
    assert(!hpclang_stage_stack_hide(NULL, 0, &bias));
    assert(hpclang_stage_stack_restore(NULL, NULL));

    setup();
    put(0x90005, 21, 5);
    put(0x9000a, 0x08012340, 8);
    put(0x90012, 0x01c00000, 8);
    assert(hpclang_stage_stack_hide(&io, 0x08012340, &bias) == 1);
    assert(hpclang_stage_stack_restore(&io, &bias));
    for (uint32_t at = 0x9000a; at < 0x90012; ++at) {
        fault = at;
        writes = 0;
        assert(!hpclang_stage_stack_hide(&io, 0x08012340, &bias) && !writes);
    }
    fault = UINT32_MAX;
    put(0x9000a, 0x08012344, 8);
    assert(!hpclang_stage_stack_hide(&io, 0x08012340, &bias) && !writes);
    setup();
    put(0x90000, 0x2a2c, 5);
    put(0x90005, 19, 5);
    const uint8_t magic[] = {0x4c, 0xb3, 0x80, 0x41, 0x3e, 0x43, 0x50};
    for (unsigned i = 0; i < sizeof magic; ++i)
        put(0x9000a + 2 * i, magic[i], 2);
    assert(hpclang_stage_stack_hide(&io, 0x08012340, &bias) == 1);
    assert(hpclang_stage_stack_restore(&io, &bias));
    memory[0x90017 - 0x80000] ^= 1;
    writes = 0;
    assert(!hpclang_stage_stack_hide(&io, 0x08012340, &bias) && !writes);
    return 0;
}
