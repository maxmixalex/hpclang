#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "object-snapshot.h"

enum {
    RPL_DOINT      = 0x02614u,
    RPL_DOLNGREAL  = 0x0263au,
    RPL_DOLNGCMP   = 0x02660u,
    RPL_DOMATRIX   = 0x02686u,
    RPL_DOAPLET    = 0x026d5u,
    RPL_DOBINT     = 0x02911u,
    RPL_DOREAL     = 0x02933u,
    RPL_DOEREL     = 0x02955u,
    RPL_DOCMP      = 0x02977u,
    RPL_DOECMP     = 0x0299du,
    RPL_DOCHAR     = 0x029bfu,
    RPL_DOARRY     = 0x029e8u,
    RPL_DOLNKARRY  = 0x02a0au,
    RPL_DOCSTR     = 0x02a2cu,
    RPL_DOLIST     = 0x02a74u,
    RPL_DORRP      = 0x02a96u,
    RPL_DOSYMB     = 0x02ab8u,
    RPL_DOEXT      = 0x02adau,
    RPL_DOGROB     = 0x02b1eu,
    RPL_DOLIB      = 0x02b40u,
    RPL_DOCOL      = 0x02d9du,
    RPL_DOCODE     = 0x02dccu,
    RPL_DOIDNT     = 0x02e48u,
    RPL_DOLAM      = 0x02e6du
};

static enum hpclang_object_kind classify(uint32_t prologue)
{
    switch (prologue) {
    case RPL_DOINT:
    case RPL_DOBINT:
        return HPCLANG_OBJECT_INTEGER;
    case RPL_DOREAL:
    case RPL_DOEREL:
    case RPL_DOLNGREAL:
        return HPCLANG_OBJECT_REAL;
    case RPL_DOCMP:
    case RPL_DOECMP:
    case RPL_DOLNGCMP:
        return HPCLANG_OBJECT_COMPLEX;
    case RPL_DOCSTR:
        return HPCLANG_OBJECT_STRING;
    case RPL_DOCHAR:
        return HPCLANG_OBJECT_CHARACTER;
    case RPL_DOIDNT:
    case RPL_DOLAM:
        return HPCLANG_OBJECT_IDENTIFIER;
    case RPL_DOLIST:
        return HPCLANG_OBJECT_LIST;
    case RPL_DOARRY:
    case RPL_DOLNKARRY:
    case RPL_DOMATRIX:
        return HPCLANG_OBJECT_ARRAY;
    case RPL_DOSYMB:
    case RPL_DOEXT:
        return HPCLANG_OBJECT_SYMBOLIC;
    case RPL_DOCOL:
    case RPL_DOCODE:
        return HPCLANG_OBJECT_PROGRAM;
    case RPL_DORRP:
        return HPCLANG_OBJECT_DIRECTORY;
    case RPL_DOGROB:
        return HPCLANG_OBJECT_GRAPHIC;
    case RPL_DOLIB:
        return HPCLANG_OBJECT_LIBRARY;
    case RPL_DOAPLET:
        return HPCLANG_OBJECT_APLET;
    default:
        return HPCLANG_OBJECT_UNKNOWN;
    }
}

static int read_prologue(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t *prologue)
{
    if (reader == NULL || reader->read_nibble == NULL || prologue == NULL)
        return 0;

    uint32_t value = 0u;
    for (uint32_t i = 0u; i < 5u; ++i) {
        if (address > UINT32_MAX - i)
            return 0;
        uint8_t nibble = 0u;
        if (!reader->read_nibble(reader->context, address + i, &nibble) ||
            nibble > 0x0fu)
            return 0;
        value |= (uint32_t)nibble << (i * 4u);
    }

    *prologue = value;
    return 1;
}

static int span_errno(int result)
{
    switch (result) {
    case HPCLANG_RPL_SPAN_UNSUPPORTED:
        return ENOTSUP;
    case HPCLANG_RPL_SPAN_UNAVAILABLE:
        return EIO;
    case HPCLANG_RPL_SPAN_BOUNDS:
    case HPCLANG_RPL_SPAN_DEPTH:
    case HPCLANG_RPL_SPAN_MALFORMED:
    default:
        return EILSEQ;
    }
}

int hpclang_object_snapshot_copy(
    const struct hpclang_rpl_reader *reader,
    uint32_t address,
    uint32_t limit,
    struct hpclang_object **object)
{
    if (object == NULL) {
        errno = EFAULT;
        return -1;
    }
    *object = NULL;

    uint32_t span = 0u;
    int result = hpclang_rpl_object_span(reader, address, limit, &span);
    if (result != HPCLANG_RPL_SPAN_OK || span == 0u) {
        errno = span_errno(result);
        return -1;
    }

    uint32_t prologue = 0u;
    if (!read_prologue(reader, address, &prologue)) {
        errno = EIO;
        return -1;
    }

    size_t bytes = ((size_t)span + 1u) / 2u;
    if (bytes > SIZE_MAX - sizeof(struct hpclang_object)) {
        errno = ENOMEM;
        return -1;
    }

    struct hpclang_object *snapshot =
        malloc(sizeof(*snapshot) + bytes);
    if (snapshot == NULL) {
        errno = ENOMEM;
        return -1;
    }

    snapshot->kind = (uint32_t)classify(prologue);
    snapshot->nibble_count = span;
    snapshot->native_prologue = prologue;

    for (size_t i = 0u; i < bytes; ++i)
        snapshot->data[i] = 0u;

    for (uint32_t i = 0u; i < span; ++i) {
        if (address > UINT32_MAX - i) {
            free(snapshot);
            errno = EILSEQ;
            return -1;
        }

        uint8_t nibble = 0u;
        if (!reader->read_nibble(reader->context, address + i, &nibble)) {
            free(snapshot);
            errno = EIO;
            return -1;
        }
        if (nibble > 0x0fu) {
            free(snapshot);
            errno = EILSEQ;
            return -1;
        }

        if ((i & 1u) == 0u)
            snapshot->data[i >> 1] = nibble;
        else
            snapshot->data[i >> 1] |= (uint8_t)(nibble << 4);
    }

    *object = snapshot;
    return 0;
}

enum hpclang_object_kind hpclang_object_kind(
    const struct hpclang_object *object)
{
    if (object == NULL)
        return HPCLANG_OBJECT_UNKNOWN;
    return (enum hpclang_object_kind)object->kind;
}

void hpclang_object_release(struct hpclang_object *object)
{
    free(object);
}

int hpclang_object_snapshot_read_nibble(
    const struct hpclang_object *object,
    uint32_t offset,
    uint8_t *value)
{
    if (object == NULL || value == NULL || offset >= object->nibble_count)
        return 0;

    uint8_t packed = object->data[offset >> 1];
    *value = (offset & 1u) != 0u
        ? (uint8_t)(packed >> 4)
        : (uint8_t)(packed & 0x0fu);
    return 1;
}
