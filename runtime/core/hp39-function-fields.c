#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hp39-function-fields.h"
#include "object-snapshot.h"
#include "rpl-object-store.h"

#define HP39_SATURN_LIMIT UINT32_C(0x00100000)
#define HP39_RPL_DOLIST UINT32_C(0x00002a74)
#define HP39_RPL_DOROMP UINT32_C(0x00002e92)
#define HP39_RPL_DOBINT UINT32_C(0x00002911)
#define HP39_RPL_DOREAL UINT32_C(0x00002933)
#define HP39_RPL_DOARRY UINT32_C(0x000029e8)
#define HP39_RPL_DOCSTR UINT32_C(0x00002a2c)
#define HP39_RPL_DOGROB UINT32_C(0x00002b1e)
#define HP39_RPL_SEMI UINT32_C(0x0000312b)

#define HP39_PAGE_WIDTH UINT32_C(131)
#define HP39_PAGE_HEIGHT UINT32_C(64)
#define HP39_PAGE_STRIDE_NIBBLES UINT32_C(33)

struct hp39_function_field_schema
{
    const char *name;
    uint8_t name_length;
    uint32_t prologue;
};

/*
 * Source authority: HPGCC39's HP39-Aplet/Skeleton/=Sharedvar.h followed by
 * =Funcdir.h. These are the fixed-length Function Aplet fields, not the final
 * VAR-menu/ApletVars projection. Internal fields such as PlotCKSUM therefore
 * remain represented here even if a later public Variable service hides them.
 */
static const struct hp39_function_field_schema hp39_function_fields[] = {
    {"Xmin", 4u, HP39_RPL_DOREAL},        {"Xmax", 4u, HP39_RPL_DOREAL},
    {"Ymin", 4u, HP39_RPL_DOREAL},        {"Ymax", 4u, HP39_RPL_DOREAL},
    {"Xmin2", 5u, HP39_RPL_DOREAL},       {"Xmax2", 5u, HP39_RPL_DOREAL},
    {"Ymin2", 5u, HP39_RPL_DOREAL},       {"Ymax2", 5u, HP39_RPL_DOREAL},
    {"HTick", 5u, HP39_RPL_DOREAL},       {"VTick", 5u, HP39_RPL_DOREAL},
    {"HZoom", 5u, HP39_RPL_DOREAL},       {"VZoom", 5u, HP39_RPL_DOREAL},
    {"Angle", 5u, HP39_RPL_DOBINT},       {"LastX", 5u, HP39_RPL_DOREAL},
    {"LastY", 5u, HP39_RPL_DOREAL},       {"LastIndep", 9u, HP39_RPL_DOREAL},
    {"LastEq", 6u, HP39_RPL_DOBINT},      {"PlotFlag", 8u, HP39_RPL_DOBINT},
    {"Plot2Flg", 8u, HP39_RPL_DOBINT},    {"PlotCKSUM", 9u, HP39_RPL_DOREAL},
    {"SplitCKSUM", 10u, HP39_RPL_DOREAL}, {"Split2CKSUM", 11u, HP39_RPL_DOREAL},
    {"TStart", 6u, HP39_RPL_DOREAL},      {"TStep", 5u, HP39_RPL_DOREAL},
    {"TZoom", 5u, HP39_RPL_DOREAL},       {"TFlags", 6u, HP39_RPL_DOBINT},
    {"Trow", 4u, HP39_RPL_DOBINT},        {"Tcol", 4u, HP39_RPL_DOBINT},
    {"TCol1", 5u, HP39_RPL_DOBINT},       {"SplitRow", 8u, HP39_RPL_DOBINT},
    {"BTRow1", 6u, HP39_RPL_DOBINT},      {"Format", 6u, HP39_RPL_DOBINT},
    {"Digits", 6u, HP39_RPL_DOBINT},      {"Root", 4u, HP39_RPL_DOREAL},
    {"Area", 4u, HP39_RPL_DOREAL},        {"Slope", 5u, HP39_RPL_DOREAL},
    {"Isect", 5u, HP39_RPL_DOREAL},       {"Extremum", 8u, HP39_RPL_DOREAL},
};

_Static_assert(sizeof(hp39_function_fields) / sizeof(hp39_function_fields[0]) ==
                   HPCLANG_HP39_FUNCTION_FIXED_FIELD_COUNT,
               "Function fixed-field schema must stay source-complete");

struct hp39_function_variable_schema
{
    const char *category;
    uint8_t category_length;
    const char *name;
    uint8_t name_length;
    enum hpclang_hp39_function_variable_projection_kind kind;
    uint8_t fixed_field_index;
    uint16_t mask;
};

#define HP39_VAR_UNRESOLVED(category_, name_)                                                      \
    {category_,                                                                                    \
     (uint8_t)(sizeof(category_) - 1u),                                                            \
     name_,                                                                                        \
     (uint8_t)(sizeof(name_) - 1u),                                                                \
     HPCLANG_HP39_FUNCTION_VARIABLE_UNRESOLVED,                                                    \
     UINT8_MAX,                                                                                    \
     0u}
#define HP39_VAR_DIRECT(category_, name_, field_)                                                  \
    {category_,                                                                                    \
     (uint8_t)(sizeof(category_) - 1u),                                                            \
     name_,                                                                                        \
     (uint8_t)(sizeof(name_) - 1u),                                                                \
     HPCLANG_HP39_FUNCTION_VARIABLE_DIRECT_FIELD,                                                  \
     field_,                                                                                       \
     0u}
#define HP39_VAR_BINT(category_, name_, field_)                                                    \
    {category_,                                                                                    \
     (uint8_t)(sizeof(category_) - 1u),                                                            \
     name_,                                                                                        \
     (uint8_t)(sizeof(name_) - 1u),                                                                \
     HPCLANG_HP39_FUNCTION_VARIABLE_BINT_UNCOERCE,                                                 \
     field_,                                                                                       \
     0u}
#define HP39_VAR_REAL_ARRAY_LIST(category_, name_)                                                 \
    {category_,                                                                                    \
     (uint8_t)(sizeof(category_) - 1u),                                                            \
     name_,                                                                                        \
     (uint8_t)(sizeof(name_) - 1u),                                                                \
     HPCLANG_HP39_FUNCTION_VARIABLE_REAL_ARRAY_LIST,                                               \
     UINT8_MAX,                                                                                    \
     0u}
