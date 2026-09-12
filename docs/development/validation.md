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
| `E2E / Font Gallery` | exhaustively traverse generated/native font entries in Direct, plus compact live-native Stock controls on HP50g and HP48gII |
| `Maintenance / CI` | trim Actions history and CI evidence |

Direct runtime passes on HP 39gs, HP 40gs, HP 48gII, HP 49g+, and HP 50g. HP 39g+ is intentionally blocked until a trustworthy real-machine stock flash fixture exists.

Stock launch status must be reported from the latest run rather than inferred from an older all-green checkpoint. For the physical-font fix, hp48 and hp4950 Stock families are green. HP39gs and HP40gs have each independently completed the full Stock serial/APLET/ARMToolbox path in recent runs, and HP40gs produced a visually correct `FONT OK` PASS frame with the new physical-ROM reader. The bounded rerun of the family job brought both HP39gs and HP40gs into the payload, then both failed the keyboard transition with `0x46410010`. Because the five-model Direct 51-key gate remains green, this is tracked as a Stock harness/input-synchronization flake rather than a font/runtime regression.

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

## Native-font validation and provenance

Calculator-native font bitmaps are borrowed stock assets and are never redistributed by hpclang. The public font API hides two platform-specific lookup mechanisms that historical code and ROM evidence show are genuinely different.

For HP48gII/49g+/50g, historical HPGCC's calculator-native helpers use the live logical Saturn map: `hplib/stdio/get_minifont.c` resolves `0x812c8` through `sat_map_s2a()`, `get_bigfont.c` resolves `0x84d82` the same way, and `sat_map_s2a()` is exactly `_saturn_cpu->Read[sat_addr >> 12] + offset`. hpclang therefore keeps native HP48/49/50 lookup on the launch-provided `SAT_CPU->Read[]` view.

HP39/40 is a separate stock-ROM path. HPGCC's HP39 Aplet/Library 275 support binds the HP39 ROM text entries `Put3x5` and `Put5x7` rather than reusing the HP48/49/50 RAM-font locator. Independent HP39 ROM reverse engineering then closes the raster tables themselves: mini-font physical Saturn base `0x07E695`, 11 nibbles per record with one advance nibble plus five bitmap rows, and main-font base `0x07C651`, 16 nibbles per record. These exact physical table addresses come from HP39 stock-ROM analysis, not from an HPGCC C constant.

A diagnostic HP39gs 2.22 run proved why the first hpclang reader produced stable corruption: logical `SAT_CPU->Read[]` pointed Saturn page `0x7e` at ARM `0x0009f000`, a different physical bank from the fixed raster asset at physical `0x07E695` / ARM byte `0x0003f34a`.

The first physical-NOR implementation then exposed another hardware-integration trap. It borrowed an apparently empty high L1 section descriptor for a long-lived 1 MiB NOR alias. On the HP39 fixtures, the active LCD framebuffer itself occupies bytes inside the physical 16 KiB L1-table region: the selected HP39gs `L1[0xBE]` descriptor at `0x07F002F8` and HP40gs `L1[0xA0]` descriptor at `0x07F00280` both fall inside the framebuffer interval around `0x07F0026C..0x07F0076C`. Gallery drawing therefore overwrote the MMU descriptor as pixels and produced the observed Data Abort. An empty descriptor value is not sufficient evidence that its backing word is safe persistent storage.

The production reader no longer borrows any stock high L1 slot. HP39gs/40gs map only physical NOR pages `0x00042000` and `0x00043000`, which contain the complete main and mini raster tables, as uncached 4 KiB small pages in the otherwise-unused prefix of hpclang's own code coarse table. The private virtual window is `0x01200000..0x01201FFF`; public HPC1 code still starts at `0x01220000`. `platform_cookie` carries the exact private-window base as an opaque launcher/runtime token. Applications do not gain a physical-ROM or private-window ABI contract, and ordinary Saturn RAM/object access continues to use `Read[]`.

Validation is closed for that mapping and for the automated live-Saturn native-font paths:

- HP39gs and HP40gs each complete the targeted exhaustive Gallery at **310 entries / 620 captures** with PASS and settled return;
- production `E2E / Font Gallery` run **34710471059 / #45** is fully green: Build, universal x50ng, `Gallery / hp39`, `Gallery / hp48`, `Gallery / hp4950`, `Stock Gallery / hp50g`, and `Stock native-font evidence / hp48gii` all succeed;
- corrected HP50g Stock evidence independently shows the live `Read[]` calculator minifont and main font plus a generated control face, with PASS and settled stock return;
- compact HP48gII Stock evidence independently shows the live `Read[]` calculator minifont and main font plus a generated control face through the stock Serial Kermit/ARMToolbox48/HPC1 path;
- the HP48 font-specific gate requires `saw_pass: true` and valid distinct native-mini/native-main/generated captures, while allowing only the already-classified post-PASS `return-unsettled` outcome; this does **not** satisfy the separate visible Stock-return gate;
- HP40gs Stock/ARMToolbox has independent visually correct native-font evidence;
- Core font-window/MMU tests, font-selection modes, ARM hardware-validation builds, and HP39 packages pass;
- temporary alias, Data-Abort, framebuffer, compact-window, and exhaustive-window diagnostics/workflows are removed.

