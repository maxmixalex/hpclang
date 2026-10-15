#include <stdint.h>
#include <hpclang/abi.h>
#include <hpclang/port.h>
#include <hpclang/target.h>

uint32_t hpclang_e2e_port_discovery(void)
{
    const struct hpclang_boot_info *boot = hpclang_boot_info();
    if (!boot)
        return 0xe0u;
    const uint32_t reads =
        HPCLANG_PORT_ENUMERATE | HPCLANG_PORT_READ_BACKUP | HPCLANG_PORT_READ_LIBRARY;
    const uint32_t writes =
        HPCLANG_PORT_CREATE_BACKUP | HPCLANG_PORT_REPLACE_BACKUP | HPCLANG_PORT_REMOVE_BACKUP;
    for (uint32_t port = 0; port < 3; ++port) {
        struct hpclang_port_info info = {.size = sizeof(info),
                                         .version = HPCLANG_PORT_INFO_VERSION};
        if (hpclang_port_query(port, &info))
            return 0xe3u;
        uint32_t expected = 0, medium = HPCLANG_PORT_ABSENT;
        if (boot->model >= HPCLANG_MODEL_HP48GII && port == 0) {
            expected = reads | writes | HPCLANG_PORT_CREATE_LIBRARY;
            medium = HPCLANG_PORT_MAIN_RAM;
        } else if (boot->model == HPCLANG_MODEL_HP49GPLUS || boot->model == HPCLANG_MODEL_HP50G) {
            expected = reads | HPCLANG_PORT_CREATE_BACKUP;
            if (port == 2)
                expected |= HPCLANG_PORT_REMOVE_BACKUP | HPCLANG_PORT_REPLACE_BACKUP;
            else
                expected |= HPCLANG_PORT_CREATE_LIBRARY;
            medium = port == 1 ? HPCLANG_PORT_EXTENDED_RAM : HPCLANG_PORT_FLASH;
        }
        if (info.port != port || info.medium != medium || info.implemented != expected ||
            info.available != (boot->saturn_state ? expected : 0u))
            return 0xe4u;
    }
    return 0;
}