#define HP39_VAR_SYMBOLIC_SLOT(category_, name_, slot_)                                            \
    {category_,                                                                                    \
     (uint8_t)(sizeof(category_) - 1u),                                                            \
     name_,                                                                                        \
     (uint8_t)(sizeof(name_) - 1u),                                                                \
     HPCLANG_HP39_FUNCTION_VARIABLE_SYMBOLIC_SLOT,                                                 \
     UINT8_MAX,                                                                                    \
     slot_}
#define HP39_VAR_NOTE_TEXT(category_, name_)                                                       \
    {category_,                                                                                    \
     (uint8_t)(sizeof(category_) - 1u),                                                            \
     name_,                                                                                        \
     (uint8_t)(sizeof(name_) - 1u),                                                                \
     HPCLANG_HP39_FUNCTION_VARIABLE_NOTE_TEXT,                                                     \
     UINT8_MAX,                                                                                    \
     0u}
#define HP39_VAR_PAGE_NUMBER(category_, name_, field_)                                             \
    {category_,                                                                                    \
     (uint8_t)(sizeof(category_) - 1u),                                                            \
     name_,                                                                                        \
     (uint8_t)(sizeof(name_) - 1u),                                                                \
     HPCLANG_HP39_FUNCTION_VARIABLE_PAGE_NUMBER,                                                   \
     field_,                                                                                       \
     0u}
#define HP39_VAR_PAGE_GROB(category_, name_)                                                       \
    {category_,                                                                                    \
     (uint8_t)(sizeof(category_) - 1u),                                                            \
     name_,                                                                                        \
     (uint8_t)(sizeof(name_) - 1u),                                                                \
     HPCLANG_HP39_FUNCTION_VARIABLE_PAGE_GROB,                                                     \
     UINT8_MAX,                                                                                    \
     0u}
#define HP39_VAR_FLAG(category_, name_, field_, mask_)                                             \
    {category_,                                                                                    \
     (uint8_t)(sizeof(category_) - 1u),                                                            \
     name_,                                                                                        \
     (uint8_t)(sizeof(name_) - 1u),                                                                \
     HPCLANG_HP39_FUNCTION_VARIABLE_FLAG_REAL,                                                     \
     field_,                                                                                       \
     mask_}
#define HP39_VAR_FLAG_NOT(category_, name_, field_, mask_)                                         \
    {category_,                                                                                    \
     (uint8_t)(sizeof(category_) - 1u),                                                            \
     name_,                                                                                        \
     (uint8_t)(sizeof(name_) - 1u),                                                                \
     HPCLANG_HP39_FUNCTION_VARIABLE_FLAG_REAL_INVERTED,                                            \
     field_,                                                                                       \
     mask_}

/*
 * Function ApletVars is the Topic Type property #8 schema at FPTR 8:02A
 * (bank-7 logical 0x599B0). Keep this table in the ROM-published category and
 * variable order, including unresolved entries.
 *
 * Direct-field mappings are backed by HPGCC39 Sharedvar.h + Funcdir.h and the
 * Stock fixed-offset accessors. The native getter table at bank-2 0x428FF..
 * 0x42CB0 proves which internal BINTs are UNCOERCEd to public REALs and maps
 * Indep/Xcross/Ycross to LastIndep/LastX/LastY. PlotFlag/Plot2Flg masks and
 * Table TFlags bits come from HEADER.H.
 *
 * User-facing names come from the ID objects themselves, not their internal
 * x-symbol labels: xHzoom/xVzoom expose Xzoom/Yzoom, xHTick/xVTick expose
 * Xtick/Ytick, xHighRes exposes FastRes and xInvCursor exposes InvCross.
 * FastRes and InvCross are intentionally inverted by their Stock getters.
 * NumType is also inverted: TBBuildBit set means Build Your Own, while the
 * published value is 0 for Build Your Own and 1 for Automatic.
 *
 * NumIndep is the proven exception to direct fixed-field projection: its
 * Stock getter reads funcCache@ (LastBut3), enumerates the one-dimensional
 * DOREAL array and rebuilds a DOLIST. The LIST setter path accepts values only
 * when every element is a REAL, converts them to that same vector
 * representation, and replaces funcCache.
 *
 * Function's Symbolic variables are the idF1..idF0 IDs, not the callable
 * xF1..xF0 command objects. Stock 0x61399 maps those IDs to positions 1..10
 * in ListOfEqs@ (LastBut4), then NTHELCOMP returns the stored object unchanged.
 * Primitive 0x6132B only turns a returned DOBINT slot into FALSE, establishing
 * DOBINT as the undefined-slot sentinel rather than an expression transform.
 *
 * HPGCC39's standard Aplet skeleton proves the shared tail convention:
 * DirNot is LastBut1 and is a CSTR, while DirEnd/LastBut0 is the Sketch set.
 * Stock pairs NoteText getter/setter at 0x42CCE/0x43859, so NoteText is the
 * LastBut1 CSTR snapshot.
 *
 * PageNum is separately source-closed by the Stock 0x42E18 getter and helper
 * 0x4BB8F: ((Plot2Flg / YHI) & 15) + 1, with YHI=#40 (64). Thus public
 * PageNum is a REAL in 1..16 encoded in Plot2Flg bits 6..9.
 *
 * Page is closed by the Stock getter at 0x42D0F plus the native Sketch
 * loader at bank-3 0x5D706. LastBut0 is a flat DOLIST of optional CSTR hints
 * and GROB pages; CSTR updates the hint for the next GROB, while each GROB
 * advances the page count. The getter locates PageNum's GROB. With no hint it
 * returns that GROB unchanged; with a hint it reads bytes 5 and 7 as X/Y,
 * creates a blank 131x64 GROB, and GROB! copies the stored page at that offset.
 * DefaultHint is the eight bytes 83 00 40 00 00 00 00 00, confirming
 * width=131, height=64, x=0, y=0 in the shared hint encoding.
 */
