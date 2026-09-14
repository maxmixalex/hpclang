# Validation

hpclang separates common-runtime correctness from calculator UI/package automation so a menu, transfer, launcher, or final repaint failure cannot be mistaken for an ABI/runtime regression.

## Continuous gates

| Gate | Purpose |
| --- | --- |
| `CI / Core runtime` | host/link/runtime invariants for the LLVM-native core |
| `CI / Font Assets` | deterministic generated-font catalog, source parity, and repertoire assertions |
| `CI / HP39 package` | build HP39g+/HP39gs/HP40gs hardware-validation packages |
| `E2E / Runtime` Direct families | boot target-appropriate firmware, execute canonical HPC1 through stage0/stage1/common bootstrap, observe PASS, and validate machine-state restoration |
| `E2E / Runtime` Stock families | drive original transfer/UI/ARMToolbox launch paths and require the same canonical runtime PASS artifact |
| `E2E / Font Gallery` | exhaustively traverse generated/native Gallery entries in Direct plus compact live-native Stock controls on HP50g and HP48gII |
| `Maintenance / CI` | trim Actions history and retained CI evidence |

Direct runtime passes on HP39gs, HP40gs, HP48gII, HP49g+, and HP50g. HP39g+ remains blocked until a trustworthy real-machine stock flash fixture exists.

Current canonical runtime run **34792822400 / #78** is green for Build, all three Direct families, Stock hp39, and Stock hp4950. HP48 Stock reaches the payload PASS and control-flow return but fails only the final visible Stock repaint with `return-unsettled`. That outcome is a Stock harness/UI-restoration concern, not a common-runtime or font regression.

## Runtime PASS contract

The canonical runtime test validates:

- initialized globals and zeroed BSS;
- static TLS and constructors;
- ARMv4T-safe integer and soft-float helpers;
- libc memory/string/formatting behavior;
- heap allocation;
- public display/font rendering used by visible result and keyboard screens;
- the complete 51-key physical keyboard/event path;
- hp4950 stream-storage behavior where supported;
- machine-readable `0x50415353` plus visible runtime evidence.

The Direct emulator gate also verifies that temporary mappings and the SWI vector are restored before the stock calculator resumes. Stable S3C2410 LCD-controller state is compared before harness cleanup so display-state damage cannot be hidden by host-side restoration.

## PASS, control-flow return, and visible return

These are separate claims and must remain separately observable:

1. **HPC1 PASS** proves the payload completed its assertions.
2. **Control-flow/state return** proves temporary hpclang mappings/state were removed and execution returned to the stock environment.
3. **Visible Stock return** proves the stock OS can process a normal UI action and redraw the LCD after return.

A nonblank `return.pgm` that still contains hpclang's last frame is insufficient for item 3. The harness must not manufacture success by copying the pre-launch framebuffer back. HP39 real-hardware validation uses **APLET** as the harmless post-return stock action.

## Native-font validation and provenance

Calculator-native font bitmaps are borrowed stock assets and are never redistributed by hpclang. The common API deliberately hides two different platform mechanisms.

### HP48gII / HP49g+ / HP50g

Historical HPGCC calculator-native helpers resolve the minifont and bigfont through `sat_map_s2a()` and the live logical Saturn `Read[]` mapping. hpclang therefore keeps HP48/49/50 native lookup on the launch-preserved `SAT_CPU->Read[]` view.

### HP39gs / HP40gs

HP39/40 is a separate stock-ROM path. Historical HPGCC HP39 support binds stock `Put3x5` / `Put5x7` ROM entries, while independent ROM analysis establishes the raster tables themselves: mini-font physical Saturn base `0x07E695` and main-font base `0x07C651`.

The first physical-NOR implementation borrowed an apparently empty high L1 section descriptor. Diagnostics proved that descriptor storage could overlap the active LCD framebuffer and be overwritten by ordinary drawing. Production code therefore maps only physical NOR pages `0x00042000` and `0x00043000` as uncached 4 KiB small pages in the private prefix of hpclang's own code coarse table. The private virtual window is `0x01200000..0x01201FFF`; public HPC1 code still starts at `0x01220000`. `platform_cookie` carries the private-window base only as an opaque launcher/runtime token.

HP39g+ is deliberately excluded from that physical layout until a real-hardware full-flash fixture establishes its own identity.

## Final font validation closure

The current canonical source bundle is bitmap-only. The generated catalog is **245 faces / 1,639,485 glyph mappings / 3,472 assembly sources** with **245/245** source→generated parity. Font Assets **#24** regenerates the package without drift and reports zero exact duplicates, overflow, glyph-collapse, blank-heavy, or Powerline regressions.

