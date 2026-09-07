#!/usr/bin/env python3
"""Prepare a pinned x50ng checkout for one hpclang headless E2E frontend."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def replace_once(path: Path, old: str, new: str, what: str) -> None:
    text = path.read_text()
    if old not in text:
        raise SystemExit(f"{path}: {what} anchor changed")
    path.write_text(text.replace(old, new, 1))


def patch_host_uart0(root: Path) -> None:
    path = root / "src" / "s3c2410" / "s3c2410_uart.c"

    replace_once(
        path,
        "#include <fcntl.h>\n#include <sys/mman.h>",
        "#include <fcntl.h>\n#include <errno.h>\n#include <sys/mman.h>",
        "UART errno include",
    )
    replace_once(
        path,
        '#include "../module.h"\n',
        '#include "../module.h"\n#include "../timer.h"\n',
        "UART timer include",
    )
    replace_once(
        path,
        "    hdw_t* hdw_state;\n} s3c2410_uart_reg_t;",
        """    hdw_t* hdw_state;

    int host_fd;
    hdw_timer_t* host_timer;
} s3c2410_uart_reg_t;""",
        "UART host state",
    )

    replace_once(
        path,
        "static uint32_t s3c2410_uart_read( void* opaque, target_phys_addr_t offset )\n",
        """static void s3c2410_uart_host_poll( void* opaque )
{
    s3c2410_uart_reg_t* uart_regs = opaque;
    unsigned char byte;
    ssize_t count;

    if ( uart_regs->host_fd >= 0 && !( uart_regs->utrstat & 1 ) ) {
        count = read( uart_regs->host_fd, &byte, 1 );
        if ( count == 1 ) {
            uart_regs->urxh = byte;
            uart_regs->utrstat |= 1;
            if ( uart_regs->ucon & ( 1 << 8 ) )
                s3c2410_intc_sub_assert( uart_regs->hdw_state, uart_regs->int_rxd, 1 );
            else
                s3c2410_intc_sub_assert( uart_regs->hdw_state, uart_regs->int_rxd, 0 );
        }
    }

    if ( uart_regs->host_timer )
        timer_mod( uart_regs->host_timer, timer_get_clock() + 1000 );
}

static uint32_t s3c2410_uart_read( void* opaque, target_phys_addr_t offset )
""",
        "UART host poll",
    )
    replace_once(
        path,
        """        case S3C2410_UART0_UTXH:
            if ( uart_regs->ucon & ( 1 << 9 ) )""",
        """        case S3C2410_UART0_UTXH:
            if ( uart_regs->host_fd >= 0 ) {
                unsigned char byte = ( unsigned char )data;
                ( void )write( uart_regs->host_fd, &byte, 1 );
            }

            if ( uart_regs->ucon & ( 1 << 9 ) )""",
        "UART host transmit",
    )
    replace_once(
        path,
        """    cpu_register_physical_memory( S3C2410_UART0_BASE, S3C2410_MAP_SIZE, iotype );

    return 0;""",
        """    uint32_t uart_base =
        uart_regs->int_rxd == SUB_INT_RXD0 ? S3C2410_UART0_BASE :
        uart_regs->int_rxd == SUB_INT_RXD1 ? S3C2410_UART1_BASE :
                                             S3C2410_UART2_BASE;
    cpu_register_physical_memory( uart_base, 0x4000, iotype );

    if ( uart_regs->int_rxd == SUB_INT_RXD0 ) {
        const char* host_path = getenv( "HPCLANG_X50NG_UART0" );
        if ( host_path && *host_path ) {
            uart_regs->host_fd =
                open( host_path, O_RDWR | O_NONBLOCK | O_NOCTTY );
            if ( uart_regs->host_fd < 0 ) {
                fprintf(
                    stderr,
                    "s3c2410-uart0: cannot open host transport %s: %s\\n",
                    host_path,
                    strerror( errno ) );
                return -errno;
            }
            uart_regs->host_timer =
                timer_new( HDW_TIMER_REALTIME, s3c2410_uart_host_poll, uart_regs );
            if ( uart_regs->host_timer == NULL )
                return -ENOMEM;
            timer_mod( uart_regs->host_timer, timer_get_clock() + 1000 );
        }
    }

    return 0;""",
        "UART host open",
    )
    replace_once(
        path,
        """    if ( module->user_data ) {
        uart_regs = module->user_data;
        if ( uart_regs->regs )
            free( uart_regs->regs );
    }""",
        """    if ( module->user_data ) {
        uart_regs = module->user_data;
        if ( uart_regs->host_timer ) {
            timer_del( uart_regs->host_timer );
            timer_free( uart_regs->host_timer );
        }
        if ( uart_regs->host_fd >= 0 )
            close( uart_regs->host_fd );
        if ( uart_regs->regs )
            free( uart_regs->regs );
    }""",
        "UART host close",
    )
    replace_once(
        path,
        """    memset( uart, 0, sizeof( s3c2410_uart_t ) );

    if ( s3c2410_uart_data_init( uart ) ) {""",
        """    memset( uart, 0, sizeof( s3c2410_uart_t ) );
    for ( int i = 0; i < 3; ++i )
        uart->uart[ i ].host_fd = -1;

    if ( s3c2410_uart_data_init( uart ) ) {""",
        "UART host fd init",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("x50ng", type=Path)
    parser.add_argument("--frontend", type=Path, required=True)
    parser.add_argument("--headless-header", type=Path, required=True)
    parser.add_argument("--little-apple", action="store_true")
    parser.add_argument("--hp48gii", action="store_true")
    parser.add_argument("--host-uart0", action="store_true")
    parser.add_argument("--reserve-ram", action="store_true")
    args = parser.parse_args()

    root = args.x50ng
    shutil.copy2(args.frontend, root / "src" / "main.c")
    shutil.copy2(args.headless_header, root / "src" / "headless-ui-init.h")

    if args.little_apple:
        replace_once(
            root / "src" / "flash.c",
            'memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOUPDATEIMAGE", 16 ) != 0',
            '''memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOUPDATEIMAGE", 16 ) != 0 &&
                         memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOHP39G+IMAGE", 16 ) != 0 &&
                         memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOHP40G+IMAGE", 16 ) != 0''',
            "Little-Apple firmware header",
        )

    if args.hp48gii:
        replace_once(
            root / "src" / "sram.c",
            "#define S3C2410_SRAM_SIZE 0x00080000",
            "#define S3C2410_SRAM_SIZE 0x00040000",
            "HP48gII SRAM mapping size",
        )
        replace_once(
            root / "src" / "sram.c",
            "    sram->size = 0x00080000;",
            "    sram->size = S3C2410_SRAM_SIZE;",
            "HP48gII SRAM backing size",
        )
        replace_once(
            root / "src" / "flash.c",
            'memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOUPDATEIMAGE", 16 ) != 0',
            '''memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOUPDATEIMAGE", 16 ) != 0 &&
                         memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOHP48GIIMAGE", 16 ) != 0''',
            "HP48gII firmware header",
        )

    if args.host_uart0:
        patch_host_uart0(root)

    if args.reserve_ram:
        replace_once(
            root / "src" / "module.c",
            "    phys_ram_base = mmap( 0, phys_ram_size, PROT_NONE,",
            "    /* hpclang E2E reserved RAM. */\n"
            "    phys_ram_size += 0x00100000;\n\n"
            "    phys_ram_base = mmap( 0, phys_ram_size, PROT_NONE,",
            "reserved RAM",
        )


if __name__ == "__main__":
    main()