static const struct hp39_function_variable_schema hp39_function_variables[] = {
    HP39_VAR_PAGE_NUMBER("Sketch", "PageNum", 18u),
    HP39_VAR_PAGE_GROB("Sketch", "Page"),
    HP39_VAR_NOTE_TEXT("Note", "NoteText"),

    HP39_VAR_DIRECT("Numeric", "NumZoom", 24u),
    HP39_VAR_FLAG_NOT("Numeric", "NumType", 25u, 2u),
    HP39_VAR_DIRECT("Numeric", "NumStep", 23u),
    HP39_VAR_DIRECT("Numeric", "NumStart", 22u),
    HP39_VAR_BINT("Numeric", "NumRow", 26u),
    HP39_VAR_REAL_ARRAY_LIST("Numeric", "NumIndep"),
    HP39_VAR_FLAG("Numeric", "NumFont", 25u, 1u),
    HP39_VAR_BINT("Numeric", "NumCol", 27u),
    HP39_VAR_BINT("Numeric", "Format", 31u),
    HP39_VAR_BINT("Numeric", "Digits", 32u),

    HP39_VAR_SYMBOLIC_SLOT("Symbolic", "F0", 10u),
    HP39_VAR_SYMBOLIC_SLOT("Symbolic", "F9", 9u),
    HP39_VAR_SYMBOLIC_SLOT("Symbolic", "F8", 8u),
    HP39_VAR_SYMBOLIC_SLOT("Symbolic", "F7", 7u),
    HP39_VAR_SYMBOLIC_SLOT("Symbolic", "F6", 6u),
    HP39_VAR_SYMBOLIC_SLOT("Symbolic", "F5", 5u),
    HP39_VAR_SYMBOLIC_SLOT("Symbolic", "F4", 4u),
    HP39_VAR_SYMBOLIC_SLOT("Symbolic", "F3", 3u),
    HP39_VAR_SYMBOLIC_SLOT("Symbolic", "F2", 2u),
    HP39_VAR_SYMBOLIC_SLOT("Symbolic", "F1", 1u),
    HP39_VAR_BINT("Symbolic", "Angle", 12u),

    HP39_VAR_DIRECT("Plot FCN", "Slope", 35u),
    HP39_VAR_DIRECT("Plot FCN", "Root", 33u),
    HP39_VAR_DIRECT("Plot FCN", "Isect", 36u),
    HP39_VAR_DIRECT("Plot FCN", "Extremum", 37u),
    HP39_VAR_DIRECT("Plot FCN", "Area", 34u),

    HP39_VAR_DIRECT("Plot", "Yzoom", 11u),
    HP39_VAR_DIRECT("Plot", "Xzoom", 10u),
    HP39_VAR_DIRECT("Plot", "Ymax", 3u),
    HP39_VAR_DIRECT("Plot", "Ymin", 2u),
    HP39_VAR_DIRECT("Plot", "Xmax", 1u),
    HP39_VAR_DIRECT("Plot", "Xmin", 0u),
    HP39_VAR_DIRECT("Plot", "Ytick", 9u),
    HP39_VAR_DIRECT("Plot", "Xtick", 8u),
    HP39_VAR_DIRECT("Plot", "Ycross", 14u),
    HP39_VAR_DIRECT("Plot", "Xcross", 13u),
    HP39_VAR_FLAG("Plot", "Tracing", 18u, 1024u),
    HP39_VAR_FLAG("Plot", "Simult", 17u, 256u),
    HP39_VAR_FLAG("Plot", "Recenter", 17u, 128u),
    HP39_VAR_FLAG("Plot", "Labels", 17u, 64u),
    HP39_VAR_FLAG_NOT("Plot", "InvCross", 17u, 512u),
    HP39_VAR_DIRECT("Plot", "Indep", 15u),
    HP39_VAR_FLAG("Plot", "Grid", 17u, 16u),
    HP39_VAR_FLAG_NOT("Plot", "FastRes", 17u, 32u),
    HP39_VAR_FLAG("Plot", "Coord", 18u, 2048u),
    HP39_VAR_FLAG("Plot", "Connect", 17u, 4u),
    HP39_VAR_FLAG("Plot", "Axes", 17u, 2u),
};

_Static_assert(sizeof(hp39_function_variables) / sizeof(hp39_function_variables[0]) ==
                   HPCLANG_HP39_FUNCTION_VARIABLE_COUNT,
               "Function ApletVars projection must stay ROM-schema complete");

#undef HP39_VAR_UNRESOLVED
#undef HP39_VAR_DIRECT
#undef HP39_VAR_BINT
#undef HP39_VAR_REAL_ARRAY_LIST
#undef HP39_VAR_SYMBOLIC_SLOT
#undef HP39_VAR_NOTE_TEXT
#undef HP39_VAR_PAGE_NUMBER
#undef HP39_VAR_PAGE_GROB
#undef HP39_VAR_FLAG
#undef HP39_VAR_FLAG_NOT

static int hp39_read_word(const struct hpclang_rpl_reader *reader, uint32_t address,
                          uint32_t nibbles, uint32_t *value)
{
    if (reader == NULL || reader->read_nibble == NULL || value == NULL || nibbles == 0u ||
        nibbles > 8u || address >= HP39_SATURN_LIMIT || nibbles > HP39_SATURN_LIMIT - address)
        return 0;

    uint32_t out = 0u;
    for (uint32_t i = 0u; i < nibbles; ++i) {
        uint8_t nibble = 0u;
        if (!reader->read_nibble(reader->context, address + i, &nibble) || nibble > 0x0fu)
            return 0;
        out |= (uint32_t)nibble << (i * 4u);
    }
    *value = out;
    return 1;
}

