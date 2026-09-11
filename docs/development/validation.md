# Validation

hpclang separates common-runtime correctness from calculator UI/package automation so a menu, transfer, or launcher failure cannot be mistaken for an ABI/runtime regression.

## Continuous gates

| Gate | Purpose |
| --- | --- |
| `CI / Core runtime` | host/link/runtime invariants for the LLVM-native core |
| `CI / Font Assets` | deterministic generated-font catalog/repertoire assertions |
| `CI / HP39 package` | build HP39g+/HP39gs/HP40gs hardware-validation packages |
| `E2E / Runtime — Direct` | boot target-appropriate firmware, execute HPC1 through stage0/stage1/common bootstrap, observe PASS, and validate machine-state restoration |
| `E2E / Runtime — Stock` | drive the original transfer/UI/ARMToolbox launch path and require the same canonical runtime PASS artifact; Stock-specific transfer/install captures remain supplementary |
| `Maintenance / CI` | trim Actions history and CI evidence |

Direct runtime passes on HP 39gs, HP 40gs, HP 48gII, HP 49g+, and HP 50g. HP 39g+ is intentionally blocked until a trustworthy real-machine stock flash fixture exists.

Stock launch status must be reported from the latest run rather than assumed from an older all-green checkpoint. In the current font-gallery diagnostic run, hp48 and hp4950 families pass, HP39gs reaches canonical PASS and return, while HP40gs timed out before payload entry. That HP40gs result is a Stock transport/UI flake to repeat; it is not evidence of a font/runtime failure.

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
- public display/font rendering used by visible result and keyboard screens;
- the complete 51-key physical keyboard/event path;
- hp4950 stream-storage behavior where supported;
- machine-readable `0x50415353` plus visible runtime evidence.

The Direct emulator gate additionally verifies that temporary mappings and the SWI vector are restored before the stock calculator resumes. It snapshots stable S3C2410 LCD controller configuration (`LCDCON1..5`, `LCDSADDR1..3`, LUT/dither/TPAL, interrupt mask and LPCSEL) before HPC1 and requires an exact match at the return sentinel. Dynamic interrupt pending/source registers are excluded. This comparison occurs before harness restoration, so display-state damage cannot be hidden by host cleanup.

## PASS, control-flow return, and visible return

These are separate claims and must remain separately observable:

1. **HPC1 PASS** proves the payload completed its assertions.
2. **Control-flow/state return** proves temporary hpclang mappings/state were removed and execution returned to the stock environment.
3. **Visible Stock return** proves the stock OS can actually process a normal UI action and redraw the LCD after return.

A nonblank `return.pgm` that still contains hpclang's last frame is insufficient for item 3. The harness must not manufacture success by copying the pre-launch framebuffer back. Each family should issue a harmless native UI action after control-flow restoration and then require a visibly stock-owned frame. HP39 real-hardware guidance already uses **APLET** for this purpose.

## HP39 native-font regression gate

HP39 native font data is calculator-owned ROM content and is not redistributed by hpclang. Reverse engineering has independently closed the mini-font layout used by the stock renderer: physical Saturn base `0x07E695`, 11 nibbles per record, first nibble advance plus five bitmap rows. Stock `Put3x5` uses the same table layout.

A diagnostic HP39gs 2.22 run proved why the first hpclang reader produced stable corruption: logical `SAT_CPU->Read[]` was pointing the `0x7e` Saturn page at ARM `0x0009f000`, i.e. a different physical ROM bank than the fixed native raster asset. Therefore fixed HP39 ROM raster addresses must be resolved as physical flash assets; ordinary Saturn RAM/objects continue to use the live `Read[]` map. The production mapping is not considered complete until Direct and Stock screenshots validate it, and temporary bank-probe code must be removed afterwards.

## HP 39gs real-hardware gate

Use the target-specific hardware-validation artifact from `CI / HP39 package`. The HP39 transfer names are intentionally preserved:

- `HPGCC000.000`;
- `LIB275L.000`;
- `HP39DIR.000`;
- `HP39DIR.CUR`.

Transfer library 275 and the test aplet with Conn3x/HP Connectivity Kit, launch `HPCLANG HPC1 TEST`, and expect runtime PASS.

After return, press APLET. A valid return opens/redraws the normal APLET library. A stale LCD page before that stock repaint is acceptable; a frozen keyboard, reset, ADDRESS ERROR, or persistent calculator-state damage is not.

The real HP 39gs run has passed the complete canonical runtime and returned cleanly to the stock OS.

The HP 39g+ model-1 hardware package is built by the same workflow, but that is not equivalent to native HP39g+ E2E validation.

## HP 39gs/40gs stock emulator gate

