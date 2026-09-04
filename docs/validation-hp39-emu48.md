# HP39gs Emu48 validation

This is the emulator execution gate for the clean hpclang v1 HP39 backend.

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
communication port.

References:

- https://hp.giesselink.com/emu48faq.htm
- https://www.hpcalc.org/details/5421
- https://www.hpcalc.org/hp39/pc/

The historical manual setup used a host COM port or a virtual null-modem pair
between Emu48 and HPGComm/HPComm38. That remains usable when a working serial
endpoint already exists, but it is not a prerequisite for hpclang development.
In particular, do not require unsigned/test-signed Windows kernel drivers merely
to satisfy this gate.

If Emu48 exposes only `disabled` serial choices, use the real-hardware gate in
`validation-hp39-hardware.md` while the driverless emulator transport is being
completed.

## Driverless E2E boundary

Emu48 models the calculator UART separately from its host serial transport. The
calculator-side transmit/receive state lives in the emulated UART registers;
the Win32 COM handle is only the host byte transport.

The automated HP39 harness should therefore keep the calculator UART behavior
unchanged and replace only the host endpoint with an in-memory peer:

```text
HP39 ROM
  -> emulated UART
  -> in-memory byte transport
  -> minimal HP39 transfer peer
  -> generated HP39DIR/HPGCC000/LIB275 bundle
```

This is preferable to GUI-coordinate automation and to requiring a host virtual
COM driver. Emu48 and HPComm38 remain external GPL research/test dependencies;
hpclang's production runtime does not absorb their implementations.

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

This address/value pair is the contract for the headless Emu48 E2E harness.

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