static int hp39_function_type_ok(const struct hpclang_rpl_reader *reader, uint32_t object_address)
{
    /*
     * Native Function tTYPE:
     * { nFUNCTOPIC nFUNCTION xFUNCTAB xFUNCSYMB xDFLTNOTE xDFLTPICT }
     * SUPROM39.A encodes these six ROMPTR payloads exactly as below.
     */
    static const uint32_t romptrs[] = {
        UINT32_C(0x000f0f1), UINT32_C(0x00000f1), UINT32_C(0x00080f1),
        UINT32_C(0x002b0f1), UINT32_C(0x00320f1), UINT32_C(0x00330f1),
    };

    uint32_t prologue = 0u;
    if (!hp39_read_word(reader, object_address, 5u, &prologue))
        return 0;
    if (prologue != HP39_RPL_DOLIST)
        return 0;

    uint32_t cursor = object_address + 5u;
    for (uint32_t i = 0u; i < (uint32_t)(sizeof(romptrs) / sizeof(romptrs[0])); ++i) {
        uint32_t payload = 0u;
        if (!hp39_read_word(reader, cursor, 5u, &prologue) || prologue != HP39_RPL_DOROMP ||
            !hp39_read_word(reader, cursor + 5u, 6u, &payload) || payload != romptrs[i])
            return 0;
        cursor += 11u;
    }

    return hp39_read_word(reader, cursor, 5u, &prologue) && prologue == HP39_RPL_SEMI;
}

static int hp39_function_entry_count_and_type(const struct hpclang_rpl_reader *reader,
                                              uint32_t aplet_object, uint32_t *entry_count)
{
    if (entry_count == NULL) {
        errno = EINVAL;
        return -1;
    }
    *entry_count = 0u;

    struct hpclang_rpl_directory_cursor cursor;
    int result = hpclang_rpl_directory_cursor_open(reader, aplet_object, &cursor);
    if (result != HPCLANG_RPL_STORE_OK) {
        errno = result == HPCLANG_RPL_STORE_UNAVAILABLE ? EIO : EILSEQ;
        return -1;
    }

    struct hpclang_rpl_directory_entry_view entry;
    struct hpclang_rpl_directory_entry_view oldest = {0u};
    uint32_t count = 0u;
    while ((result = hpclang_rpl_directory_cursor_next(reader, &cursor, &entry)) > 0) {
        oldest = entry;
        ++count;
    }
    if (result < 0) {
        errno = result == HPCLANG_RPL_STORE_LIMIT ? EOVERFLOW : EILSEQ;
        return -1;
    }

    if (count < 1u + HPCLANG_HP39_FUNCTION_FIXED_FIELD_COUNT || oldest.name_length != 0u ||
        !hp39_function_type_ok(reader, oldest.object_address)) {
        errno = ENOTSUP;
        return -1;
    }

    *entry_count = count;
    return 0;
}

int hpclang_hp39_function_field_at(const struct hpclang_rpl_reader *reader, uint32_t aplet_object,
                                   uint32_t index, struct hpclang_hp39_function_field_view *field)
{
    if (reader == NULL || reader->read_nibble == NULL || field == NULL ||
        index >= HPCLANG_HP39_FUNCTION_FIXED_FIELD_COUNT) {
        errno = EINVAL;
        return -1;
    }
    memset(field, 0, sizeof(*field));

    uint32_t count = 0u;
    if (hp39_function_entry_count_and_type(reader, aplet_object, &count) != 0)
        return -1;

    /* Cursor order is newest->oldest. Oldest is tTYPE, then fields 0..37. */
    uint32_t wanted_from_newest = count - 2u - index;
    struct hpclang_rpl_directory_cursor cursor;
    int result = hpclang_rpl_directory_cursor_open(reader, aplet_object, &cursor);
    if (result != HPCLANG_RPL_STORE_OK) {
        errno = EILSEQ;
        return -1;
    }

    struct hpclang_rpl_directory_entry_view entry;
    uint32_t ordinal = 0u;
    while ((result = hpclang_rpl_directory_cursor_next(reader, &cursor, &entry)) > 0) {
        if (ordinal == wanted_from_newest) {
            const struct hp39_function_field_schema *schema = &hp39_function_fields[index];
            if (entry.name_length != 0u || entry.prologue != schema->prologue) {
                errno = EILSEQ;
                return -1;
            }

            field->name = schema->name;
            field->name_length = schema->name_length;
            field->object_address = entry.object_address;
            field->prologue = entry.prologue;
            return 0;
        }
        ++ordinal;
    }

    errno = result < 0 ? EILSEQ : ENOENT;
    return -1;
}

int hpclang_hp39_function_field_find(const struct hpclang_rpl_reader *reader, uint32_t aplet_object,
                                     const uint8_t *name, size_t name_length,
                                     struct hpclang_hp39_function_field_view *field)
{
    if (name == NULL || name_length == 0u || field == NULL) {
        errno = EINVAL;
        return -1;
    }

    for (uint32_t i = 0u; i < HPCLANG_HP39_FUNCTION_FIXED_FIELD_COUNT; ++i) {
        const struct hp39_function_field_schema *schema = &hp39_function_fields[i];
        if (name_length == schema->name_length && memcmp(name, schema->name, name_length) == 0)
            return hpclang_hp39_function_field_at(reader, aplet_object, i, field);
    }

    errno = ENOENT;
    return -1;
}

int hpclang_hp39_function_field_snapshot(const struct hpclang_rpl_reader *reader,
                                         uint32_t aplet_object, uint32_t index,
                                         struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    struct hpclang_hp39_function_field_view field;
    if (hpclang_hp39_function_field_at(reader, aplet_object, index, &field) != 0)
        return -1;

    return hpclang_object_snapshot_copy(reader, field.object_address,
                                        HP39_SATURN_LIMIT - field.object_address, object);
}