The final exhaustive Gallery contains **247 entries: 245 generated + 2 native**. Direct renders both SAMPLE and TEXT pages for **494 captures per target** on HP39gs, HP40gs, HP48gII, HP49g+, and HP50g.

Formal production `E2E / Font Gallery` run **34792822393 / #55** is fully green:

- Build Font Gallery;
- universal x50ng;
- `Gallery / hp39`;
- `Gallery / hp48`;
- `Gallery / hp4950`;
- `Stock / hp48`;
- `Stock / hp4950`.

The Stock evidence was visually checked from the run artifacts. Both HP48gII and HP50g summaries use the same compact three-panel structure: calculator minifont, calculator main font, then an Ark Pixel generated control face. HP50g reaches settled PASS. HP48gII has `saw_pass=true`, control-flow return, and valid distinct nonblank native/generated captures, while the final visible repaint remains `return-unsettled`. The font-specific gate accepts that already-classified post-PASS state only for its concern; it does **not** satisfy the separate visible-return gate.

The Gallery also validates the production HP39 private two-page mapping, generated aliases/arbitrary face selection, arbitrary metrics/spacing, strengthened repertoire-sensitive specimens, and deterministic pagination. Earlier 242-face / 244-entry / 488-capture runs remain historical evidence but are no longer the production closure numbers.

## HP39 real-hardware gate

Use the target-specific hardware-validation artifact from `CI / HP39 package`. The transfer names are intentionally preserved:

- `HPGCC000.000`;
- `LIB275L.000`;
- `HP39DIR.000`;
- `HP39DIR.CUR`.

Transfer library 275 and the test aplet with Conn3x/HP Connectivity Kit, launch `HPCLANG HPC1 TEST`, and expect runtime PASS. After return, press APLET. A valid return opens/redraws the normal APLET library. A stale LCD page before that stock repaint is acceptable; a frozen keyboard, reset, ADDRESS ERROR, or persistent calculator-state damage is not.

The real HP39gs run has passed the complete canonical runtime and returned cleanly to the stock OS. The HP39g+ model-1 hardware package is not equivalent to native HP39g+ E2E validation.

## HP39gs/40gs Stock emulator gate

`Stock / hp39` boots each official 2.22 ROM, transfers `HP39DIR.CUR`, `HP39DIR.000`, `HPGCC000.000`, and `LIB275L.000` through the original serial-disk path, launches the installed aplet through ARMToolbox39, and requires the canonical HPC1 PASS result.

Closed-loop `KBRD -> KACK(n)` key synchronization is now implemented. Runtime #78 completes Stock hp39 successfully. The remaining requested follow-up is to mirror Direct's 52-frame keyboard progression evidence in Stock, not to rewrite the common keyboard service.

## HP48gII Direct and Stock gates

Direct boots the verified 2 MiB full-NOR image with 256 KiB RAM and a 131×64 display and validates the same target-independent HPC1 image.

Stock uses the verified NOR fixture, switches the factory-default Algebraic mode to RPN, selects Serial wire I/O, starts the stock Kermit server, receives library 275 plus `HPCLANG.HP`, and launches through ARMToolbox48 / `PrRUN`.

HP48 Stock currently reaches HPC1 PASS and control-flow restoration. The unresolved gate is only the first native visible repaint after return. This must remain reported as `return-unsettled` until a genuinely stock-owned post-return frame is observed.

## HP49g+/50g stream-storage and Stock gate

M7.2 validates storage through standard picolibc stdio rather than a public KOS API. HP49g+/HP50g Stock tests cover read, create/truncate, write/flush, seek/ftell, reread, close/reopen, remove and `ENOENT`. HP39/40 and HP48 must reject unsupported byte-stream operations cleanly with `ENOSYS`.

The hp4950 Stock family uses each model's native ARM bootloader with the shared official 2.15 OS image, installs ARMToolbox/library 275 from emulated SD, copies `HPCLANG.HP` through the stock File Manager, enters `ARMTo` / `PrRUN`, and requires the canonical PASS result.

## Evidence retention

Machine-readable assertions remain the gate. Images are review/debug evidence rather than substitutes for assertions.

Canonical summaries are published to `ci-evidence` and standard Actions artifacts retain per-run evidence. Keyboard progression and Font Gallery contact sheets/summaries use separate paths. Concurrent `ci-evidence` publication still needs a race-safe update strategy; an evidence-branch push conflict does not invalidate an already-uploaded Actions artifact or a passed calculator assertion.
