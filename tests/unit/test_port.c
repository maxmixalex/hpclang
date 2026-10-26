#include <assert.h>
#include <errno.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <hpclang/abi.h>
#include <hpclang/bootstrap.h>
#include <hpclang/device.h>
#include <hpclang/port.h>
#include <hpclang/target.h>
#include "saturn-view.h"
#include "rpl-stock-memory.h"
#include "rpl-flash-write.h"
#include "object-snapshot.h"
#include "runtime-stop.h"

static uint8_t memory[0x100000];
static int unavailable;
static int writable = 1;
static int port1_available;
static int port2_available;
static uint8_t flash[HPCLANG_RPL_PORT2_BANKS][HPCLANG_RPL_FLASH_BANK_NIBBLES];
static uint32_t missing_flash_bank = UINT32_MAX;
static uint32_t port1_limit = 0x2000u;
static unsigned flash_writes;
static struct hpclang_boot_info boot = {.saturn_state = 1};
static struct hpclang_device_info device = {.family = HPCLANG_FAMILY_HP4950};

static uint32_t failed_write = UINT32_MAX;
static int expect_terminal;
static jmp_buf terminal_context;

__attribute__((noreturn)) void hpclang_runtime_stop(void)
{
    assert(expect_terminal);
    longjmp(terminal_context, 1);
}

static int flash_word_read(void *context, uint32_t address, uint16_t *value)
{
    (void)context;
    assert(address >= 0x140000u && address < 0x200000u && !(address & 1u));
    uint32_t bank = (address - 0x140000u) / 0x20000u;
    if (!port2_available || bank == missing_flash_bank)
        return 0;
    uint32_t offset = (address & 0x1ffffu) * 2u;
    *value = 0;
    for (unsigned i = 0; i < 4; ++i)
        *value |= (uint16_t)flash[bank][offset + i] << (4u * i);
    return 1;
}

static int flash_word_program(void *context, uint32_t address, uint16_t value)
{
    ++flash_writes;
    uint16_t old;
    assert(flash_word_read(context, address, &old) && (old & value) == value);
    uint32_t bank = (address - 0x140000u) / 0x20000u;
    uint32_t offset = (address & 0x1ffffu) * 2u;
    for (unsigned i = 0; i < 4; ++i)
        flash[bank][offset + i] = (value >> (4u * i)) & 15u;
    return 1;
}

int hpclang_stock_flash_recover_tails(void)
{
    if (!port2_available)
        return HPCLANG_RPL_FLASH_UNSUPPORTED;
    const struct hpclang_rpl_flash_io io = {0, flash_word_read, flash_word_program, 0x140000u,
                                            0x200000u};
    return hpclang_rpl_flash_prepare_write(&io);
}

int hpclang_stock_flash_create_backup(const uint8_t *record, uint32_t bytes)
{
    if (!port2_available)
        return HPCLANG_RPL_FLASH_UNSUPPORTED;
    const struct hpclang_rpl_flash_io io = {0, flash_word_read, flash_word_program, 0x140000u,
                                            0x200000u};
    return hpclang_rpl_flash_create_backup(&io, record, bytes, NULL);
}
int hpclang_stock_flash_create_library(const uint8_t *record, uint32_t bytes)
{
    if (!port2_available)
        return HPCLANG_RPL_FLASH_UNSUPPORTED;
    const struct hpclang_rpl_flash_io io = {0, flash_word_read, flash_word_program, 0x140000u,
                                            0x200000u};
    return hpclang_rpl_flash_create_library(&io, record, bytes, NULL);
}
int hpclang_stock_flash_remove_backup(const uint8_t *name, uint32_t name_length)
{
    if (!port2_available)
        return HPCLANG_RPL_FLASH_UNSUPPORTED;
    const struct hpclang_rpl_flash_io io = {0, flash_word_read, flash_word_program, 0x140000u,
                                            0x200000u};
    return hpclang_rpl_flash_remove_backup(&io, name, name_length, NULL);
}
int hpclang_stock_flash_replace_backup(const uint8_t *record, uint32_t bytes)
{
    if (!port2_available)
        return HPCLANG_RPL_FLASH_UNSUPPORTED;
    const struct hpclang_rpl_flash_io io = {0, flash_word_read, flash_word_program, 0x140000u,
                                            0x200000u};
    struct hpclang_rpl_flash_replacement transaction = {UINT32_MAX, UINT32_MAX, 0};
    return hpclang_rpl_flash_replace_backup(&io, record, bytes, &transaction);
}
const struct hpclang_boot_info *hpclang_boot_info(void)
{
    return &boot;
}
const struct hpclang_device_info *hpclang_device_info(void)
{
    return &device;
}
int hpclang_saturn_read_nibble(struct hpclang_saturn_address address, uint8_t *value)
{
    if (unavailable || address.nibble >= sizeof(memory))
        return 0;
    *value = memory[address.nibble];
    return 1;
}

