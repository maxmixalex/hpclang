#ifndef HPCLANG_RUNTIME_RPL_OBJECT_SPAN_H
#define HPCLANG_RUNTIME_RPL_OBJECT_SPAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Distinguish a native runstream reference from an object prologue, including
     * recognized
     * formats whose shape is not yet supported by the span parser. */
    int hpclang_rpl_object_prologue_known(uint32_t prologue);

    typedef int (*hpclang_rpl_read_nibble_fn)(void *context, uint32_t nibble_address,
                                              uint8_t *value);

    struct hpclang_rpl_reader
    {
        hpclang_rpl_read_nibble_fn read_nibble;
        void *context;
    };

    enum hpclang_rpl_span_result
    {
        HPCLANG_RPL_SPAN_OK = 0,
        HPCLANG_RPL_SPAN_UNAVAILABLE = -1,
        HPCLANG_RPL_SPAN_MALFORMED = -2,
        HPCLANG_RPL_SPAN_UNSUPPORTED = -3,
        HPCLANG_RPL_SPAN_BOUNDS = -4,
        HPCLANG_RPL_SPAN_DEPTH = -5
    };

    /*
     * Determine the exact object extent in Saturn nibbles.
     *
     * limit is the maximum readable extent starting at address. A successful
     * result always satisfies 0 < *span <= limit. Unknown prologues are rejected;
     * the parser never scans heuristically into adjacent calculator objects.
     */
    int hpclang_rpl_object_span(const struct hpclang_rpl_reader *reader, uint32_t address,
                                uint32_t limit, uint32_t *span);

    /* DOARRY bodies omit the shared prologue. Visit each bounded body with its
     * declared type; no synthetic prefix is written into native memory. A
     * nonzero read-only callback result aborts and is returned unchanged.
     * Discard partial traversal on error and keep the reader mapping stable.
     * Linked arrays have a different layout and are not accepted here. */
    typedef int (*hpclang_rpl_array_visit_fn)(void *context, uint32_t index, uint32_t prologue,
                                              uint32_t body, uint32_t nibbles);
    int hpclang_rpl_array_visit(const struct hpclang_rpl_reader *reader, uint32_t address,
                                uint32_t limit, hpclang_rpl_array_visit_fn visit, void *context);

#ifdef __cplusplus
}
#endif

#endif
