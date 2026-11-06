#include "rpl-execution.h"
#include "rpl-stock-memory.h"
#include "launch-services.h"

struct execution_read
{
    const struct hpclang_rpl_execution_io *io;
    const struct hpclang_rpl_reader *reader;
    unsigned space;
    int *error;
};

static int observe(const struct hpclang_rpl_execution_io *io, unsigned space, uint32_t at,
                   uint8_t value)
{
    int result = io->observe(io->context, space, at, value);
    return result > 0 ? HPCLANG_RPL_SPAN_MALFORMED : result;
}

static int read_observed(void *context, uint32_t at, uint8_t *value)
{
    struct execution_read *read = context;
    if (*read->error || !read->reader->read_nibble(read->reader->context, at, value))
        return 0;
    if (*value > 15)
        return 1; /* The evaluator rejects malformed nibbles. */
    *read->error = observe(read->io, read->space, at, *value);
    return !*read->error;
}

static int observe_native(void *context, uint32_t at, uint8_t value)
{
    return observe(context, HPCLANG_RPL_EXECUTION_NATIVE, at, value);
}

int hpclang_rpl_execution_collect_effects(const struct hpclang_boot_info *boot,
                                          const struct hpclang_rpl_execution_io *io,
                                          uint32_t native_origin_base, uint32_t budget,
                                          struct hpclang_rpl_code_execution *state, uint32_t *steps,
                                          hpclang_rpl_code_store_fn store)
{
    if (!io || !io->native.read_nibble || !io->code.read_nibble || !io->data.read_nibble ||
        !io->observe || !io->token || !io->visit || !state || !steps)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    *steps = 0;
    if (!budget)
        return HPCLANG_RPL_SPAN_BOUNDS;
    uint32_t *entry = hpclang_launch_execution_pc(boot);
    if (!entry)
        return HPCLANG_RPL_SPAN_UNSUPPORTED;
    uint32_t pc = *entry;
    for (unsigned digit = 0; digit < 8; ++digit) {
        int result = observe(io, HPCLANG_RPL_EXECUTION_PC, digit, (pc >> (4 * digit)) & 15);
        if (result)
            return result;
    }
    int result = hpclang_rpl_stock_execution_capture(&io->native, pc, native_origin_base, state,
                                                     observe_native, (void *)io);
    if (result)
        return result;
    int error = 0;
    struct execution_read code = {io, &io->code, HPCLANG_RPL_EXECUTION_CODE, &error};
    struct execution_read data = {io, &io->data, HPCLANG_RPL_EXECUTION_DATA, &error};
    const struct hpclang_rpl_reader instructions = {read_observed, &code};
    const struct hpclang_rpl_reader memory = {read_observed, &data};
    while (*steps < budget) {
        struct hpclang_rpl_code_step_uses uses;
        pc = state->control.pc;
        result = hpclang_rpl_code_execution_step_effects(&instructions, &memory, 0x100000u - pc,
                                                         state, io->token, io->derive, store,
                                                         io->context, &uses);
        if (error || result)
            return error ? error : result;
        ++*steps;
        result = io->visit(io->context, pc, state, &uses);
        if (result)
            return result == 1  ? HPCLANG_RPL_EXECUTION_BOUNDARY
                   : result < 0 ? result
                                : HPCLANG_RPL_SPAN_MALFORMED;
    }
    return HPCLANG_RPL_EXECUTION_BUDGET;
}

int hpclang_rpl_execution_collect(const struct hpclang_boot_info *boot,
                                  const struct hpclang_rpl_execution_io *io,
                                  uint32_t native_origin_base, uint32_t budget,
                                  struct hpclang_rpl_code_execution *state, uint32_t *steps)
{
    return hpclang_rpl_execution_collect_effects(boot, io, native_origin_base, budget, state, steps,
                                                 0);
}