static int mapped_read(void *context, uint32_t address, uint8_t *value)
{
    (void)context;
    return hpclang_saturn_read_nibble(hpclang_saturn_address_make(address), value);
}

static int mapped_write(void *context, uint32_t address, uint8_t value)
{
    (void)context;
    if (address >= sizeof(memory) || address == failed_write)
        return 0;
    memory[address] = value;
    return 1;
}

int hpclang_rpl_stock_port1_reader(struct hpclang_rpl_reader *reader)
{
    if (!port1_available)
        return 0;
    *reader = (struct hpclang_rpl_reader){mapped_read, NULL};
    return 1;
}

static int flash_read(void *context, uint32_t address, uint8_t *value)
{
    if (!port2_available || address >= HPCLANG_RPL_FLASH_BANK_NIBBLES)
        return 0;
    *value = ((const uint8_t *)context)[address];
    return 1;
}

int hpclang_rpl_stock_port2_reader(uint32_t bank, struct hpclang_rpl_reader *reader)
{
    if (!port2_available || bank >= HPCLANG_RPL_PORT2_BANKS || bank == missing_flash_bank)
        return 0;
    *reader = (struct hpclang_rpl_reader){flash_read, flash[bank]};
    return 1;
}

int hpclang_rpl_stock_port1_mutation_io(struct hpclang_rpl_mutation_io *io, uint32_t *limit)
{
    if (!port1_available || !writable)
        return 0;
    *io =
        (struct hpclang_rpl_mutation_io){.read_nibble = mapped_read, .write_nibble = mapped_write};
    *limit = port1_limit;
    return 1;
}

int hpclang_rpl_stock_mutation_io(struct hpclang_rpl_mutation_io *io,
                                  struct hpclang_rpl_stock_memory *view)
{
    (void)view;
    *io = (struct hpclang_rpl_mutation_io){.read_nibble = mapped_read,
                                           .write_nibble = mapped_write,
                                           .temp_limit = 0x86000,
                                           .user_limit = 0x88000};
    return writable;
}

static void put_word(uint32_t address, uint32_t value)
{
    for (uint32_t i = 0; i < 5; ++i)
        memory[address + i] = (value >> (4 * i)) & 15;
}

static void fixture(void)
{
    const char *hex = "26b20130008005f42545a55425f48041620900003210119200f9e3";
    memset(memory, 0, sizeof(memory));
    for (uint32_t i = 0; i < 5; ++i)
        memory[0x80716 + i] = (0xfffc0u >> (4 * i)) & 15;
    for (uint32_t i = 0; hex[i]; ++i)
        memory[0xfffc0 + i] = (uint8_t)(hex[i] <= '9' ? hex[i] - '0' : hex[i] - 'a' + 10);
}