static int hp39_function_tail_object(const struct hpclang_rpl_reader *reader, uint32_t aplet_object,
                                     uint32_t ordinal_from_newest, uint32_t expected_prologue,
                                     uint32_t *object_address)
{
    if (reader == NULL || reader->read_nibble == NULL || ordinal_from_newest == 0u ||
        object_address == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object_address = 0u;

    uint32_t entry_count = 0u;
    if (hp39_function_entry_count_and_type(reader, aplet_object, &entry_count) != 0)
        return -1;
    if (entry_count < ordinal_from_newest) {
        errno = EILSEQ;
        return -1;
    }

    struct hpclang_rpl_directory_cursor cursor;
    int result = hpclang_rpl_directory_cursor_open(reader, aplet_object, &cursor);
    if (result != HPCLANG_RPL_STORE_OK) {
        errno = result == HPCLANG_RPL_STORE_UNAVAILABLE ? EIO : EILSEQ;
        return -1;
    }

    struct hpclang_rpl_directory_entry_view entry = {0u};
    for (uint32_t ordinal = 1u; ordinal <= ordinal_from_newest; ++ordinal) {
        result = hpclang_rpl_directory_cursor_next(reader, &cursor, &entry);
        if (result <= 0) {
            errno = result < 0 ? EILSEQ : ENOENT;
            return -1;
        }
    }

    if (entry.name_length != 0u || entry.prologue != expected_prologue) {
        errno = EILSEQ;
        return -1;
    }

    *object_address = entry.object_address;
    return 0;
}

static int hp39_span_error(int result)
{
    switch (result) {
    case HPCLANG_RPL_SPAN_UNAVAILABLE:
        errno = EIO;
        break;
    case HPCLANG_RPL_SPAN_UNSUPPORTED:
        errno = ENOTSUP;
        break;
    case HPCLANG_RPL_SPAN_MALFORMED:
    case HPCLANG_RPL_SPAN_BOUNDS:
    case HPCLANG_RPL_SPAN_DEPTH:
    default:
        errno = EILSEQ;
        break;
    }
    return -1;
}

static int hp39_function_symbolic_slot_snapshot(const struct hpclang_rpl_reader *reader,
                                                uint32_t aplet_object, uint32_t slot,
                                                struct hpclang_object **object)
{
    if (object == NULL || slot == 0u || slot > 10u) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    uint32_t list = 0u;
    /*
     * ListOfEqs@=LastBut4 is the fifth newest entry because LastBut0 is the
     * newest directory entry.
     */
    if (hp39_function_tail_object(reader, aplet_object, 5u, HP39_RPL_DOLIST, &list) != 0)
        return -1;
    if (list > HP39_SATURN_LIMIT - 5u) {
        errno = EILSEQ;
        return -1;
    }

    uint32_t cursor = list + 5u;
    for (uint32_t index = 1u; index <= slot; ++index) {
        if (cursor > HP39_SATURN_LIMIT - 5u) {
            errno = EILSEQ;
            return -1;
        }

        uint32_t prologue = 0u;
        if (!hp39_read_word(reader, cursor, 5u, &prologue)) {
            errno = EIO;
            return -1;
        }
        if (prologue == HP39_RPL_SEMI) {
            errno = ENOENT;
            return -1;
        }

        uint32_t span = 0u;
        int result = hpclang_rpl_object_span(reader, cursor, HP39_SATURN_LIMIT - cursor, &span);
        if (result != HPCLANG_RPL_SPAN_OK || span == 0u)
            return hp39_span_error(result);

        if (index == slot) {
            if (prologue == HP39_RPL_DOBINT) {
                errno = ENOENT;
                return -1;
            }
            return hpclang_object_snapshot_copy(reader, cursor, span, object);
        }
        cursor += span;
    }

    errno = ENOENT;
    return -1;
}

static int hp39_function_note_text_snapshot(const struct hpclang_rpl_reader *reader,
                                            uint32_t aplet_object, struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    uint32_t address = 0u;
    if (hp39_function_tail_object(reader, aplet_object, 2u, HP39_RPL_DOCSTR, &address) != 0)
        return -1;

    return hpclang_object_snapshot_copy(reader, address, HP39_SATURN_LIMIT - address, object);
}

struct hp39_real_array_list_reader
{
    const struct hpclang_rpl_reader *source;
    uint32_t element_base;
    uint32_t element_count;
    uint32_t nibble_count;
};

static int hp39_function_func_cache(const struct hpclang_rpl_reader *reader, uint32_t aplet_object,
                                    uint32_t *object_address)
{
    /*
     * LastBut numbering is zero-based: LastBut0 is the newest directory
     * entry, so funcCache@=LastBut3 is the fourth newest entry.
     */
    return hp39_function_tail_object(reader, aplet_object, 4u, HP39_RPL_DOARRY, object_address);
}

static int hp39_function_func_cache_elements(const struct hpclang_rpl_reader *reader,
                                             uint32_t aplet_object, uint32_t *element_base,
                                             uint32_t *element_count)
{
    if (element_base == NULL || element_count == NULL) {
        errno = EINVAL;
        return -1;
    }
    *element_base = 0u;
    *element_count = 0u;

    uint32_t cache = 0u;
    if (hp39_function_func_cache(reader, aplet_object, &cache) != 0)
        return -1;
    if (cache > HP39_SATURN_LIMIT - 25u) {
        errno = EILSEQ;
        return -1;
    }

    uint32_t length = 0u;
    uint32_t element_type = 0u;
    uint32_t dimensions = 0u;
    uint32_t count = 0u;
    if (!hp39_read_word(reader, cache + 5u, 5u, &length) ||
        !hp39_read_word(reader, cache + 10u, 5u, &element_type) ||
        !hp39_read_word(reader, cache + 15u, 5u, &dimensions) ||
        !hp39_read_word(reader, cache + 20u, 5u, &count)) {
        errno = EIO;
        return -1;
    }

    /*
     * Stock's LIST->funcCache conversion constructs a one-dimensional DOREAL
     * generalized array. DOARRY stores one common prologue followed by
     * prologue-less 16-nibble DOREAL bodies.
     */
    if (element_type != HP39_RPL_DOREAL || dimensions != 1u ||
        count > (HP39_SATURN_LIMIT - 25u) / 16u) {
        errno = EILSEQ;
        return -1;
    }

    uint32_t total = 25u + count * 16u;
    if (length != total - 5u || total > HP39_SATURN_LIMIT - cache) {
        errno = EILSEQ;
        return -1;
    }

    *element_base = cache + 25u;
    *element_count = count;
    return 0;
}

static int hp39_real_array_list_read(void *context, uint32_t address, uint8_t *value)
{
    struct hp39_real_array_list_reader *view = (struct hp39_real_array_list_reader *)context;
    if (view == NULL || view->source == NULL || view->source->read_nibble == NULL ||
        value == NULL || address >= view->nibble_count)
        return 0;

    if (address < 5u) {
        *value = (uint8_t)((HP39_RPL_DOLIST >> (address * 4u)) & 0x0fu);
        return 1;
    }

    uint32_t offset = address - 5u;
    uint32_t element_nibbles = view->element_count * 21u;
    if (offset < element_nibbles) {
        uint32_t index = offset / 21u;
        uint32_t within = offset % 21u;
        if (within < 5u) {
            *value = (uint8_t)((HP39_RPL_DOREAL >> (within * 4u)) & 0x0fu);
            return 1;
        }

        return view->source->read_nibble(view->source->context,
                                         view->element_base + index * 16u + within - 5u, value);
    }

    offset -= element_nibbles;
    if (offset < 5u) {
        *value = (uint8_t)((HP39_RPL_SEMI >> (offset * 4u)) & 0x0fu);
        return 1;
    }
    return 0;
}

static int hp39_function_num_indep_snapshot(const struct hpclang_rpl_reader *reader,
                                            uint32_t aplet_object, struct hpclang_object **object)
{
    uint32_t element_base = 0u;
    uint32_t element_count = 0u;
    if (hp39_function_func_cache_elements(reader, aplet_object, &element_base, &element_count) != 0)
        return -1;

    if (element_count > (UINT32_MAX - 10u) / 21u) {
        errno = EOVERFLOW;
        return -1;
    }

    struct hp39_real_array_list_reader view = {
        .source = reader,
        .element_base = element_base,
        .element_count = element_count,
        .nibble_count = 10u + element_count * 21u,
    };
    struct hpclang_rpl_reader list_reader = {
        .read_nibble = hp39_real_array_list_read,
        .context = &view,
    };

    /*
     * Reproduce the Stock getter without numeric conversion: each compact
     * DOREAL payload is wrapped with its prologue and the sequence is closed
     * by SEMI, yielding the same owned LIST while preserving every BCD nibble.
     */
    return hpclang_object_snapshot_copy(&list_reader, 0u, view.nibble_count, object);
}

struct hp39_inline_buffer
{
    uint8_t nibbles[21];
    uint32_t nibble_count;
};

static int hp39_inline_buffer_read(void *context, uint32_t address, uint8_t *value)
{
    struct hp39_inline_buffer *buffer = (struct hp39_inline_buffer *)context;
    if (buffer == NULL || value == NULL || address >= buffer->nibble_count)
        return 0;
    *value = buffer->nibbles[address];
    return 1;
}

static int hp39_read_bint_value(const struct hpclang_rpl_reader *reader, uint32_t object_address,
                                int32_t *value)
{
    uint32_t raw = 0u;
    if (value == NULL || !hp39_read_word(reader, object_address, 5u, &raw) ||
        raw != HP39_RPL_DOBINT || !hp39_read_word(reader, object_address + 5u, 5u, &raw))
        return 0;

    *value =
        (raw & UINT32_C(0x00080000)) != 0u ? (int32_t)(raw | UINT32_C(0xfff00000)) : (int32_t)raw;
    return 1;
}

struct hp39_page_grob_reader
{
    const struct hpclang_rpl_reader *source;
    uint32_t source_payload;
    uint32_t source_width;
    uint32_t source_height;
    uint32_t source_stride;
    uint32_t x;
    uint32_t y;
    uint32_t nibble_count;
};

static int hp39_page_grob_read(void *context, uint32_t address, uint8_t *value)
{
    struct hp39_page_grob_reader *view = (struct hp39_page_grob_reader *)context;
    if (view == NULL || view->source == NULL || view->source->read_nibble == NULL ||
        value == NULL || address >= view->nibble_count)
        return 0;

    uint32_t word = 0u;
    uint32_t offset = 0u;
    if (address < 5u) {
        word = HP39_RPL_DOGROB;
        offset = address;
    } else if (address < 10u) {
        word = 15u + HP39_PAGE_HEIGHT * HP39_PAGE_STRIDE_NIBBLES;
        offset = address - 5u;
    } else if (address < 15u) {
        word = HP39_PAGE_HEIGHT;
        offset = address - 10u;
    } else if (address < 20u) {
        word = HP39_PAGE_WIDTH;
        offset = address - 15u;
    } else {
        uint32_t payload = address - 20u;
        uint32_t row = payload / HP39_PAGE_STRIDE_NIBBLES;
        uint32_t nibble_x = payload % HP39_PAGE_STRIDE_NIBBLES;
        uint32_t base_x = nibble_x * 4u;
        uint8_t out = 0u;

        for (uint32_t bit = 0u; bit < 4u; ++bit) {
            uint32_t dx = base_x + bit;
            if (dx >= HP39_PAGE_WIDTH || row < view->y || row >= view->y + view->source_height ||
                dx < view->x || dx >= view->x + view->source_width)
                continue;

            uint32_t sx = dx - view->x;
            uint32_t sy = row - view->y;
            uint8_t source_nibble = 0u;
            if (!view->source->read_nibble(
                    view->source->context,
                    view->source_payload + sy * view->source_stride + sx / 4u, &source_nibble) ||
                source_nibble > 0x0fu)
                return 0;

            if ((source_nibble & (uint8_t)(1u << (sx & 3u))) != 0u)
                out |= (uint8_t)(1u << bit);
        }

        *value = out;
        return 1;
    }

    *value = (uint8_t)((word >> (offset * 4u)) & 0x0fu);
    return 1;
}

static int hp39_function_page_number_value(const struct hpclang_rpl_reader *reader,
                                           uint32_t aplet_object, uint32_t *page)
{
    if (page == NULL) {
        errno = EINVAL;
        return -1;
    }
    *page = 0u;

    struct hpclang_hp39_function_field_view field;
    if (hpclang_hp39_function_field_at(reader, aplet_object, 18u, &field) != 0)
        return -1;
    if (field.prologue != HP39_RPL_DOBINT) {
        errno = EILSEQ;
        return -1;
    }

    int32_t stored = 0;
    if (!hp39_read_bint_value(reader, field.object_address, &stored)) {
        errno = EIO;
        return -1;
    }

    *page = (((uint32_t)stored / UINT32_C(64)) & UINT32_C(15)) + 1u;
    return 0;
}

static int hp39_function_grob_layout(const struct hpclang_rpl_reader *reader, uint32_t address,
                                     uint32_t *width, uint32_t *height, uint32_t *payload,
                                     uint32_t *span)
{
    if (width == NULL || height == NULL || payload == NULL || span == NULL) {
        errno = EINVAL;
        return -1;
    }

    uint32_t prologue = 0u;
    uint32_t length = 0u;
    uint32_t h = 0u;
    uint32_t w = 0u;
    if (!hp39_read_word(reader, address, 5u, &prologue) ||
        !hp39_read_word(reader, address + 5u, 5u, &length) ||
        !hp39_read_word(reader, address + 10u, 5u, &h) ||
        !hp39_read_word(reader, address + 15u, 5u, &w)) {
        errno = EIO;
        return -1;
    }
    if (prologue != HP39_RPL_DOGROB || w > HP39_PAGE_WIDTH || h > HP39_PAGE_HEIGHT) {
        errno = EILSEQ;
        return -1;
    }

    uint32_t stride = (w + 3u) / 4u;
    uint32_t data_nibbles = stride * h;
    uint32_t expected_length = 15u + data_nibbles;
    if (length != expected_length || address > HP39_SATURN_LIMIT - (5u + expected_length)) {
        errno = EILSEQ;
        return -1;
    }

    *width = w;
    *height = h;
    *payload = address + 20u;
    *span = 5u + expected_length;
    return 0;
}

static int hp39_function_page_hint(const struct hpclang_rpl_reader *reader, uint32_t address,
                                   uint32_t *x, uint32_t *y)
{
    if (x == NULL || y == NULL) {
        errno = EINVAL;
        return -1;
    }

    uint32_t prologue = 0u;
    uint32_t length = 0u;
    if (!hp39_read_word(reader, address, 5u, &prologue) ||
        !hp39_read_word(reader, address + 5u, 5u, &length)) {
        errno = EIO;
        return -1;
    }
    /* Need at least seven bytes so Stock's SUB$1# indices 5 and 7 exist. */
    if (prologue != HP39_RPL_DOCSTR || length < 5u + 14u || ((length - 5u) & 1u) != 0u) {
        errno = EILSEQ;
        return -1;
    }

    if (!hp39_read_word(reader, address + 18u, 2u, x) ||
        !hp39_read_word(reader, address + 22u, 2u, y)) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static int hp39_function_page_normalized_snapshot(const struct hpclang_rpl_reader *reader,
                                                  uint32_t grob_address, uint32_t hint_address,
                                                  struct hpclang_object **object)
{
    uint32_t width = 0u;
    uint32_t height = 0u;
    uint32_t payload = 0u;
    uint32_t span = 0u;
    if (hp39_function_grob_layout(reader, grob_address, &width, &height, &payload, &span) != 0)
        return -1;

    if (hint_address == 0u)
        return hpclang_object_snapshot_copy(reader, grob_address, span, object);

    uint32_t x = 0u;
    uint32_t y = 0u;
    if (hp39_function_page_hint(reader, hint_address, &x, &y) != 0)
        return -1;
    if (x > HP39_PAGE_WIDTH || y > HP39_PAGE_HEIGHT || width > HP39_PAGE_WIDTH - x ||
        height > HP39_PAGE_HEIGHT - y) {
        errno = EILSEQ;
        return -1;
    }

    struct hp39_page_grob_reader view = {
        .source = reader,
        .source_payload = payload,
        .source_width = width,
        .source_height = height,
        .source_stride = (width + 3u) / 4u,
        .x = x,
        .y = y,
        .nibble_count = 20u + HP39_PAGE_HEIGHT * HP39_PAGE_STRIDE_NIBBLES,
    };
    struct hpclang_rpl_reader page_reader = {
        .read_nibble = hp39_page_grob_read,
        .context = &view,
    };
    return hpclang_object_snapshot_copy(&page_reader, 0u, view.nibble_count, object);
}

static int hp39_function_page_snapshot(const struct hpclang_rpl_reader *reader,
                                       uint32_t aplet_object, struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    uint32_t page = 0u;
    if (hp39_function_page_number_value(reader, aplet_object, &page) != 0)
        return -1;

    uint32_t sketch = 0u;
    if (hp39_function_tail_object(reader, aplet_object, 1u, HP39_RPL_DOLIST, &sketch) != 0)
        return -1;
    if (sketch > HP39_SATURN_LIMIT - 5u) {
        errno = EILSEQ;
        return -1;
    }

    uint32_t cursor = sketch + 5u;
    uint32_t hint = 0u;
    uint32_t current_page = 0u;
    for (;;) {
        if (cursor > HP39_SATURN_LIMIT - 5u) {
            errno = EILSEQ;
            return -1;
        }

        uint32_t prologue = 0u;
        if (!hp39_read_word(reader, cursor, 5u, &prologue)) {
            errno = EIO;
            return -1;
        }
        if (prologue == HP39_RPL_SEMI) {
            errno = ENOENT;
            return -1;
        }

        uint32_t span = 0u;
        int result = hpclang_rpl_object_span(reader, cursor, HP39_SATURN_LIMIT - cursor, &span);
        if (result != HPCLANG_RPL_SPAN_OK || span == 0u)
            return hp39_span_error(result);

        if (prologue == HP39_RPL_DOCSTR) {
            hint = cursor;
        } else if (prologue == HP39_RPL_DOGROB) {
            ++current_page;
            if (current_page == page)
                return hp39_function_page_normalized_snapshot(reader, cursor, hint, object);
            hint = 0u;
        } else {
            errno = EILSEQ;
            return -1;
        }

        if (span > HP39_SATURN_LIMIT - cursor) {
            errno = EILSEQ;
            return -1;
        }
        cursor += span;
    }
}

static int hp39_make_integer_real_snapshot(int32_t value, struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    int64_t signed_magnitude = value;
    uint64_t magnitude =
        signed_magnitude < 0 ? (uint64_t)(-signed_magnitude) : (uint64_t)signed_magnitude;

    uint32_t digits = 1u;
    if (magnitude != 0u) {
        uint64_t scan = magnitude;
        digits = 0u;
        while (scan != 0u) {
            ++digits;
            scan /= UINT64_C(10);
        }
    }
    if (digits > 12u) {
        errno = ERANGE;
        return -1;
    }

    uint32_t exponent = magnitude == 0u ? 0u : digits - 1u;
    uint64_t mantissa = magnitude;
    for (uint32_t i = digits; i < 12u; ++i)
        mantissa *= UINT64_C(10);

    struct hp39_inline_buffer buffer = {
        .nibbles = {0u},
        .nibble_count = 21u,
    };
    for (uint32_t i = 0u; i < 5u; ++i)
        buffer.nibbles[i] = (uint8_t)((HP39_RPL_DOREAL >> (i * 4u)) & 0x0fu);
    for (uint32_t i = 0u; i < 3u; ++i) {
        buffer.nibbles[5u + i] = (uint8_t)(exponent % 10u);
        exponent /= 10u;
    }
    for (uint32_t i = 0u; i < 12u; ++i) {
        buffer.nibbles[8u + i] = (uint8_t)(mantissa % UINT64_C(10));
        mantissa /= UINT64_C(10);
    }
    buffer.nibbles[20u] = value < 0 ? 9u : 0u;

    struct hpclang_rpl_reader encoded_reader = {
        .read_nibble = hp39_inline_buffer_read,
        .context = &buffer,
    };
    return hpclang_object_snapshot_copy(&encoded_reader, 0u, buffer.nibble_count, object);
}

static void
hp39_function_variable_projection_copy(const struct hp39_function_variable_schema *schema,
                                       struct hpclang_hp39_function_variable_projection *projection)
{
    projection->category = schema->category;
    projection->category_length = schema->category_length;
    projection->name = schema->name;
    projection->name_length = schema->name_length;
    projection->kind = schema->kind;
    projection->fixed_field_index = schema->fixed_field_index == UINT8_MAX
                                        ? HPCLANG_HP39_FUNCTION_VARIABLE_NO_FIELD
                                        : (uint32_t)schema->fixed_field_index;
    projection->mask = schema->mask;
}

int hpclang_hp39_function_variable_projection_at(
    uint32_t index, struct hpclang_hp39_function_variable_projection *projection)
{
    if (projection == NULL || index >= HPCLANG_HP39_FUNCTION_VARIABLE_COUNT) {
        errno = EINVAL;
        return -1;
    }

    hp39_function_variable_projection_copy(&hp39_function_variables[index], projection);
    return 0;
}

int hpclang_hp39_function_variable_projection_find(
    const uint8_t *category, size_t category_length, const uint8_t *name, size_t name_length,
    struct hpclang_hp39_function_variable_projection *projection)
{
    if (category == NULL || category_length == 0u || name == NULL || name_length == 0u ||
        projection == NULL) {
        errno = EINVAL;
        return -1;
    }

    for (uint32_t i = 0u; i < HPCLANG_HP39_FUNCTION_VARIABLE_COUNT; ++i) {
        const struct hp39_function_variable_schema *schema = &hp39_function_variables[i];
        if (category_length == schema->category_length && name_length == schema->name_length &&
            memcmp(category, schema->category, category_length) == 0 &&
            memcmp(name, schema->name, name_length) == 0) {
            hp39_function_variable_projection_copy(schema, projection);
            return 0;
        }
    }

    errno = ENOENT;
    return -1;
}

int hpclang_hp39_function_variable_snapshot(const struct hpclang_rpl_reader *reader,
                                            uint32_t aplet_object, const uint8_t *category,
                                            size_t category_length, const uint8_t *name,
                                            size_t name_length, struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EINVAL;
        return -1;
    }
    *object = NULL;

    struct hpclang_hp39_function_variable_projection projection;
    if (hpclang_hp39_function_variable_projection_find(category, category_length, name, name_length,
                                                       &projection) != 0)
        return -1;

    if (projection.kind == HPCLANG_HP39_FUNCTION_VARIABLE_UNRESOLVED) {
        errno = ENOTSUP;
        return -1;
    }

    if (projection.kind == HPCLANG_HP39_FUNCTION_VARIABLE_DIRECT_FIELD)
        return hpclang_hp39_function_field_snapshot(reader, aplet_object,
                                                    projection.fixed_field_index, object);

    if (projection.kind == HPCLANG_HP39_FUNCTION_VARIABLE_REAL_ARRAY_LIST)
        return hp39_function_num_indep_snapshot(reader, aplet_object, object);

    if (projection.kind == HPCLANG_HP39_FUNCTION_VARIABLE_SYMBOLIC_SLOT)
        return hp39_function_symbolic_slot_snapshot(reader, aplet_object, (uint32_t)projection.mask,
                                                    object);

    if (projection.kind == HPCLANG_HP39_FUNCTION_VARIABLE_NOTE_TEXT)
        return hp39_function_note_text_snapshot(reader, aplet_object, object);

    if (projection.kind == HPCLANG_HP39_FUNCTION_VARIABLE_PAGE_GROB)
        return hp39_function_page_snapshot(reader, aplet_object, object);

    struct hpclang_hp39_function_field_view field;
    if (hpclang_hp39_function_field_at(reader, aplet_object, projection.fixed_field_index,
                                       &field) != 0)
        return -1;
    if (field.prologue != HP39_RPL_DOBINT) {
        errno = EILSEQ;
        return -1;
    }

    int32_t stored = 0;
    if (!hp39_read_bint_value(reader, field.object_address, &stored)) {
        errno = EIO;
        return -1;
    }

    if (projection.kind == HPCLANG_HP39_FUNCTION_VARIABLE_BINT_UNCOERCE)
        return hp39_make_integer_real_snapshot(stored, object);

    if (projection.kind == HPCLANG_HP39_FUNCTION_VARIABLE_PAGE_NUMBER) {
        uint32_t page = 0u;
        if (hp39_function_page_number_value(reader, aplet_object, &page) != 0)
            return -1;
        return hp39_make_integer_real_snapshot((int32_t)page, object);
    }

    uint32_t value = ((uint32_t)stored & projection.mask) != 0u ? 1u : 0u;
    if (projection.kind == HPCLANG_HP39_FUNCTION_VARIABLE_FLAG_REAL_INVERTED)
        value ^= 1u;

    return hp39_make_integer_real_snapshot((int32_t)value, object);
}
