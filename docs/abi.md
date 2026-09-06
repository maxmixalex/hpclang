# hpclang v1 application ABI

The v1 ABI describes what an application sees after the calculator-specific
launcher has completed its work.

## CPU ABI

Application code uses:

```text
--target=arm-none-eabi
-march=armv4t
-mabi=aapcs
-mfloat-abi=soft
-mthumb
```

ARM startup code may be used where useful. ARM/Thumb interworking is part of
the normal ARMv4T build.

Applications use fixed virtual addresses and ordinary non-PIC code. There is
no application-visible r9/r10 static-base contract.

## Virtual layout

```text
0x01100000  boot page, 4 KiB
0x01101000  initialized writable data
            BSS
            heap
            free writable pages
            stack grows down from the top

0x01220000  executable image
            text / rodata
            load image for initialized writable data
```

The virtual addresses are common across calculator families. The platform
launcher chooses the physical pages that back them.

## Boot record

```c
struct hpclang_boot_info {
    uint32_t model;
    uint32_t rw_bytes;
    uintptr_t saturn_state;
    uintptr_t platform_cookie;
};
```

`platform_cookie` is opaque to applications.

## HPC1 image

The model-independent post-link container begins with:

```c
struct hpclang_image_header {
    uint8_t magic[4];       /* "HPC1" */
    uint32_t image_size;
    uint32_t entry_offset;
    uint32_t rw_static_size;
};
```

The header is followed by the initialized code/load image. HPC1 contains no
calculator target ID, historical launcher flags or secondary entry table.

## Entry and return

The platform launcher maps the code and writable arena, writes the boot record,
provides an 8-byte-aligned stack and enters the HPC1 entry point.

The common runtime initializes language state, calls `main()`, tears down the
runtime, restores temporary mappings and returns through the platform launcher.

## Stable model IDs

| ID | Model |
| ---: | --- |
| 0 | unknown |
| 1 | HP 39g+ |
| 2 | HP 39gs |
| 3 | HP 40gs |
| 4 | HP 48gII |
| 5 | HP 49g+ |
| 6 | HP 50g |

Existing IDs are ABI values and must not be renumbered.

## Packaging

HPC1 is not itself an HP39 aplet, RPL object or HP transfer file. Each family
backend wraps the same logical image in the calculator-native package required
by that model. Those outer formats are not visible to application code.