static void test_discovery(void)
{
    struct hpclang_boot_info saved_boot = boot;
    struct hpclang_device_info saved_device = device;
    struct hpclang_port_info info = {.size = sizeof(info), .version = HPCLANG_PORT_INFO_VERSION};
    struct hpclang_rpl_memory_bounds bounds = {.magic = HPCLANG_RPL_BOUNDS_MAGIC,
                                               .temp_limit = 0xa0000u,
                                               .user_limit = 0xb0000u,
                                               .port1_limit = 0};
    uint32_t reads = HPCLANG_PORT_ENUMERATE | HPCLANG_PORT_READ_BACKUP | HPCLANG_PORT_READ_LIBRARY;
    for (uint32_t model = HPCLANG_MODEL_HP39GPLUS; model <= HPCLANG_MODEL_HP50G; ++model) {
        device.model = boot.model = model;
        boot.platform_cookie = (uintptr_t)&bounds | HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
        for (uint32_t port = 0; port < 3; ++port) {
            int present =
                model >= HPCLANG_MODEL_HP48GII && (port == 0 || model != HPCLANG_MODEL_HP48GII);
            boot.saturn_state = 0;
            errno = EDOM;
            assert(hpclang_port_query(port, &info) == 0 && errno == EDOM);
            assert(info.port == port && info.available == 0);
            assert((info.medium != HPCLANG_PORT_ABSENT) == present);
            assert((info.implemented & reads) == (present ? reads : 0));
            assert(!!(info.implemented & HPCLANG_PORT_CREATE_LIBRARY) == present);
            if (present)
                assert(info.medium == port + HPCLANG_PORT_MAIN_RAM);
            boot.saturn_state = 1;
            assert(hpclang_port_query(port, &info) == 0);
            assert(info.available == info.implemented);
            if (present && port == 2)
                assert(info.available ==
                       (reads | HPCLANG_PORT_CREATE_BACKUP | HPCLANG_PORT_REMOVE_BACKUP |
                        HPCLANG_PORT_REPLACE_BACKUP | HPCLANG_PORT_CREATE_LIBRARY));
        }
    }
    // Zero writable headroom means supported-but-full, not an unavailable API.
    assert(hpclang_port_query(1, &info) == 0 && (info.available & HPCLANG_PORT_CREATE_BACKUP));
    bounds.port1_limit = HPCLANG_RPL_PORT1_NIBBLES + 1;
    assert(hpclang_port_query(1, &info) == 0 && info.available == reads);
    bounds.port1_limit = 0;
    bounds.magic = HPCLANG_RPL_BOUNDS_MAGIC_V2;
    assert(hpclang_port_query(1, &info) == 0 && info.available == reads);
    assert(hpclang_port_query(0, &info) == 0 && info.available == info.implemented);
    size_t legacy_size = offsetof(struct hpclang_rpl_memory_bounds, port1_limit);
    void *legacy = malloc(legacy_size);
    assert(legacy);
    memcpy(legacy, &bounds, legacy_size);
    boot.platform_cookie = (uintptr_t)legacy | HPCLANG_PLATFORM_COOKIE_RPL_BOUNDS_TAG;
    assert(hpclang_port_query(1, &info) == 0 && info.available == reads);
    assert(hpclang_port_query(0, &info) == 0 && info.available == info.implemented);
    free(legacy);
    boot.platform_cookie = 0;
    assert(hpclang_port_query(0, &info) == 0 && info.available == reads);
    boot.model = HPCLANG_MODEL_HP39GPLUS;
    assert(hpclang_port_query(0, &info) == 0 && info.available == 0);
    info.size += 16;
    assert(hpclang_port_query(0, &info) == 0 && info.size == sizeof(info) + 16);
    struct hpclang_port_info previous = info;
    assert(hpclang_port_query(3, &info) == -1 && errno == EINVAL);
    assert(memcmp(&info, &previous, sizeof(info)) == 0);
    device.model = HPCLANG_MODEL_UNKNOWN;
    assert(hpclang_port_query(0, &info) == -1 && errno == ENOSYS);
    assert(memcmp(&info, &previous, sizeof(info)) == 0);
    info.version = 0;
    assert(hpclang_port_query(0, &info) == -1 && errno == EINVAL);
    assert(hpclang_port_query(0, NULL) == -1 && errno == EINVAL);
    info.version = HPCLANG_PORT_INFO_VERSION;
    info.size = sizeof(info) - 1;
    assert(hpclang_port_query(0, &info) == -1 && errno == EINVAL);
    boot = saved_boot;
    device = saved_device;
}

static int packed_flash_nibble(void *context, uint32_t address, uint8_t *value)
{
    const uint8_t *data = context;
    *value = (data[address / 2u] >> (4u * (address & 1u))) & 15u;
    return 1;
}

