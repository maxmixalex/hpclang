# HP39gs real-hardware validation

This is the authoritative execution gate for the HP39/40-family backend. It
validates the same HPC1 ABI/runtime that is also exercised by automated ARM
execution CI on other S3C2410-family targets.

## Requirements

- an HP39gs;
- its normal 5-pin mini-USB PC cable;
- HP 39/40 Connectivity Kit / Conn3x 2.x;
- the `hpclang-hp39-hardware-validation` artifact from the `HP39 package`
  workflow.

The HP39gs supports PC transfer over USB. Its separate serial port is not
required for this test.

## Bundle layout

The package intentionally preserves the stock HPGCC39 transfer names:

- `HPGCC000.000` — `HPCLANG HPC1 TEST` Aplet;
- `LIB275L.000` — ARMToolbox library 275;
- `HP39DIR.000` and `HP39DIR.CUR` — Connectivity Kit disk-drive metadata;
- `E2E_RESULT_ADDRESS.txt`, `BUILD_INFO.txt`, `SHA256SUMS.txt` — test
  metadata.

Do not rename the calculator transfer files for validation.

## Direct USB transfer

1. Extract the artifact into its own directory.
2. Start Conn3x and connect the powered-on HP39gs over USB.
3. On the calculator open the APLET library and press `RECV`.
4. Choose `HP39/40 (USB)`.
5. If library 275 is not already installed, transfer `LIB275L.000` once.
6. Repeat `APLET -> RECV -> HP39/40 (USB)` and transfer
   `HPGCC000.000`.
7. Confirm that `HPCLANG HPC1 TEST` appears in the APLET library.
8. Launch it.

The Connectivity Kit's `USB DISK DRIVE` mode is also valid: point Conn3x at
the extracted directory and let the calculator browse the generated
`HP39DIR.*` metadata.

## Expected result

The current package runs the canonical runtime validation. It briefly leaves a
machine-readable and visible result in the framebuffer, then returns to the
stock OS.

Success:

```text
RUNTIME PASS
```

Failure:

```text
RUNTIME FAIL
CODE xx
```

After the result, APLET/ENTER/ON must still respond. A stale result page until
the stock UI redraws is acceptable; a frozen keyboard, reset, ADDRESS ERROR or
persistent OS-state damage is not.

## First real-hardware observation

The first HP39gs run of commit `4c4db3d` reached:

```text
HPC1 PASS
MODEL 00000002
RW 00014000
APLET TO E IT
```

This proves real-hardware entry through the stock Aplet, stage0, stage1, common
MMU/bootstrap and HPC1 runtime. The blank character in `EXIT` was a test-font
bug: the tiny smoke font did not define `X`.

That version then remained in its APLET polling loop with IRQ/FIQ masked. Since
the loop never returned, it did not exercise the common bootstrap's return/MMU
restore path. The next gate therefore removes keyboard polling entirely and
uses timed automatic return.


## Second observation and correction

The second hardware build displayed `A TO EXIT` and appeared unresponsive.
The missing `U` was another omission in the tiny diagnostic font. More
importantly, that build still masked IRQ/FIQ and executed a raw
`delay(60000000)`; therefore APLET/ON being unresponsive during the page was
expected, and the actual delay duration was not calibrated on real hardware.

That build is not used as return-path evidence. The next gate removes the
display, interrupt masking, GPIO access and delay entirely and immediately
returns from `main()`.


## Third observation: return gate passed

The side-effect-free immediate-return build returned successfully on real
HP39gs hardware. The LCD continued to show the previous diagnostic contents,
which initially looked like a hang, but the calculator responded to ENTER/ON
and APLET opened the normal APLET menu.

This is a PASS for the return gate: CPU control, interrupt handling and the
stock calculator UI were live again. The stale LCD contents only show that the
stock UI does not force an immediate full repaint when the ARM Aplet returns.
Treat repaint/dirty-screen integration as a later HP39 platform-service issue.


## Canonical runtime validation

After the entry/return gate passed, the hardware package was upgraded to the
canonical runtime validation. It leaves its result in the framebuffer and then
returns immediately to the stock OS.

Success:

```text
RUNTIME PASS
```

Failure:

```text
RUNTIME FAIL
CODE xx
```

Failure codes:

| Code | Check |
| --- | --- |
| 01 | boot info / model / writable arena |
| 02 | initialized global data |
| 03 | BSS zeroing |
| 04 | initial static TLS (.tdata/.tbss) |
| 05 | static TLS read/write |
| 06 | init-array constructor |
| 07 | compiler-rt 32-bit division |
| 08 | compiler-rt 64-bit division |
| 09 | compiler-rt software floating point |
| 0A | picolibc memcpy/memset/strlen/strcmp |
| 0B | malloc allocation |
| 0C | heap memory read/write |
| 0D | snprintf formatting |

After either result appears, press APLET. Opening the normal APLET library
confirms that the result was left by a program that has already returned rather
than by a hung HPC1 process.


## Final-result visibility correction

With the target-built ARMv4T double soft-float build, the real HP39gs no longer
stopped at a checkpoint: ENTER caused only a short busy interval and the stock
OS remained responsive afterwards, including a working APLET menu.

That run is strong execution evidence but not yet a visually explicit PASS:
the diagnostic startup wrapper overwrote `RUNTIME PASS/FAIL` with teardown
steps `F0` and `FF` immediately before returning.

The current test no longer writes teardown checkpoints. It leaves
`RUNTIME PASS` or `RUNTIME FAIL / CODE xx` visible for a short finite hold,
then returns automatically. No key press is required during the result page.


## HP40gs sibling validation

HP40gs uses the same S3C2410A/ARM920T generation, 256 KiB RAM and 131x64 LCD as
HP39gs, but has a different stock ROM/CAS image, 2 MiB flash, no infrared, and a
distinct hpclang model ID.

The HP39gs hardware run is sufficient evidence for the common HP39/40 ARM
runtime implementation. HP40gs support still requires one target-specific
closure test:

1. build/package the same HPC1 runtime for model 3;
2. transfer it through the stock HP40gs Connectivity Kit/Aplet path;
3. observe model 3 and `RUNTIME PASS`;
4. verify clean return to the HP40gs stock OS.

That check may be done on real HP40gs hardware or a future emulator that truly
executes the underlying ARM920T code. A Saturn-only simulator is not an
execution substitute.
