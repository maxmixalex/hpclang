#!/usr/bin/env python3
"""Prepare one pinned x50ng build for all hpclang headless E2E modes/models."""

from __future__ import annotations

import argparse
import shutil
from pathlib import Path


def replace_once(path: Path, old: str, new: str, what: str) -> None:
    text = path.read_text()
    if old not in text:
        raise SystemExit(f"{path}: {what} anchor changed")
    path.write_text(text.replace(old, new, 1))


def copy_frontend(source: Path, destination: Path, entrypoint: str) -> None:
    text = source.read_text()
    old = "int main(int argc, char **argv)"
    if text.count(old) != 1:
        raise SystemExit(f"{source}: expected exactly one x50ng frontend main()")
    destination.write_text(text.replace(old, f"int {entrypoint}(int argc, char **argv)", 1))


def patch_ui_build(root: Path) -> None:
    makefile = root / "Makefile"
    lines = makefile.read_text().splitlines(keepends=True)
    matches = []
    for index, line in enumerate(lines):
        tokens = line.split()
        if (
            "libui4x.a" in line
            and "WITH_SDL=yes" in tokens
            and "WITH_SDL2=no" in tokens
        ):
            matches.append(index)
    if len(matches) != 1:
        raise SystemExit(
            f"{makefile}: expected one ui4x SDL selector line, found {len(matches)}"
        )

    index = matches[0]
    lines[index] = lines[index].replace(
        "WITH_SDL=yes", "WITH_SDL=$(WITH_SDL)", 1
    ).replace(
        "WITH_SDL2=no", "WITH_SDL2=$(WITH_SDL2)", 1
    )
    makefile.write_text("".join(lines))


def patch_universal_flash(root: Path) -> None:
    replace_once(
        root / "src" / "flash.c",
        'memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOUPDATEIMAGE", 16 ) != 0',
        '''memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOUPDATEIMAGE", 16 ) != 0 &&
                         memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOHP39G+IMAGE", 16 ) != 0 &&
                         memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOHP40G+IMAGE", 16 ) != 0 &&
                         memcmp( phys_ram_base + flash->offset + BOOT_SIZE, "KINPOHP48GIIMAGE", 16 ) != 0''',
        "universal HP39/40/48 firmware headers",
    )


def patch_runtime_sram(root: Path) -> None:
    path = root / "src" / "sram.c"

    replace_once(
        path,
        "#define S3C2410_SRAM_SIZE 0x00080000\n",
        '''#define S3C2410_SRAM_DEFAULT_SIZE 0x00080000
#define S3C2410_SRAM_HP48_SIZE 0x00040000

static size_t hpclang_sram_size( void )
{
    const char* text = getenv( "HPCLANG_X50NG_SRAM_SIZE" );
    char* end = NULL;
    unsigned long value;

    if ( text == NULL || *text == '\0' )
        return S3C2410_SRAM_DEFAULT_SIZE;

    errno = 0;
    value = strtoul( text, &end, 0 );
    if ( errno != 0 || end == text || *end != '\0' ||
         ( value != S3C2410_SRAM_DEFAULT_SIZE &&
           value != S3C2410_SRAM_HP48_SIZE ) ) {
        fprintf( stderr,
                 "hpclang-x50ng: unsupported SRAM size %s "
                 "(expected 0x40000 or 0x80000)\n",
                 text );
        return 0;
    }

    return ( size_t )value;
}
''',
        "runtime SRAM selector",
    )
    replace_once(
        path,
        "    sram->size = 0x00080000;\n",
        "    if ( sram->size == 0 ) return -EINVAL;\n",
        "SRAM load keeps runtime-selected size",
    )
    replace_once(
        path,
        '''    sram->data = ( void* )-1;
    sram->shadow = ( void* )-1;
    sram->offset = phys_ram_size;
    phys_ram_size += S3C2410_SRAM_SIZE;
    phys_ram_size += S3C2410_SRAM_SIZE;

    cpu_register_physical_memory( S3C2410_SRAM_BASE, S3C2410_SRAM_SIZE, sram->offset | IO_MEM_RAM );
    cpu_register_physical_memory( S3C2410_SRAM_BASE + S3C2410_SRAM_SIZE, S3C2410_SRAM_SIZE,
                                  ( sram->offset + S3C2410_SRAM_SIZE ) | IO_MEM_RAM );''',
        '''    sram->data = ( void* )-1;
    sram->shadow = ( void* )-1;
    sram->size = hpclang_sram_size();
    if ( sram->size == 0 ) {
        free( sram );
        module->user_data = NULL;
        return -EINVAL;
    }
    sram->offset = phys_ram_size;
    phys_ram_size += sram->size;
    phys_ram_size += sram->size;

    cpu_register_physical_memory( S3C2410_SRAM_BASE, sram->size, sram->offset | IO_MEM_RAM );
    cpu_register_physical_memory( S3C2410_SRAM_BASE + sram->size, sram->size,
                                  ( sram->offset + sram->size ) | IO_MEM_RAM );''',
        "runtime SRAM map size",
    )