static void test_flash_read_recovery(const struct hpclang_object *original)
{
    const uint8_t integer[] = {0x14, 0x26, 0x90, 0, 0, 0x86, 0x09}; /* ZINT 986. */
    const struct hpclang_rpl_reader source = {packed_flash_nibble, (void *)integer};
    uint8_t *backup, *record;
    uint32_t span, bytes;
    assert(
        !hpclang_rpl_backup_encode(&source, 0, 14, (const uint8_t *)"PORTZERO", 8, &backup, &span));
    const struct hpclang_rpl_reader encoded = {packed_flash_nibble, backup};
    assert(!hpclang_rpl_flash_backup_record(&encoded, 0, span, &record, &bytes));
    free(backup);
    const struct hpclang_rpl_flash_io io = {0, flash_word_read, flash_word_program, 0x140000u,
                                            0x200000u};
    for (unsigned scenario = 0; scenario < 5; ++scenario) {
        struct hpclang_rpl_flash_replace_plan plan;
        assert(!hpclang_rpl_flash_publish_intent(&io, record, bytes, &plan));
        if (scenario == 1)
            assert(flash_word_program(NULL, (plan.intent.new_slot + 6u) & ~1u, 0));
        else if (scenario >= 2) {
            assert(!hpclang_rpl_flash_append(&io, plan.intent.new_slot, record, bytes));
            if (scenario == 3) {
                uint32_t bank = (plan.intent.new_slot - io.first) / 0x20000u;
                uint32_t at = (plan.intent.new_slot & 0x1ffffu) * 2u;
                flash[bank][at] = 15;
                flash[bank][at + 1u] = 7;
            }
        }
        if (scenario == 4) {
            flash[5][0x202] ^= 1u;
            unsigned before = flash_writes;
            assert(!hpclang_port_backup_get(2, (const uint8_t *)"PORTZERO", 8));
            assert(errno == EILSEQ && flash_writes == before);
            flash[5][0x202] ^= 1u;
        }
        if (scenario == 1) {
            struct hpclang_port_iterator *iterator = hpclang_port_iterator_open(2);
            assert(iterator);
            hpclang_port_iterator_close(iterator);
        } else if (scenario == 3) {
            struct hpclang_object *library = hpclang_port_library_get(2, 123);
            assert(library);
            hpclang_object_release(library);
        }
        struct hpclang_object *value = hpclang_port_backup_get(2, (const uint8_t *)"PORTZERO", 8);
        int64_t number;
        assert(value && !hpclang_object_integer_i64(value, &number));
        assert(number == (scenario < 2 ? 123 : 986));
        hpclang_object_release(value);
        unsigned recovered = flash_writes;
        value = hpclang_port_backup_get(2, (const uint8_t *)"PORTZERO", 8);
        assert(value && flash_writes == recovered);
        hpclang_object_release(value);
        assert(!hpclang_port_backup_replace(2, (const uint8_t *)"PORTZERO", 8, original));
    }
    free(record);
}

