# Validation

hpclang separates common-runtime correctness from calculator UI/package
automation so a menu failure cannot be mistaken for an ABI/runtime regression.

## Continuous gates

| Gate | Purpose |
| --- | --- |
| `CI / Core runtime` | host/link/runtime invariants for the LLVM-native core |
| `CI / HP39 package` | build HP39g+/HP39gs/HP40gs hardware-validation packages |
| `E2E / Direct runtime` | boot official firmware, execute HPC1 through stage0/stage1/common bootstrap, observe PASS and validate stock return |
| `E2E / HP39/40 stock launch` | original HP39gs/HP40gs APLET UI -> serial-disk transfer -> ARMToolbox39 -> HPC1 PASS |
| `E2E / HP48gII stock launch` | original HP48gII UI -> Kermit transfer -> ARMToolbox48 -> HPCLANG.HP -> PrRUN -> HPC1 PASS |
| `E2E / HP49/50 stock launch` | original HP49g+/HP50g UI -> SD -> ARMToolbox -> HPCLANG.HP -> PrRUN -> HPC1 PASS |
| `Maintenance / CI` | trim Actions history and CI evidence |

Direct runtime currently passes on HP39gs, HP40gs, HP48gII, HP49g+ and HP50g.
HP48gII uses a verified 2 MiB full-NOR fixture and a 256 KiB medium-apple x50ng
personality. Full stock launch also passes on HP39gs, HP40gs, HP48gII, HP49g+ and HP50g.

## Evidence

LCD summaries are written to the dedicated `ci-evidence` orphan branch and
tracked with Git LFS. The branch contains only images plus `.gitattributes`.

Weekly maintenance keeps the newest 20 evidence run directories per family.
Current workflows retain up to 20 completed Actions runs; removed workflows
retain only 2. Transient E2E artifacts use short retention windows.

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

The direct emulator gate additionally verifies that temporary mappings and the
SWI vector are restored before the stock calculator resumes.

## HP39gs real-hardware gate

Use the target-specific hardware-validation artifact from
`CI / HP39 package` (including the model-1 HP39g+ package when testing that
hardware). Preserve the stock transfer filenames:

- `HPGCC000.000`;
- `LIB275L.000`;
- `HP39DIR.000`;
- `HP39DIR.CUR`.

Transfer library 275 and the test aplet with Conn3x/HP Connectivity Kit, launch
`HPCLANG HPC1 TEST`, and expect:

```text
RUNTIME PASS
```

After return, APLET/ENTER/ON must remain responsive. A stale LCD page until the
stock UI repaints is acceptable; a frozen keyboard, reset, ADDRESS ERROR or
persistent calculator-state damage is not.

The real HP39gs run has passed the complete canonical runtime and returned
cleanly to the stock OS.

## HP39gs/HP40gs stock emulator gate

The `E2E / HP39/40 stock launch` matrix boots each official 2.22 ROM and uses
the calculator's original serial-disk path to request `HP39DIR.CUR`,
`HP39DIR.000`, `HPGCC000.000` and `LIB275L.000`. It then launches the installed
HPCLANG aplet through ARMToolbox39 and requires the same machine-readable HPC1
PASS result. HP40gs uses its own stock UI sequence because it must leave the CAS
front end before APLET and its RECEIVE FROM menu has no IrDA entry.

## HP48gII direct emulator gate

The HP48gII Direct gate boots a verified 2 MiB full-NOR image, including its
native 48gII ARM bootloader, on a 256 KiB / 131x64 medium-apple x50ng
personality. The fixture is pinned by SHA-256 before being installed as the
persistent x50ng flash state. The same model-independent HPC1 image then
produces machine-readable PASS and returns to a responsive stock environment.

## HP48gII stock emulator gate

The `E2E / HP48gII stock launch` gate boots the verified 2 MiB full-NOR
fixture, switches the factory-default Algebraic mode to RPN, selects Serial
wire I/O and starts the calculator's Kermit server. The host transfers the
historical ARMToolbox48 library as `L275` plus the generated `HPCLANG.HP`
object as `HPCLANG`. The calculator installs library 275 into Port 0,
warmstarts, recalls `HPCLANG`, enters `ARMTo` / `PrRUN` and must produce
the same machine-readable HPC1 PASS word.

## HP49g+/HP50g stock emulator gate

The `E2E / HP49/50 stock launch` matrix uses each model's native ARM
bootloader with the shared official 2.15 OS image. It installs the same
ARMToolbox/library 275 package from the emulated SD card, copies the generated
`HPCLANG.HP` object through the stock File Manager, enters `ARMTo` /
`PrRUN` and requires the machine-readable HPC1 PASS result.

HP49g+ and HP50g intentionally share the same install and launch sequences.
Their separate `boot-49g+.bin` and `boot-50g.bin` files are target metadata;
no model-specific hpclang runtime path is involved.
