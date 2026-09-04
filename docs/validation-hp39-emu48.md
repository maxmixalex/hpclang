# HP39gs Emu48 validation

This is the first execution gate for the clean hpclang v1 HP39 backend.

## Artifact

GitHub Actions workflow: `HP39 package`

Artifact name:

`hpclang-hp39-emulator-smoke`

The artifact contains the stock HP39 Aplet directory metadata, the hpclang
Aplet, LIB275, a human-readable test procedure, the machine-readable result
address, build metadata and checksums.

## Why serial transfer is required

Emu48's HP39/40 emulation intentionally does not expose the normal object-stack
loading path. Aplets must be transferred through the emulated calculator
communication port, matching the real HP39-series workflow.

Reference:

- https://hp.giesselink.com/emu48faq.htm
- https://www.hpcalc.org/details/5421
- https://www.hpcalc.org/hp39/pc/

A convenient Windows setup is a virtual null-modem pair (for example com0com)
between Emu48 and HPGComm 2.0.

## Expected visible result

Launching `HPCLANG HPC1 TEST` must display:

```text
HPC1 PASS
MODEL 00000002
RW <mapped byte count>
APLET TO EXIT
```

The exact RW byte count depends on the stock launcher page pool and is not a
fixed pass criterion.

## Machine-readable result

The smoke writes:

```text
address: 0x01101004
value:   0x50415353
meaning: "PASS"
```

This address/value pair is the contract for the future headless Emu48 E2E
harness.

## Pass criteria

All of the following must hold:

1. The Aplet is visible in the APLET library after transfer.
2. Launch reaches the `HPC1 PASS` screen.
3. Model is `00000002` (HP39gs).
4. The calculator remains responsive while the result screen is displayed.
5. Pressing APLET exits the smoke and returns cleanly to the stock OS.
6. There is no reset, ADDRESS ERROR, corrupted LCD, or persistent OS state
   corruption after return.

## Failure report

Record the earliest observable failure boundary:

- transfer/install failure;
- Aplet visible but launch returns immediately;
- reset or ADDRESS ERROR before visible HPC1 output;
- partial/corrupt HPC1 output;
- PASS screen reached but APLET exit fails;
- return succeeds but stock OS state is corrupted.

A screenshot plus the exact category above is enough for the next debugging
iteration.