def patch_host_uart0(root: Path) -> None:
    path = root / "src" / "s3c2410" / "s3c2410_uart.c"

    replace_once(
        path,
        '#include "../module.h"\n',
        '#include <errno.h>\n#include "../module.h"\n#include "../timer.h"\n',
        "UART host includes",
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
        timer_mod( uart_regs->host_timer, timer_get_clock() + 100 );
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
                    "s3c2410-uart0: cannot open host transport %s: %s\n",
                    host_path,
                    strerror( errno ) );
                return -errno;
            }
            uart_regs->host_timer =
                timer_new( HDW_TIMER_REALTIME, s3c2410_uart_host_poll, uart_regs );
            if ( uart_regs->host_timer == NULL )
                return -ENOMEM;
            timer_mod( uart_regs->host_timer, timer_get_clock() + 100 );
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


def patch_reserved_ram(root: Path) -> None:
    replace_once(
        root / "src" / "module.c",
        "    phys_ram_base = mmap( 0, phys_ram_size, PROT_NONE,",
        "    /* hpclang E2E reserved RAM, inert unless the Direct harness maps it. */\n"
        "    phys_ram_size += 0x00100000;\n\n"
        "    phys_ram_base = mmap( 0, phys_ram_size, PROT_NONE,",
        "reserved E2E RAM",
    )


def patch_frontends(root: Path, args: argparse.Namespace) -> None:
    copy_frontend(args.direct_frontend, root / "src" / "hpclang_direct.c", "hpclang_direct_main")
    copy_frontend(args.stock_frontend, root / "src" / "hpclang_stock.c", "hpclang_stock_main")
    shutil.copy2(args.selector, root / "src" / "main.c")
    shutil.copy2(args.headless_header, root / "src" / "headless-ui-init.h")

    makefile = root / "Makefile"
    lines = makefile.read_text().splitlines(keepends=True)
    matches = []
    for index, line in enumerate(lines):
        logical = line.strip()
        if logical.endswith("\\"):
            logical = logical[:-1].rstrip()
        if logical.endswith("src/main.c"):
            matches.append(index)
    if len(matches) != 1:
        raise SystemExit(
            f"{makefile}: expected one src/main.c source line, found {len(matches)}"
        )

    index = matches[0]
    original = lines[index]
    leading = original[: len(original) - len(original.lstrip())]
    continuation = chr(92)
    lines[index:index + 1] = [
        f"{leading}./src/hpclang_direct.c {continuation}\n",
        f"{leading}./src/hpclang_stock.c {continuation}\n",
        original,
    ]
    makefile.write_text("".join(lines))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("x50ng", type=Path)
    parser.add_argument("--direct-frontend", type=Path, required=True)
    parser.add_argument("--stock-frontend", type=Path, required=True)
    parser.add_argument("--selector", type=Path, required=True)
    parser.add_argument("--headless-header", type=Path, required=True)
    args = parser.parse_args()

    root = args.x50ng
    patch_frontends(root, args)
    patch_ui_build(root)
    patch_universal_flash(root)
    patch_runtime_sram(root)
    patch_host_uart0(root)
    patch_reserved_ram(root)


if __name__ == "__main__":
    main()
