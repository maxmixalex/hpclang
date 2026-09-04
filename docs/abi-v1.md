# hpclang v1 ABI

## Goal

The v1 ABI is intentionally small. It describes what an hpclang application
sees after the calculator-specific launcher has finished its work.

It does not expose HPGCC launcher flags, historical PIC registers, ROMLIB
versions or calculator storage formats.

## CPU ABI

Application code uses:

```
--target=arm-none-eabi
-march=armv4t
-mabi=aapcs
-mfloat-abi=soft
-mthumb
```

Assembly startup may use ARM state. Interworking is part of the normal ARMv4T
build, not a separate compatibility mode.

Applications are linked at fixed virtual addresses, so ordinary non-PIC code
is used. There is no r9/r10 static-base contract.

## Virtual layout

```
0x01100000  boot page, 4 KiB
0x01101000  initialized writable data
            BSS
            heap
            free writable pages
            stack grows down from top

0x01220000  executable image
            text
            rodata
            load image for initialized writable data
```

The addresses are deliberately common across all target families. Physical
memory backing them is a platform-launcher concern.

## Entry

The platform launcher:

1. maps the executable image at `HPCLANG_CODE_BASE`;
2. maps the writable arena at `HPCLANG_BOOT_BASE`;
3. writes `struct hpclang_boot_info` at the start of the boot page;
4. provides an 8-byte-aligned stack at the top of the writable arena;
5. enters the image entry point.

The common ARM `_start` then:

1. copies initialized writable data from its load image to
   `HPCLANG_DATA_BASE`;
2. clears BSS;
3. switches into normal Thumb C code;
4. calls `main()`;
5. returns to the platform launcher.

Hardware state ownership belongs to the platform launcher/runtime boundary,
not to application startup.

## Boot info

The v1 boot record is intentionally small:

```c
struct hpclang_boot_info {
    uint32_t model;
    uint32_t rw_bytes;
    uintptr_t saturn_state;
    uintptr_t platform_cookie;
};
```

`platform_cookie` is opaque to applications. The common runtime may use it
when implementing platform services.

## HPC1 logical image

The model-independent post-link container begins with:

```c
struct hpclang_image_header {
    uint8_t magic[4];       // "HPC1"
    uint32_t image_size;
    uint32_t entry_offset;
    uint32_t rw_static_size;
};
```

The header is followed immediately by the initialized code/load image.

There is no target ID, feature bitmap, old-launcher flag word or secondary
entry table in v1. A platform backend either implements the hpclang v1 ABI or
it does not.

## Packaging

HPC1 is not itself an HP39 aplet, HP48/49 RPL object or HP transfer file.

Each platform packager wraps the same logical image in the calculator's native
container and supplies whatever alignment/storage metadata that platform
requires. Those outer formats are not visible to compiled application code.


## Stable model IDs

`hpclang_boot_info.model` is an ABI value, not an enum ordinal that may be
renumbered. The v1 assignments are fixed:

| ID | Model |
| ---: | --- |
| 0 | unknown |
| 1 | HP 39g+ |
| 2 | HP 39gs |
| 3 | HP 40gs |
| 4 | HP 48gII |
| 5 | HP 49g+ |
| 6 | HP 50g |

New models must receive new IDs; existing values must not be renumbered.