The compact Stock Gallery sequence must move **DOWN** between calculator minifont and calculator main font because they are two faces of the same `Native` family, then move **RIGHT** to the generated control family. Earlier HP48/HP50 evidence moved RIGHT between native captures and therefore mislabeled an Ark Pixel frame as native main; both sequences were corrected and revalidated before run #45 became the formal closure.

HP39g+ remains deliberately excluded from the gs/40gs physical-NOR layout until a trustworthy real-hardware full-flash fixture establishes its own identity. The retained HP39g+ NOR dumper is evidence for the historical transient alias mechanism, not evidence that HP39g+ shares the gs/40gs firmware layout.

The remaining Stock/hp39 keyboard flake and HP48 visible-return settling issue are downstream of this font proof and are not blockers for the native-font mapping/evidence closure.

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

### Current Stock keyboard flake and repair boundary

The current Stock launcher waits for the guest's `PHASE_KEYBOARD_READY` marker and then schedules all 51 key down/up pairs on x50ng's realtime timer. Direct uses the same 50 ms hold and 120 ms step values and passes on all five automated models. The guest keyboard test is intentionally strict: after resetting its event baseline, any event other than the expected next DOWN/UP pair fails immediately with `0x10` (`0x46410010` in the shared result word).

That makes the first repair attempt a **small, harness-local synchronization change**, not a keyboard-runtime rewrite. Preferred order:

1. make Stock key progression closed-loop or explicitly acknowledge guest progress instead of pre-scheduling all 51 pairs;
2. publish the same 52-frame keyboard progression evidence already available in Direct so the first divergent key is visible;
3. if needed, add an explicit release-all/quiescent baseline handshake after Stock UI/ARMToolbox launch and before the first injected key.

Only if those bounded changes still reproduce a failure should investigation move into x50ng GPIO/IRQ behavior. That escalation is deliberately not part of the current font-gallery critical path; a short real-hardware keyboard run is preferable to an open-ended emulator-internals investigation.

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

Font Gallery is a separate HPC1 E2E application rather than additional pages in the canonical runtime payload. The exhaustive Direct payload traverses **310 entries** — 308 generated faces plus two calculator-native entries — and renders both SAMPLE and TEXT pages, for **620 captures per target**. The production family jobs cover HP39gs, HP40gs, HP48gII, HP49g+, and HP50g across the 64/80-line layouts. Direct does not fabricate a live Saturn map, so the compact Stock controls provide authoritative HP48/49/50 native-font pixels: HP50g covers hp4950 and HP48gII covers the independent 64-line hp48 path.

The HP48 compact gate is intentionally concern-specific. It requires valid distinct native-mini/native-main/generated captures and `saw_pass: true`; the known first post-return repaint may still report `return-unsettled`. That outcome is accepted only by the font-evidence gate and must never be reported as a visible Stock-return PASS.

The public font core remains direct rather than semantic-role based: applications choose native `hpclang_font_default()/mini()/big()` or generated `hpclang_target_font()` products explicitly. Any future `mini` / `text` / `edit-large` theme vocabulary belongs above this layer in a UI toolkit.

Production run **34710471059 / #45** is the current formal Gallery closure and is green across Build, universal x50ng, all three Direct families, HP50g Stock, and HP48gII Stock native-font evidence. Font Gallery evidence remains separate from canonical runtime/keyboard/storage evidence so visual font regressions cannot silently change those gates.

## Evidence retention

Canonical LCD summaries are written to the dedicated `ci-evidence` orphan branch and tracked with Git LFS. The branch contains only images plus `.gitattributes`.

Keyboard progression and Font Gallery contact sheets are supplementary evidence and should use their own paths/names instead of being merged into the canonical three-frame runtime summary.

Weekly maintenance keeps the newest 20 evidence run directories per family. Current workflows retain up to 20 completed Actions runs; transient E2E artifacts use short retention windows.

See [E2E internals](e2e.md) for target profiles, sequence syntax, x50ng integration and focused E2E-app boundaries, and [Fixture provenance](fixtures.md) for the external firmware/full-NOR inputs used by these gates.
