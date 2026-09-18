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

Current fully-green canonical runtime evidence is **35298949365 / #135** on the final M7.1 head: Build, all three Direct families, Stock hp39, Stock hp48, Stock hp4950, and the exact 52-frame keyboard progression all pass. The HP48 Stock path reaches payload PASS, restores control flow, tears down the temporary stage mapping, and reaches a settled stock-owned repaint. Runtime #122 remains the first run that closed this same contract; later intermediate runs are historical diagnostics rather than the current gate.

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

Historical HPGCC calculator-native helpers resolve the minifont and bigfont through `sat_map_s2a()` and the live logical Saturn `Read[]` mapping. hpclang therefore keeps HP48/49/50 native lookup on the launch-preserved `SAT_CPU->Read[]` view. HPGCC3 explicitly defines bigfont as an eight-row raster with nominal advance 6; hpclang treats it as a concrete HP48/49/50 stock font, not a generic “large text” role.

### HP39gs / HP40gs

HP39/40 is a separate stock-ROM path. Historical HPGCC HP39 support binds stock `Put3x5` / `Put5x7` ROM entries, while independent ROM analysis establishes the raster tables themselves: mini-font physical Saturn base `0x07E695` and main-font base `0x07C651`. The HP39 UI reverse-engineering workspace independently establishes a third fixed 6×10 EditLine table at legacy base `0x07D3EA`, with 20 Saturn nibbles per character. This is a distinct ROM font, not the HP48/49/50 HPGCC bigfont.

The first physical-NOR implementation borrowed an apparently empty high L1 section descriptor. Diagnostics proved that descriptor storage could overlap the active LCD framebuffer and be overwritten by ordinary drawing. Production code therefore maps only physical NOR pages `0x0007e000` and `0x0007f000` as uncached 4 KiB small pages in the private prefix of hpclang's own code coarse table. The private virtual window is `0x01200000..0x01201FFF`; public HPC1 code still starts at `0x01220000`. `platform_cookie` carries the private-window base only as an opaque launcher/runtime token.

HP39g+ is deliberately excluded from that physical layout until a real-hardware full-flash fixture establishes its own identity.

## Native ROM-font API contract

Native access is intentionally a small census of concrete stock fonts, not a semantic role layer. `hpclang_font_default()` remains a convenience selection. `hpclang_font_mini()` exposes the stock minifont. `hpclang_font_big()` is reserved for the historical HP48gII/49g+/50g bigfont and must be unavailable on the HP39 family rather than aliasing the 5×8 main raster. A new `hpclang_font_editline()` accessor is reserved for the independently established HP39gs/40gs 6×10 EditLine table and is unavailable on the HP48/49/50 family and on HP39g+ until its flash layout is verified.

The 2.22 HP39gs/40gs EditLine relocation is independently gated against stock NOR data rather than accepted from address arithmetic alone. HP39gs physical nibble `0x0FD882` and HP40gs `0x0FDA35` both match the legacy EditLine-table SHA-256 `f106efa599a5bb8a783f29cbc2dfa3afbace750975e783c223f53864a2945f53`; the Direct and Stock Gallery paths also pass the independent ROM-derived 6×10 pixel oracle.

Gallery **#103** on commit `0821c822` is the pre-change baseline, #111 is the first fully-green post-EditLine gate, and Gallery **#112** (`35298949457`) is the final M7.1 head gate after documentation consistency cleanup. HP39gs/40gs carry the additional concrete EditLine ROM font while HP48/49/50 retain their two-font native census.

## Final font validation closure

The current canonical source bundle is bitmap-only. The generated catalog is **245 faces / 1,639,485 glyph mappings / 3,472 assembly sources** with **245/245** source→generated parity. Font Assets **#24** regenerates the package without drift and reports zero exact duplicates, overflow, glyph-collapse, blank-heavy, or Powerline regressions.

The exhaustive Gallery is now family-dependent. HP39gs/40gs contain **248 entries: 245 generated + mini + main/default + EditLine**, rendering **496 captures**. HP48gII/49g+/50g contain **247 entries: 245 generated + minifont + bigfont/default**, rendering **494 captures**.

Formal production `E2E / Font Gallery` run **35298949457 / #112** is fully green:

- Build Font Gallery;
- universal x50ng;
- `Gallery / hp39`;
- `Gallery / hp48`;
- `Gallery / hp4950`;
- `Stock / hp48`;
- `Stock / hp4950`.

