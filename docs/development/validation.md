# Validation

hpclang separates common-runtime correctness from calculator UI/package automation so a menu, transfer, or launcher failure cannot be mistaken for an ABI/runtime regression.

## Continuous gates

| Gate | Purpose |
| --- | --- |
| `CI / Core runtime` | host/link/runtime invariants for the LLVM-native core |
| `CI / HP39 package` | build HP39g+/HP39gs/HP40gs hardware-validation packages |
| `E2E / Direct runtime` | family-level Direct jobs boot target-appropriate firmware, execute HPC1 through stage0/stage1/common bootstrap, observe PASS, and validate stock return |
| `E2E / Stock launch` | family-level Stock jobs drive the original transfer/UI/ARMToolbox launch path and require the same HPC1 PASS result |
| `Maintenance / CI` | trim Actions history and CI evidence |

Direct runtime passes on HP 39gs, HP 40gs, HP 48gII, HP 49g+, and HP 50g. Full stock launch passes on the same five models. Direct and Stock each use one runner per platform family; sibling targets share one x50ng/runtime setup and execute in parallel. HP 39g+ is intentionally blocked until a trustworthy real-machine stock flash fixture exists.

## Runtime PASS contract

The canonical runtime test validates:

- initialized globals and zeroed BSS;
- static TLS;
- constructors;
- 32-bit and 64-bit division;
- ARMv4T-safe software floating point;
- libc memory/string operations;
- heap allocation;
- `snprintf`;
- machine-readable `0x50415353` plus visible `RUNTIME PASS`.

The Direct emulator gate additionally verifies that temporary mappings and the
SWI vector are restored before the stock calculator resumes. It also snapshots
stable S3C2410 LCD controller configuration (`LCDCON1..5`,
`LCDSADDR1..3`, LUT/dither/TPAL, interrupt mask and LPCSEL) before HPC1 and
requires an exact match at the return sentinel. Dynamic interrupt
pending/source registers are excluded. This comparison occurs before harness
restoration, so display-state damage cannot be hidden by host cleanup.

## HP 39gs real-hardware gate

Use the target-specific hardware-validation artifact from `CI / HP39 package`. The HP39 transfer names are intentionally preserved:

- `HPGCC000.000`;
- `LIB275L.000`;
- `HP39DIR.000`;
- `HP39DIR.CUR`.

Transfer library 275 and the test aplet with Conn3x/HP Connectivity Kit, launch `HPCLANG HPC1 TEST`, and expect:

```text
RUNTIME PASS
```

After return, APLET/ENTER/ON must remain responsive. A stale LCD page until the stock UI repaints is acceptable; a frozen keyboard, reset, ADDRESS ERROR, or persistent calculator-state damage is not.

The real HP 39gs run has passed the complete canonical runtime and returned cleanly to the stock OS.

The HP 39g+ model-1 hardware package is built by the same workflow, but that is not equivalent to native HP39g+ E2E validation.

## HP 39gs/40gs stock emulator gate

The `Stock / hp39` family job boots each official 2.22 ROM and uses the calculator's original serial-disk path to request `HP39DIR.CUR`, `HP39DIR.000`, `HPGCC000.000`, and `LIB275L.000`.

It then launches the installed hpclang aplet through ARMToolbox39 and requires the same machine-readable HPC1 PASS result.

HP 40gs uses its own stock UI sequence because it must leave the CAS front end before APLET and its RECEIVE FROM menu has no IrDA entry.

## HP 48gII Direct emulator gate

The HP48gII Direct gate boots a verified 2 MiB full-NOR image, including its native HP48gII ARM bootloader, with 256 KiB physical RAM and a 131×64 display.

The fixture is pinned by SHA-256 before being installed as persistent x50ng flash state. The same model-independent HPC1 image then produces machine-readable PASS and returns to a responsive stock environment.

## HP 48gII stock emulator gate

The `Stock / hp48` family job boots the verified 2 MiB full-NOR fixture, switches the factory-default Algebraic mode to RPN, selects Serial wire I/O, and starts the calculator's Kermit server.

The host transfers historical ARMToolbox48 library 275 as `L275` plus the generated `HPCLANG.HP` object as `HPCLANG`. The calculator installs library 275 into Port 0, warmstarts, recalls `HPCLANG`, enters `ARMTo` / `PrRUN`, and must produce the same machine-readable HPC1 PASS word.

The host Kermit process begins after 35 seconds and is allowed up to 70 seconds
to complete. Calculator-side post-transfer automation therefore does not resume
before 110 seconds. The former 95-second resume point could race a valid slow
transfer that completed around 97 seconds and produce a misleading `Too Few
Arguments` UI failure even though serial transfer itself succeeded.

## HP49g+/50g stream-storage gate

M7.2 validates storage through **standard picolibc stdio**, not a public KOS
API. The Stock SD fixture contains a known read file and a longer pre-existing
output file. The calculator payload must:

1. `fopen/fread/fclose` the known input;
2. open the output as `w+b` and prove create/truncate semantics;
3. `fwrite` + `fflush`;
4. seek to a nonzero offset, verify `ftell`, read back through the same
   stream, and verify the resulting position;
5. close, reopen read-only, and prove the old tail was truncated;
6. `remove` the output and require the next `fopen` to fail with
   `ENOENT`;
7. on HP39/40 and HP48, require the same stream `fopen` path to fail
   cleanly with `ENOSYS` without issuing a KOS filesystem call.

The runtime backend underneath those calls is hp4950-only
`open/close/read/write/lseek/unlink` over the stock KOS filesystem table.
HP39/40 calculator objects are deliberately excluded from this byte-stream
gate.

## HP 49g+/50g stock emulator gate

The `Stock / hp4950` family job uses each model's native ARM bootloader with the shared official 2.15 OS image.

It installs the same ARMToolbox/library 275 package from the emulated SD card, copies the generated `HPCLANG.HP` object through the stock File Manager, enters `ARMTo` / `PrRUN`, and requires the machine-readable HPC1 PASS result.

HP 49g+ and HP 50g intentionally share the same install and launch sequences. Their separate bootloader files are target metadata; no model-specific hpclang runtime path is involved.

## Evidence retention

LCD summaries are written to the dedicated `ci-evidence` orphan branch and tracked with Git LFS. The branch contains only images plus `.gitattributes`.

Weekly maintenance keeps the newest 20 evidence run directories per family. Current workflows retain up to 20 completed Actions runs; transient E2E artifacts use short retention windows.

See [E2E internals](e2e.md) for target profiles, sequence syntax, and x50ng integration, and [Fixture provenance](fixtures.md) for the external firmware/full-NOR inputs used by these gates.