static void test_flash(void)
{
    memset(flash, 15, sizeof(flash));
    for (uint32_t bank = 0; bank < HPCLANG_RPL_PORT2_BANKS; ++bank) {
        uint32_t header = 0xf0u | ((bank + 8u) << 8u);
        for (uint32_t i = 0; i < 4; ++i)
            flash[bank][0x200 + i] = (header >> (4u * i)) & 15u;
    }
    const char *records[] = {"35e3000104b2062000108510b7000000000000000000000d5c2",
                             "35e3000026b20130008005f42545a55425f48041620900003210119200f9e3"};
    const uint32_t banks[] = {1, 5};
    for (uint32_t j = 0; j < 2; ++j)
        for (uint32_t i = 0; records[j][i]; ++i)
            flash[banks[j]][0x20a + i] =
                records[j][i] <= '9' ? records[j][i] - '0' : records[j][i] - 'a' + 10;
    port2_available = 1;
    device.family = HPCLANG_FAMILY_HP48;
    assert(!hpclang_port_iterator_open(2) && errno == ENOSYS);
    device.family = HPCLANG_FAMILY_HP4950;
    boot.saturn_state = 0;
    assert(!hpclang_port_iterator_open(2) && errno == ENOSYS);
    boot.saturn_state = 1;
    struct hpclang_port_iterator *iterator = hpclang_port_iterator_open(2);
    assert(iterator);
    struct hpclang_object *value = hpclang_port_backup_get(2, (const uint8_t *)"PORTZERO", 8);
    struct hpclang_object *library = hpclang_port_library_get(2, 123);
    assert(value && library && hpclang_object_kind(library) == HPCLANG_OBJECT_LIBRARY);
    test_flash_read_recovery(value);
    assert(!hpclang_port_backup_create(2, (const uint8_t *)"NEWFLASH", 8, value));
    struct hpclang_object *created = hpclang_port_backup_get(2, (const uint8_t *)"NEWFLASH", 8);
    int64_t number;
    assert(created && !hpclang_object_integer_i64(created, &number) && number == 123);
    hpclang_object_release(created);
    assert(hpclang_port_backup_create(2, (const uint8_t *)"NEWFLASH", 8, value) == -1 &&
           errno == EEXIST);
    assert(hpclang_port_backup_create(2, (const uint8_t *)"PORTZERO", 8, value) == -1 &&
           errno == EEXIST);
    assert(!hpclang_port_backup_remove(2, (const uint8_t *)"NEWFLASH", 8));
    assert(!hpclang_port_backup_get(2, (const uint8_t *)"NEWFLASH", 8) && errno == ENOENT);
    assert(hpclang_port_backup_remove(2, (const uint8_t *)"NEWFLASH", 8) == -1 && errno == ENOENT);
    assert(!hpclang_object_integer_i64(value, &number) && number == 123);
    assert(!hpclang_port_backup_create(2, (const uint8_t *)"NEWFLASH", 8, value));
    assert(!hpclang_port_backup_get(2, (const uint8_t *)"missing", 7) && errno == ENOENT);
    assert(hpclang_port_backup_replace(2, (const uint8_t *)"missing", 7, value) == -1 &&
           errno == ENOENT);
    struct hpclang_object *replacement = NULL;
    assert(!hpclang_object_create_string((const uint8_t *)"replacement", 11, &replacement));
    assert(!hpclang_port_backup_replace(2, (const uint8_t *)"NEWFLASH", 8, replacement));
    hpclang_object_release(replacement);
    replacement = hpclang_port_backup_get(2, (const uint8_t *)"NEWFLASH", 8);
    assert(replacement && hpclang_object_kind(replacement) == HPCLANG_OBJECT_STRING);
    hpclang_object_release(replacement);
    assert(!hpclang_port_backup_replace(2, (const uint8_t *)"NEWFLASH", 8, value));
    assert(!hpclang_object_integer_i64(value, &number) && number == 123);
    missing_flash_bank = 3;
    assert(hpclang_port_backup_remove(2, (const uint8_t *)"NEWFLASH", 8) == -1 && errno == EIO);
    assert(hpclang_port_backup_create(2, (const uint8_t *)"FAIL", 4, value) == -1 && errno == EIO);
    assert(!hpclang_port_iterator_open(2) && errno == EIO);
    missing_flash_bank = UINT32_MAX;
    flash[5][0x202] ^= 1;
    assert(hpclang_port_backup_remove(2, (const uint8_t *)"NEWFLASH", 8) == -1 && errno == EILSEQ);
    assert(hpclang_port_backup_create(2, (const uint8_t *)"FAIL", 4, value) == -1 &&
           errno == EILSEQ);
    assert(!hpclang_port_iterator_open(2) && errno == EILSEQ);
    flash[5][0x202] ^= 1;
    assert(!hpclang_port_backup_remove(2, (const uint8_t *)"PORTZERO", 8));
    assert(!hpclang_port_backup_get(2, (const uint8_t *)"PORTZERO", 8) && errno == ENOENT);
    port2_available = 0;
    memset(flash, 0, sizeof(flash));
    int64_t integer;
    assert(hpclang_object_integer_i64(value, &integer) == 0 && integer == 123);
    struct hpclang_port_entry entry = {.size = sizeof(entry),
                                       .version = HPCLANG_PORT_ENTRY_VERSION};
    assert(hpclang_port_iterator_read(iterator, &entry) == 1 && entry.library_number == 123);
    assert(hpclang_port_iterator_read(iterator, &entry) == 1 && entry.kind == HPCLANG_PORT_BACKUP);
    assert(hpclang_port_iterator_read(iterator, &entry) == 0);
    hpclang_port_iterator_close(iterator);
    hpclang_object_release(value);
    hpclang_object_release(library);
}

static void check_port1_terminal(const struct hpclang_object *value,
                                 const struct hpclang_object *library)
{
    for (unsigned kind = 0; kind < 2; ++kind) {
        /* Each prologue publication boundary must take the terminal path. */
        for (failed_write = 0; failed_write < 5; ++failed_write) {
            memset(memory, 0, port1_limit);
            expect_terminal = 1;
            if (!setjmp(terminal_context)) {
                if (kind)
                    (void)hpclang_port_library_create(1, library);
                else
                    (void)hpclang_port_backup_create(1, (const uint8_t *)"NEW", 3, value);
                assert(!"Partial native publication returned to the application");
            }
            expect_terminal = 0;
        }
        /* An unpublished-body fault remains an ordinary recoverable IO error. */
        memset(memory, 0, port1_limit);
        failed_write = 5;
        int result = kind ? hpclang_port_library_create(1, library)
                          : hpclang_port_backup_create(1, (const uint8_t *)"NEW", 3, value);
        assert(result == -1 && errno == EIO);
        for (unsigned i = 0; i < 5; ++i)
            assert(!memory[i]);
    }
    failed_write = UINT32_MAX;
    memset(memory, 0, port1_limit);
}