The Stock evidence was visually checked from the run artifacts. HP39gs/40gs now use four panels: calculator minifont, calculator main font, calculator EditLine font, then the generated control; both models pass the EditLine fingerprint and 6×10 pixel oracle. HP48gII/HP50g retain three panels: minifont, bigfont/default, generated control, and both reach settled PASS.

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

Closed-loop `KBRD -> KACK(n)` key synchronization is implemented for Stock as well as Direct. Runtime #135 proves the exact 52-frame schedule: frame 0 is the initial state and frames 1..51 are emitted only after the preceding DOWN→UP cycle is acknowledged and the payload has accepted the key. Missing any frame fails the family.

## HP48gII Direct and Stock gates

Direct boots the verified 2 MiB full-NOR image with 256 KiB RAM and a 131×64 display and validates the same target-independent HPC1 image.

Stock uses the verified NOR fixture, switches the factory-default Algebraic mode to RPN, selects Serial wire I/O, starts the stock Kermit server, receives library 275 plus `HPCLANG.HP`, and launches through ARMToolbox48 / `PrRUN`.

HP48 Stock now reaches HPC1 PASS, control-flow restoration, temporary-stage teardown, and a genuinely stock-owned settled repaint. Runtime #135 is the current canonical fully-green witness; Gallery #112 independently validates the same settled return around the live native-font path. The harness still retains `return-unsettled` as a failure label so future regressions cannot be silently reclassified.

## HP49g+/50g stream-storage and Stock gate

M7.2 keeps standard byte-stream I/O in picolibc and exposes only typed metadata/media queries above the private KOS backend. HP49g+/HP50g Stock tests cover read, create/truncate, write/flush, seek/ftell, reread, close/reopen, append/create, remove and `ENOENT`; path size/type/portable attributes; synthetic root plus a real named-directory entry; media-present/read-only state; free bytes; and a fixed last-modified FAT/KOS civil datetime. HP39/40 and HP48 reject unsupported byte-stream and stream metadata/media operations cleanly with `ENOSYS`.

Stock diagnostics established an important backend rule: repeated KOS `CHECKINSERT` during one HP49g+ native launch can corrupt the return path, while one insertion probe followed by `GETSIZE` is return-safe. The production backend therefore samples insertion and write-protect state lazily once per native launch and reuses that snapshot; free bytes are queried per call. This is a deliberate host-preservation contract, not a generic hot-plug monitor.

Storage #22 (`35378151818`) is fully green across Build, Direct hp39/hp48/hp4950, and Stock hp4950. Both HP49g+ and HP50g reach HPC1 PASS, release the ARMToolbox stage alias, accept a Stock-owned repaint, and finish PASS. The timestamp fixture pins `HPCLANG.TST` to `2024-01-02 03:04:06 UTC`; only the resulting last-modified civil fields are public. Creation/access timestamp semantics remain private because validity alone is not enough to establish their application meaning.

M7.2c additionally has a Stock-filesystem reinitialization witness. Storage #23 issues KOS syscall 19 only after every KOS-backed stream handle is closed, invalidates the runtime's cached media sample, then verifies that `HPCLANG.TST` metadata plus reopen/read work again. The Stock hp4950 job passes on both HP49g+ and HP50g with HPC1 PASS, ARMToolbox stage release, and visible Stock return. This proves the **Stock reinit** half of the future direct-FAT lease lifecycle; it does not yet prove direct SDIO/FAT unmount/release because that backend is not implemented.

The hp4950 Stock family uses each model's native ARM bootloader with the shared official 2.15 OS image, installs ARMToolbox/library 275 from emulated SD, copies `HPCLANG.HP` through the stock File Manager, enters `ARMTo` / `PrRUN`, and requires payload PASS plus stage teardown and visible Stock return.

## Evidence retention

Machine-readable assertions remain the gate. Images are review/debug evidence rather than substitutes for assertions.

Canonical summaries are published to `ci-evidence` and standard Actions artifacts retain per-run evidence. Keyboard progression and Font Gallery contact sheets/summaries use separate paths. Concurrent `ci-evidence` publication still needs a race-safe update strategy; an evidence-branch push conflict does not invalidate an already-uploaded Actions artifact or a passed calculator assertion.
