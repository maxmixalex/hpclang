#include <stdint.h>
#include <hpclang/abi.h>
#include <hpclang/port.h>
#include <hpclang/target.h>

uint32_t hpclang_e2e_port_discovery(void)
{
    /* Reserve 0x5xx for discovery; the low nibble identifies the failing port.
     * Classic mutation uses 0xEx, so sharing those codes hides the failing stage. */
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (!boot)
        return 0x500u;
    const uint32_t reads =
        HPCLANG_PORT_ENUMERATE | HPCLANG_PORT_READ_BACKUP | HPCLANG_PORT_READ_LIBRARY;
    const uint32_t writes =
        HPCLANG_PORT_CREATE_BACKUP | HPCLANG_PORT_REPLACE_BACKUP | HPCLANG_PORT_REMOVE_BACKUP;
    for (uint32_t port = 0; port < 3; ++port) {
        struct hpclang_port_info info = {.size = sizeof(info),
                                         .version = HPCLANG_PORT_INFO_VERSION};
        if (hpclang_port_query(port, &info))
            return 0x510u | port;
        uint32_t expected = 0, medium = HPCLANG_PORT_ABSENT;
        if (boot->model >= HPCLANG_MODEL_HP48GII && port == 0) {
            expected = reads | writes | HPCLANG_PORT_CREATE_LIBRARY;
            medium = HPCLANG_PORT_MAIN_RAM;
        } else if (boot->model == HPCLANG_MODEL_HP49GPLUS || boot->model == HPCLANG_MODEL_HP50G) {
            expected = reads | HPCLANG_PORT_CREATE_BACKUP | HPCLANG_PORT_CREATE_LIBRARY;
            if (port == 2)
                expected |= HPCLANG_PORT_REMOVE_BACKUP | HPCLANG_PORT_REPLACE_BACKUP;
            medium = port == 1 ? HPCLANG_PORT_EXTENDED_RAM : HPCLANG_PORT_FLASH;
        }
        if (info.port != port || info.medium != medium)
            return 0x520u | port;
        if (info.implemented != expected)
            return 0x530u | port;
        if (info.available != (boot->saturn_state ? expected : 0u))
            return 0x540u | port;
    }
    return 0;
}
