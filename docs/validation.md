# Validation

hpclang separates common-runtime correctness from calculator UI/package
automation so a menu failure cannot be mistaken for an ABI/runtime regression.

## Continuous gates

| Gate | Purpose |
| --- | --- |
| `CI / Core runtime` | host/link/runtime invariants for the LLVM-native core |
| `CI / HP39 package` | build HP39gs/HP40gs hardware-validation packages |
| `E2E / Direct runtime` | boot official firmware, execute HPC1 through stage0/stage1/common bootstrap, observe PASS and validate stock return |
| `E2E / HP50g stock launch` | original HP50g UI -> ARMToolbox -> HPCLANG.HP -> PrRUN -> HPC1 PASS |
| `Maintenance / CI` | trim Actions history and CI evidence |

Direct runtime currently passes on HP39gs, HP40gs, HP49g+ and HP50g. HP48gII
is the remaining family target to add.

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

Use the `hpclang-hp39gs-hardware-validation` artifact from
`CI / HP39 package`. Preserve the stock transfer filenames:

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