The `Stock / hp39` family job boots each official 2.22 ROM and uses the calculator's original serial-disk path to request `HP39DIR.CUR`, `HP39DIR.000`, `HPGCC000.000`, and `LIB275L.000`.

It then launches the installed hpclang aplet through ARMToolbox39 and requires the same machine-readable HPC1 PASS result. Visible-return automation should mirror the real-hardware APLET check after control-flow restoration.

HP 40gs uses its own stock UI sequence because it must leave the CAS front end before APLET and its RECEIVE FROM menu has no IrDA entry.

## HP 48gII Direct emulator gate

The HP48gII Direct gate boots a verified 2 MiB full-NOR image, including its native HP48gII ARM bootloader, with 256 KiB physical RAM and a 131×64 display.

The fixture is pinned by SHA-256 before being installed as persistent x50ng flash state. The same model-independent HPC1 image then produces machine-readable PASS and returns to the stock execution environment.

## HP 48gII stock emulator gate

The `Stock / hp48` family job boots the verified 2 MiB full-NOR fixture, switches the factory-default Algebraic mode to RPN, selects Serial wire I/O, and starts the calculator's Kermit server.

The host transfers historical ARMToolbox48 library 275 as `L275` plus the generated `HPCLANG.HP` object as `HPCLANG`. The calculator installs library 275 into Port 0, warmstarts, recalls `HPCLANG`, enters `ARMTo` / `PrRUN`, and must produce the same machine-readable HPC1 PASS word.

The host Kermit process begins after 35 seconds and is allowed up to 70 seconds to complete. Calculator-side post-transfer automation therefore does not resume before 110 seconds. The former 95-second resume point could race a valid slow transfer that completed around 97 seconds and produce a misleading `Too Few Arguments` UI failure even though serial transfer itself succeeded.

## HP49g+/50g stream-storage gate

M7.2 validates storage through **standard picolibc stdio**, not a public KOS API. The Stock SD fixture contains a known read file and a longer pre-existing output file. The calculator payload must:

1. `fopen/fread/fclose` the known input;
2. open the output as `w+b` and prove create/truncate semantics;
3. `fwrite` + `fflush`;
4. seek to a nonzero offset, verify `ftell`, read back through the same stream, and verify the resulting position;
5. close, reopen read-only, and prove the old tail was truncated;
6. `remove` the output and require the next `fopen` to fail with `ENOENT`;
7. on HP39/40 and HP48, require the same stream `fopen` path to fail cleanly with `ENOSYS` without issuing a KOS filesystem call.

The runtime backend underneath those calls is hp4950-only `open/close/read/write/lseek/unlink` over the stock KOS filesystem table. HP39/40 calculator objects are deliberately excluded from this byte-stream gate.

## HP 49g+/50g stock emulator gate

The `Stock / hp4950` family job uses each model's native ARM bootloader with the shared official 2.15 OS image.

It installs the same ARMToolbox/library 275 package from the emulated SD card, copies the generated `HPCLANG.HP` object through the stock File Manager, enters `ARMTo` / `PrRUN`, and requires the machine-readable HPC1 PASS result.

HP 49g+ and HP 50g intentionally share the same install and launch sequences. Their separate bootloader files are target metadata; no model-specific hpclang runtime path is involved.

## Keyboard progression evidence

The machine-readable keyboard assertions remain the gate. For visual diagnosis, Direct captures an initial hollow physical-keyboard diagram and one frame after each of the 51 accepted key releases, for 52 progression frames total. Stock should mirror the same capture schedule.

The merged progression contact sheet is supplementary and should be published separately from the canonical `before / pass / return` summary. A progression image must not replace the underlying key/event assertions.

## Font Gallery validation

Font Gallery is a separate HPC1 E2E application rather than additional pages in the canonical runtime payload. It should produce its own deterministic evidence for native semantic font roles, generated aliases/arbitrary faces, metrics/spacing, representative coverage, and 64/80-line layouts. Its visual regressions therefore cannot accidentally change the scope or timing of runtime/keyboard/storage validation.

## Evidence retention

Canonical LCD summaries are written to the dedicated `ci-evidence` orphan branch and tracked with Git LFS. The branch contains only images plus `.gitattributes`.

Keyboard progression and Font Gallery contact sheets are supplementary evidence and should use their own paths/names instead of being merged into the canonical three-frame runtime summary.

Weekly maintenance keeps the newest 20 evidence run directories per family. Current workflows retain up to 20 completed Actions runs; transient E2E artifacts use short retention windows.

See [E2E internals](e2e.md) for target profiles, sequence syntax, x50ng integration and focused E2E-app boundaries, and [Fixture provenance](fixtures.md) for the external firmware/full-NOR inputs used by these gates.
