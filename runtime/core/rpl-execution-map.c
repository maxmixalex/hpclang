#include "rpl-execution-map.h"
#include "rpl-stock-memory.h"

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
