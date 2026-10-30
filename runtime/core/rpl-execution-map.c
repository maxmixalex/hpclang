#include "rpl-execution-map.h"
#include "rpl-stock-memory.h"
#include "rpl-code.h"

struct instruction_view
{
    const struct hpclang_rpl_reader *reader;
    const uint32_t *before, *after;
    hpclang_rpl_reference_dependency_fn dependency;
    void *context;
    int error;
};

static int instruction_read(void *context, uint32_t address, uint8_t *value)
{
    struct instruction_view *view = context;
    if (view->error)
        return 0;
    view->error = hpclang_rpl_execution_mapping_preserved(view->before, view->after, address, 1);
    if (view->error)
        return 0;
    if (!view->reader->read_nibble(view->reader->context, address, value))
        return 0;
    if (*value > 15) {
        view->error = HPCLANG_RPL_SPAN_MALFORMED;
        return 0;
    }
    view->error = view->dependency(view->context, address, *value);
    if (view->error > 0)
        view->error = HPCLANG_RPL_SPAN_MALFORMED;
    return !view->error;
}

int hpclang_rpl_execution_instruction_preserved(const struct hpclang_rpl_reader *reader,
                                                const uint32_t before[256],
                                                const uint32_t after[256], uint32_t address,
                                                uint32_t limit, uint32_t *length,
                                                hpclang_rpl_reference_dependency_fn dependency,
                                                void *context)
{
    if (!reader || !reader->read_nibble || !before || !after || !length || !dependency)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    struct instruction_view view = {reader, before, after, dependency, context, 0};
    const struct hpclang_rpl_reader guarded = {instruction_read, &view};
    uint32_t next;
    int result = hpclang_rpl_code_extent(&guarded, address, limit, &next);
    if (view.error)
        return view.error;
    if (result)
        return result;
    *length = next;
    return 0;
}

int hpclang_rpl_execution_mapping_preserved(const uint32_t before[256], const uint32_t after[256],
                                            uint32_t address, uint32_t span)
{
    if (!before || !after)
        return HPCLANG_RPL_SPAN_UNAVAILABLE;
    if (!span || address >= 0x100000u || span > 0x100000u - address)
        return HPCLANG_RPL_SPAN_BOUNDS;
    while (span) {
        uint32_t count = 0x1000u - (address & 0xfffu);
        if (count > span)
            count = span;
        uint32_t left, right;
        int result = hpclang_rpl_stock_resolve_span(before, address, count, &left);
        if (result)
            return result;
        result = hpclang_rpl_stock_resolve_span(after, address, count, &right);
        if (result)
            return result;
        if (left != right)
            return HPCLANG_RPL_SPAN_UNSUPPORTED;
        address += count;
        span -= count;
    }
    return 0;
}
