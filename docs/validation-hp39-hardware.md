# HP39gs real-hardware validation

This is the preferred first manual execution gate for the HP39-family backend.
It validates the same generated HPC1 image that will later run under automated
Emu48 E2E.

## Requirements

- an HP39gs;
- its normal 5-pin mini-USB PC cable;
- HP 39/40 Connectivity Kit / Conn3x 2.x;
- the `hpclang-hp39-emulator-smoke` artifact from the `HP39 package`
  workflow.

The HP39gs supports PC transfer over USB. The calculator's separate serial port
and an Emu48 virtual COM setup are not required for this test.

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

The screen must show:

```text
HPC1 PASS
MODEL 00000002
RW <mapped byte count>
APLET TO EXIT
```

The RW count is informational and may vary with the page pool.

While this screen is visible, verify that the calculator has not reset or
frozen. Then press APLET once.

A pass requires a clean return to the stock APLET library with no ADDRESS
ERROR, corrupted LCD, reset, or persistent OS-state corruption.

## Report

For a pass, record the visible four lines and whether APLET returned cleanly.

For a failure, record the earliest boundary:

- Conn3x/USB connection failure;
- LIB275L transfer failure;
- Aplet transfer failure;
- Aplet is present but launch returns immediately;
- reset or ADDRESS ERROR before HPC1 output;
- partial/corrupt output;
- PASS reached but APLET exit fails;
- exit succeeds but the stock OS is corrupted.

A screenshot is useful but the exact failure boundary is more important.
