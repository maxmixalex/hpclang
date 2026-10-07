# hpclang v1 application ABI

The v1 ABI describes what an application sees after the calculator-specific launcher has completed its work.

## CPU ABI

Application code uses:

```text
--target=arm-none-eabi
-march=armv4t
-mabi=aapcs
-mfloat-abi=soft
-mthumb
```

ARM startup code may be used where useful. ARM/Thumb interworking is part of the normal ARMv4T build.

Applications use fixed virtual addresses and ordinary non-PIC code. There is no application-visible historical r9/r10 static-base contract.

## Virtual layout

```text
0x01100000  boot page, 4 KiB
0x01101000  initialized writable data
            BSS
            heap
            free writable pages
            stack grows down from the top

0x01200000  bootstrap/runtime-private code-window prefix
0x01220000  executable image
            text / rodata
            load image for initialized writable data
```

The virtual addresses are common across calculator families. The platform launcher chooses the physical pages that back them.

The last private prefix page (`0x0121F000`) is a read-only, uncached alias of the
active L2 workspace. Runtime DMA validation reads it without assuming that a
physical RAM identity mapping exists. Private asset mappings occupy at most the
preceding 31 pages. The alias disappears with the normal code-table restoration;
this adds no public boot-info field and no extra physical-page reservation.

`0x01200000..0x0121FFFF` is the otherwise-unused prefix of the same coarse code-table section whose public application image begins at `0x01220000`. Bootstrap/platform code may use this prefix for short private mappings needed by runtime services; it is **not application storage and not an application-visible mapping contract**. HP39gs/40gs currently use the first two 4 KiB pages of this prefix for read-only native-font NOR access. Applications must not address or depend on that implementation detail.

## Boot record

```c
struct hpclang_boot_info {
    uint32_t model;
    uint32_t rw_bytes;
    uintptr_t saturn_state;
    uintptr_t platform_cookie;
};
```

`platform_cookie` is opaque to applications. A launcher/runtime pair may use it for a private platform token, including the base of a launcher-private mapping, but application code must not infer a pointer type, address range, or stable meaning from it.

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

The header is followed by the initialized code/load image. HPC1 contains no calculator target ID, historical launcher flags, or secondary entry table.

This is intentional: the same logical application image can be wrapped and launched by different calculator profiles without recompiling application code for a historical launcher ABI.

## Entry and return

The platform launcher maps the code and writable arena, writes the boot record, provides an 8-byte-aligned stack, and enters the HPC1 entry point.

The common runtime initializes language state, calls `main()`, tears down the runtime, restores temporary mappings and privileged state, and returns through the platform launcher.

Bootstrap-private mappings do not extend the application ABI: they exist only to implement platform/runtime services below the HPC1 boundary and are removed with the rest of the temporary launch state.

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

## Platform family IDs

The current implementation groups models into `hp39`, `hp48`, and `hp4950`. Family identity describes the launcher/platform implementation, not the public HPC1 image format.

## Packaging

HPC1 is not itself an HP39 aplet, RPL object, or HP transfer file. Each platform wraps the same logical image in the calculator-native package required by that model. Those outer formats are not visible to application code.
