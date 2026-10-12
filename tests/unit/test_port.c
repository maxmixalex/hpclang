#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <hpclang/abi.h>
#include <hpclang/device.h>
#include <hpclang/port.h>
#include <hpclang/target.h>
#include "saturn-view.h"

static uint8_t memory[0x100000];
static int unavailable;
static struct hpclang_boot_info boot = {.saturn_state = 1};
static struct hpclang_device_info device = {.family = HPCLANG_FAMILY_HP4950};
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

static void fixture(void)
{
    const char *hex = "26b20130008005f42545a55425f48041620900003210119200f9e3";
    memset(memory, 0, sizeof(memory));
    for (uint32_t i = 0; i < 5; ++i)
        memory[0x80716 + i] = (0xfffc0u >> (4 * i)) & 15;
    for (uint32_t i = 0; hex[i]; ++i)
        memory[0xfffc0 + i] = (uint8_t)(hex[i] <= '9' ? hex[i] - '0' : hex[i] - 'a' + 10);
}

int main(void)
{
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
    return 0;
}
