# Architecture

hpclang uses one application ABI across all supported calculators. A normal application is compiled with Clang/LLVM for ARMv4T, uses AAPCS32 and soft float, and is packaged as an HPC1 image. The normal build does not depend on GCC, libgcc, historical HPGCC objects, or a replacement calculator ROM.

```text
application
    |
hpclang public API
    |
common runtime + libc + compiler builtins
    |
HPC1
    |
+-----------+-----------+-----------+
|           |           |           |
hp39        hp48        hp4950
|           |           |
39g+        48gII       49g+
39gs                    50g
40gs
```

The three platform families describe implementation differences that affect launch, packaging, memory setup, and stock-system integration. They are not separate application ABIs.

## Common runtime

The common runtime owns startup, initialized data, BSS, static TLS, constructors, compiler helpers, heap setup, libc integration, entry into `main()`, and cleanup on return.

The common bootstrap receives calculator-specific launch information, establishes the fixed HPC1 virtual address layout, enters the application, restores temporary mappings, and returns to the calculator OS.

## Platform families

`hp39` covers HP 39g+, HP 39gs, and HP 40gs. These models use the HP39 aplet/container path and ARMToolbox39/library 275.

`hp48` currently contains HP 48gII. It uses the dedicated historical ARMToolbox48 path and has a 131×64 display and 256 KiB physical RAM in the validated full-NOR fixture.

`hp4950` covers HP 49g+ and HP 50g. The two models share the same hpclang platform implementation and the same official 2.15 OS image in E2E, but retain their distinct native ARM bootloaders.

Model identity remains available through `hpclang_boot_info.model`. Code should use it only when a real hardware or stock-system difference requires model-specific behavior.

## Platform services

Display, keyboard, timers, storage, and calculator-object integration will be added behind common APIs. The project does not reproduce the historical HPGCC API wholesale; services are added when current applications require them.

## ROM policy

The supported SDK must run through ordinary calculator software mechanisms. Replacement ROMs and ROM patching may be useful for research, but they are not part of the normal hpclang execution model.