int main(void)
{
    test_discovery();
    test_flash();
    fixture();
    struct hpclang_port_iterator *iterator = hpclang_port_iterator_open(0);
    assert(iterator);
    struct hpclang_object *object = hpclang_port_backup_get(0, (const uint8_t *)"PORTZERO", 8);
    assert(object);
    /* Existing snapshots must survive loss of the original mapping. */
    memset(memory, 0, sizeof(memory));
    unavailable = 1;
    struct hpclang_port_entry entry = {.size = sizeof(entry),
                                       .version = HPCLANG_PORT_ENTRY_VERSION};
    assert(hpclang_port_iterator_read(iterator, &entry) == 1);
    assert(entry.kind == HPCLANG_PORT_BACKUP && entry.name_length == 8);
    assert(memcmp(entry.name, "PORTZERO", 8) == 0);
    assert(hpclang_port_iterator_read(iterator, &entry) == 0);
    assert(hpclang_port_iterator_read(iterator, &entry) == 0);
    int64_t value;
    assert(hpclang_object_integer_i64(object, &value) == 0 && value == 123);
    hpclang_object_release(object);
    hpclang_port_iterator_close(iterator);
    hpclang_port_iterator_close(NULL);
    assert(!hpclang_port_iterator_open(0) && errno == EIO);
    unavailable = 0;
    fixture();
    port1_available = 1;
    memcpy(memory, memory + 0xfffc0, 54);
    iterator = hpclang_port_iterator_open(1);
    assert(iterator && hpclang_port_iterator_read(iterator, &entry) == 1);
    assert(entry.kind == HPCLANG_PORT_BACKUP && entry.name_length == 8);
    assert(hpclang_port_iterator_read(iterator, &entry) == 0);
    hpclang_port_iterator_close(iterator);
    object = hpclang_port_backup_get(1, (const uint8_t *)"PORTZERO", 8);
    assert(object && hpclang_object_integer_i64(object, &value) == 0 && value == 123);
    memory[53] ^= 1;
    assert(!hpclang_port_iterator_open(1) && errno == EILSEQ);
    /* A Port 1 snapshot is independent of both native mapping and bank state. */
    port1_available = 0;
    memset(memory, 0, 54);
    assert(hpclang_object_integer_i64(object, &value) == 0 && value == 123);
    hpclang_object_release(object);
    assert(!hpclang_port_backup_get(0, (const uint8_t *)"portzero", 8) && errno == ENOENT);
    assert(!hpclang_port_backup_get(0, NULL, 1) && errno == EINVAL);
    assert(!hpclang_port_iterator_open(3) && errno == EINVAL);
    assert(!hpclang_port_iterator_open(1) && errno == ENOSYS);
    assert(!hpclang_port_iterator_open(2) && errno == ENOSYS);
    boot.saturn_state = 0;
    assert(!hpclang_port_iterator_open(0) && errno == ENOSYS);
    boot.saturn_state = 1;
    device.family = HPCLANG_FAMILY_HP39;
    assert(!hpclang_port_iterator_open(0) && errno == ENOSYS);
    device.family = HPCLANG_FAMILY_HP48;
    iterator = hpclang_port_iterator_open(0);
    assert(iterator);
    entry.version = 0;
    assert(hpclang_port_iterator_read(iterator, &entry) == -1 && errno == EINVAL);
    entry.version = HPCLANG_PORT_ENTRY_VERSION;
    assert(hpclang_port_iterator_read(iterator, &entry) == 1);
    hpclang_port_iterator_close(iterator);
    memory[0xfffc0 + 15] ^= 1;
    assert(!hpclang_port_iterator_open(0) && errno == EILSEQ);
    assert(!hpclang_port_backup_get(0, (const uint8_t *)"PORTZERO", 8) && errno == EILSEQ);
    memset(memory, 0, sizeof(memory));
    assert(!hpclang_port_iterator_open(0) && errno == EILSEQ);
    fixture();
    memory[0xfffc0] = 0;
    memory[0xfffc1] = 0;
    memory[0xfffc2] = 0;
    memory[0xfffc3] = 0;
    memory[0xfffc4] = 0;
    iterator = hpclang_port_iterator_open(0);
    assert(iterator && hpclang_port_iterator_read(iterator, &entry) == 0);
    hpclang_port_iterator_close(iterator);
    assert(!hpclang_port_library_get(0, 123) && errno == ENOENT);
    assert(!hpclang_port_library_get(0, 0x1000) && errno == EINVAL);
    const char *library = "04b2062000108510b7000000000000000000000d5c2";
    for (uint32_t i = 0; library[i]; ++i)
        memory[0xfffc0 + i] =
            (uint8_t)(library[i] <= '9' ? library[i] - '0' : library[i] - 'a' + 10);
    memset(memory + 0xfffc0 + 43, 0, 5);
    object = hpclang_port_library_get(0, 123);
    assert(object);
    memcpy(memory, memory + 0xfffc0, 48);
    port1_available = 1;
    device.family = HPCLANG_FAMILY_HP4950;
    struct hpclang_object *port1_library = hpclang_port_library_get(1, 123);
    assert(port1_library && hpclang_object_kind(port1_library) == HPCLANG_OBJECT_LIBRARY);
    port1_available = 0;
    device.family = HPCLANG_FAMILY_HP48;
    memset(memory, 0, sizeof(memory));
    assert(hpclang_object_kind(object) == HPCLANG_OBJECT_LIBRARY);
    assert(hpclang_object_kind(port1_library) == HPCLANG_OBJECT_LIBRARY);
    hpclang_object_release(port1_library);
    struct hpclang_object *library_snapshot = object;
    fixture();
    object = hpclang_port_backup_get(0, (const uint8_t *)"PORTZERO", 8);
    assert(object);
    memset(memory, 0, sizeof(memory));
    put_word(0x806ee, 0x84000);
    put_word(0x806f3, 0x85000);
    put_word(0x806f8, 0x90000);
    put_word(0x806fd, 0x90005);
    put_word(0x80e9b, (0x90000 - 0x85000) / 5);
    put_word(0x80711, 0x92000);
    put_word(0x8071b, 0x92000);
    put_word(0x92000, 0x02a96);
    put_word(0x80716, 0x9200d);
    assert(hpclang_port_backup_create(0, (const uint8_t *)"NEW", 3, object) == 0);
    assert(hpclang_port_backup_create(0, (const uint8_t *)"NEW", 3, object) == -1 &&
           errno == EEXIST);
    struct hpclang_object *copy = hpclang_port_backup_get(0, (const uint8_t *)"NEW", 3);
    assert(copy && hpclang_object_integer_i64(copy, &value) == 0 && value == 123);
    hpclang_object_release(copy);
    struct hpclang_object *replacement = NULL;
    assert(hpclang_object_create_string((const uint8_t *)"replacement", 11, &replacement) == 0);
    assert(hpclang_port_backup_replace(0, (const uint8_t *)"NEW", 3, replacement) == 0);
    copy = hpclang_port_backup_get(0, (const uint8_t *)"NEW", 3);
    assert(copy && hpclang_object_kind(copy) == HPCLANG_OBJECT_STRING);
    hpclang_object_release(copy);
    assert(hpclang_port_backup_replace(0, (const uint8_t *)"NEW", 3, object) == 0);
    copy = hpclang_port_backup_get(0, (const uint8_t *)"NEW", 3);
    assert(copy && hpclang_object_integer_i64(copy, &value) == 0 && value == 123);
    hpclang_object_release(copy);
    hpclang_object_release(replacement);
    assert(hpclang_port_backup_remove(0, (const uint8_t *)"NEW", 3) == 0);
    assert(!hpclang_port_backup_get(0, (const uint8_t *)"NEW", 3) && errno == ENOENT);
    assert(hpclang_port_backup_remove(0, (const uint8_t *)"NEW", 3) == -1 && errno == ENOENT);
    assert(hpclang_port_backup_replace(0, (const uint8_t *)"NEW", 3, object) == -1 &&
           errno == ENOENT);
    assert(hpclang_port_backup_remove(0, NULL, 1) == -1 && errno == EINVAL);
    assert(hpclang_port_backup_remove(3, NULL, 0) == -1 && errno == EINVAL);
    assert(hpclang_port_backup_remove(1, NULL, 0) == -1 && errno == ENOSYS);
    assert(hpclang_port_backup_replace(2, NULL, 0, object) == -1 && errno == ENOSYS);
    boot.saturn_state = 0;
    assert(hpclang_port_backup_remove(0, NULL, 0) == -1 && errno == ENOSYS);
    boot.saturn_state = 1;
    assert(hpclang_port_backup_create(0, NULL, 1, object) == -1 && errno == EINVAL);
    assert(hpclang_port_backup_create(3, NULL, 0, object) == -1 && errno == EINVAL);
    assert(hpclang_port_backup_create(1, NULL, 0, object) == -1 && errno == ENOSYS);
    assert(hpclang_port_backup_create(0, NULL, 0, NULL) == -1 && errno == EINVAL);
    device.family = HPCLANG_FAMILY_HP4950;
    port1_available = 1;
    check_port1_terminal(object, library_snapshot);
    memory[port1_limit] = 11;
    assert(hpclang_port_backup_create(1, (const uint8_t *)"ERAM", 4, object) == 0);
    copy = hpclang_port_backup_get(1, (const uint8_t *)"ERAM", 4);
    assert(copy && hpclang_object_integer_i64(copy, &value) == 0 && value == 123);
    hpclang_object_release(copy);
    assert(memory[port1_limit] == 11);
    assert(hpclang_port_backup_create(1, (const uint8_t *)"ERAM", 4, object) == -1 &&
           errno == EEXIST);
    assert(hpclang_port_backup_replace(1, (const uint8_t *)"ERAM", 4, object) == -1 &&
           errno == ENOSYS);
    port1_limit = 0;
    assert(hpclang_port_backup_create(1, (const uint8_t *)"OTHER", 5, object) == -1 &&
           errno == ENOSPC);
    writable = 0;
    assert(hpclang_port_backup_create(0, NULL, 0, object) == -1 && errno == ENOSYS);
    assert(hpclang_port_library_create(0, library_snapshot) == -1 && errno == ENOSYS);
    writable = 1;
    assert(hpclang_port_library_create(0, NULL) == -1 && errno == EINVAL);
    assert(hpclang_port_library_create(0, object) == -1 && errno == EINVAL);
    assert(hpclang_port_library_create(3, library_snapshot) == -1 && errno == EINVAL);
    assert(hpclang_port_library_create(1, library_snapshot) == -1 && errno == ENOSPC);
    port1_available = 0;
    assert(hpclang_port_library_create(1, library_snapshot) == -1 && errno == ENOSYS);
    port1_available = 1;
    port1_limit = 0x2000;
    assert(hpclang_port_library_create(2, library_snapshot) == -1 && errno == ENOSYS);
    boot.saturn_state = 0;
    assert(hpclang_port_library_create(0, library_snapshot) == -1 && errno == ENOSYS);
    boot.saturn_state = 1;
    library_snapshot->data[21] ^= 1;
    assert(hpclang_port_library_create(0, library_snapshot) == -1 && errno == EILSEQ);
    library_snapshot->data[21] ^= 1;
    assert(hpclang_port_library_create(0, library_snapshot) == 0);
    assert(hpclang_port_library_create(0, library_snapshot) == -1 && errno == EEXIST);
    assert(hpclang_port_library_create(1, library_snapshot) == 0);
    assert(hpclang_port_library_create(1, library_snapshot) == -1 && errno == EEXIST);
    copy = hpclang_port_backup_get(1, (const uint8_t *)"ERAM", 4);
    assert(copy && hpclang_object_integer_i64(copy, &value) == 0 && value == 123);
    hpclang_object_release(copy);
    copy = hpclang_port_library_get(1, 123);
    assert(copy && copy->nibble_count == library_snapshot->nibble_count);
    assert(!memcmp(copy->data, library_snapshot->data, (copy->nibble_count + 1) / 2));
    hpclang_object_release(copy);
    copy = hpclang_port_library_get(0, 123);
    assert(copy && copy->nibble_count == library_snapshot->nibble_count);
    assert(!memcmp(copy->data, library_snapshot->data, (copy->nibble_count + 1) / 2));
    hpclang_object_release(copy);
    port2_available = 1;
    memset(flash, 15, sizeof flash);
    for (unsigned bank = 0; bank < HPCLANG_RPL_PORT2_BANKS; ++bank) {
        flash[bank][0x200] = 0;
        flash[bank][0x202] = bank + 8;
        flash[bank][0x203] = 0;
    }
    assert(hpclang_port_library_create(2, library_snapshot) == 0);
    assert(hpclang_port_library_create(2, library_snapshot) == -1 && errno == EEXIST);
    copy = hpclang_port_library_get(2, 123);
    assert(copy && copy->nibble_count == library_snapshot->nibble_count);
    assert(!memcmp(copy->data, library_snapshot->data, (copy->nibble_count + 1) / 2));
    hpclang_object_release(library_snapshot);
    assert(hpclang_object_kind(copy) == HPCLANG_OBJECT_LIBRARY);
    hpclang_object_release(copy);
    hpclang_object_release(object);
    return 0;
}
