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
- read-only calculator-object capability discovery: unsupported families/direct launchers reject the Stock-only stack view cleanly, while Stock HP48/HP4950 can observe depth and owned level-1 snapshots without changing stack depth;
- machine-readable `0x50415353` plus visible runtime evidence.

The Direct emulator gate also verifies that temporary mappings and the SWI vector are restored before the stock calculator resumes. Stable S3C2410 LCD-controller state is compared before harness cleanup so display-state damage cannot be hidden by host-side restoration.

## PASS, control-flow return, and visible return

These are separate claims and must remain separately observable:

1. **HPC1 PASS** proves the payload completed its assertions.
2. **Control-flow/state return** proves temporary hpclang mappings/state were removed and execution returned to the stock environment.
3. **Visible Stock return** proves the stock OS can process a normal UI action and redraw the LCD after return.

A nonblank `return.pgm` that still contains hpclang's last frame is insufficient for item 3. The harness must not manufacture success by copying the pre-launch framebuffer back. HP39 real-hardware validation uses **APLET** as the harmless post-return stock action.

## M7.3 object, conversion and stack-mutation evidence

M7.3 keeps Saturn addresses and mapped ARM aliases runtime-private. Public applications see opaque owned `struct hpclang_object` snapshots plus typed conversions and stack services. The object parser remains fail-closed: it snapshots only formats whose extent can be proven from verified fixed size, self-relative length, SEMI-terminated composite structure, tag+embedded-object structure, or another documented boundary rule.

The stack/object evidence progressed through these stages:

- **Core #285 (`35431683458`)** and the successful Stock hp48/hp4950 jobs in **Runtime #232 (`35431683496`)** established the first non-mutating `DSKTOP`/D1 + `EDITLINE` stack view, level-pointer resolution, proven-boundary parsing and owned snapshots;
- **Runtime #237 (`35435438471`)** is fully green after stage1 learned to dynamically recognize the hpclang Launcher-V3 DOCSTR and normalize the historical HPGCC 1.1 launcher level-1 bias, then restore it after HPC1 returns;
- **Core #296 (`35439583956`)** is fully green for the bounded mutation core and exact integer/raw-string conversions. In **Runtime #241 (`35439583995`)**, the canonical build, Direct hp39/hp48/hp4950, Stock hp48 and Stock hp4950 jobs all succeed. The Stock RPL witness constructs owned CHAR A/B snapshots inside HPC1, performs push → replace → peek → push → pop → drop, and requires the final user-visible depth to equal the initial depth. The Stock hp39 job was cancelled by a later push and is deliberately not counted as RPL-mutation evidence;
- on `87ce4349`, the Runtime/unit job in **Core #297 (`35440008729`)** passes the classic DOREAL conversion tests and ARM hardware-validation build, while the canonical-build job in **Runtime #242 (`35440008714`)** also succeeds.

Named-store and HP39 native-object validation then close the M7.3d read side:

- **Core #311 (`35452252332`)** and fully-green **Runtime #254 (`35452252452`)** on `9111b12` established the HP48/HP4950 read-only named-store implementation across Direct hp39/hp48/hp4950 and Stock hp39/hp48/hp4950. HP49g+/50g use the real USEROB `X50NG-VV.FAT` DOCSTR rather than the SD transfer file `HPCLANG.HP`: two independent HOME iterators must find it, classify it as a non-directory string, and `hpclang_object_store_get()` must return an owned zero-length string snapshot. The same path also checks `ENOTDIR` for child-open on the string and `ENOENT` for a missing name. The final HP48 witness was later reduced to the small transferred `ZZE2E` DOCSTR so object-copy validation does not depend on allocating an owned copy of the roughly 74 KiB launcher;
- HP39 Aplet-variable research after Runtime #254 recovered the current `ApletVars# = EIGHT` schema contract, preserved genuine extended-ID source references, and pinned the standard `NoteText` descriptor triple. **Runtime #304 (`35596158126`)** remains useful negative evidence: routing genuine `xNoteText #2C812` through ordinary Stock EVAL produces the same private `B9` result on HP39gs and HP40gs, proving that EVAL is not the generic descriptor-recall protocol;
- the successful path instead quotes the genuine extended variable ID into Stock and invokes Stock `@`, so the calculator follows descriptor word 0 through its own supported recall path. The HP39 portal enters with `GETPTR`, commits the final live RPL pointers with `SAVPTR`, and restores the suspended runstream pointer before returning to ARM. The **Stock / hp39 job in Runtime #320** validates this descriptor-driven recall on both HP39gs and HP40gs using `NoteText`: both reach HPC1 PASS, `stock-hp39.json` passes, and the complete 52-frame keyboard progression returns cleanly. Runtime #304 is therefore superseded as a blocker, but retained as evidence against the discarded EVAL shortcut;
- the HP39 side also has a strict private `HP39AscA` directory parser with unit coverage against the original HPGCC 39G-support `HP39DIR.000` / `HP39DIR.CUR` records and malformed/truncated/overflow cases. Those files remain typed transfer-protocol evidence rather than an internal HOME namespace. ROM analysis likewise keeps `DoRecv/GetOb`, `DoGetObFrSto`, `OpenIO` and `StdIOProc` classified as System-RPL/DOCOL transfer/I/O controllers rather than direct ARM object-fetch entry points;
- no general named object-store write gate is claimed. HP48gII/49g+/50g named stores remain read-only; HP39 descriptor store-handler words are known but do not constitute a validated generic variable-write path. This capability is deliberately deferred beyond the closed M7.3 HPGCC 1.1/2.0 parity baseline and remains distinct from the already-validated HP48/HP4950 stack mutation capability.

M7.3e compatibility-adapter validation is closed above that read core:

- **Core #396 (`35739959095`)** is fully green with the first high-level
  HPGCC `satdir` adapter, safe `saturn.h` subset, calculator-String
  constructor, host units, unified ARM runtime build, and HP39 package build;
- later focused Core gates add historical textual numeric pushes, the exact ZINT
  constructor/exchange, the representative bundled HPGCC `stack.c` flow,
  classic REAL push, and failure-safe `stdargs`. Core #399/#400/#402/#404
  cover those steps through host units and ARMv4T hardware-validation builds;
- host units deliberately exercise malformed/truncated/overflow object extents
  and verify that failed ZINT/REAL typed pops and failed `stdargs` conversion
  do not consume the calculator stack;
- **Storage #125/#126** keep the normal Stock storage/return path green while
  the shared ARMToolbox stage-alias teardown monitor is made race-safe by
  recording the pre-PrRUN Stock L1 baseline;
- the final closure gate is **E2E / Runtime #338
  (`35812005032`) on `ba2c0398`**. Build, Direct hp39/hp48/hp4950,
  Stock hp39, Stock hp48, Stock hp4950, and evidence publication all pass.
  The focused Stock adapter witness preserves incoming stack depth across
  ZINT/REAL/String exchange and runs successful `stdargs` only on an
  initially empty stack. The existing canonical mutation witness independently
  restores push/replace/peek/pop/drop to the original depth;
- HP48's final successful named-store snapshot uses the bounded `ZZE2E`
  DOCSTR with payload `HPCLANG-E2E`. The transferred launcher remains visible
  to enumeration but is no longer used as an approximately 74 KiB owned-copy
  stress fixture. The long-running HP48 Kermit flake is closed at its actual
  synchronization boundary: the UI sequence now waits on a host marker written
  only after the Kermit process completes successfully, while the calculated
  timeout is merely a watchdog ceiling. This replaces the older fixed-delay
  workaround and prevents transfer/UI key races. The host sequence then proves
  stage baseline → active alias → HPC1 PASS → alias release → settled Stock
  return;
- HP39/40 canonical Runtime similarly gates the UI on actual serial-disk
  lazy-fetch completion. Sparse post-gate ENTER retries cover Stock's
  asynchronous install/UI settling, and the harness suppresses later sequence
  key-downs after KBRD. Runtime #338 therefore reaches HPC1 PASS and clean
  return on both HP39gs and HP40gs without changing the descriptor-recall/object
  implementation. The separate **Font Gallery #122** remains red on its HP39
  jobs and has not yet been rerun with an equivalent final lazy-fetch/settling
  fix; HP48gII and hp4950 Gallery jobs in that run are green.

**Runtime #326 (`35739887799`)** is retained only as historical diagnostic
evidence and is superseded as the closure gate. Its pre-HPC1 launcher failures
and partial keyboard run are not adapter semantics; the later focused evidence
above resolves those harness paths instead of manufacturing success through
repeated unchanged reruns.

The mutation allocator does not trust Stock `AVMEM` as a complete physical-memory description. Stage1 maps hpclang-owned ARMToolbox pages back into the live Saturn address space and publishes conservative TEMPOB/user-stack fences through a tagged private platform cookie. Expected `ENOSPC`/invalid-level failures are checked before the first mutation. `AVMEM` is updated relative to the existing Stock reservation rather than recomputed from raw `DSKTOP-RSKTOP` geometry, so launcher/native reservations are not accidentally made available.

Conversions are explicit about representation loss: BINT/ZINT → `int64_t` is exact or `ERANGE`; classic DOREAL → `double` is approximate and returns `ERANGE` instead of silently clamping overflow or nonzero underflow to the old HPGCC behavior; CHAR and DOCSTR expose calculator 8-bit codes/bytes and are not mislabeled as UTF-8.

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

M7.2 keeps standard byte-stream I/O in picolibc, but the production filesystem backend is now hpclang-owned FatFs rather than Stock KOS file calls. The first storage operation acquires a whole-process SD namespace lease; from then until shutdown, stdio, metadata, capacity, rename/mkdir/rmdir and directory traversal all use the same direct S3C2410 SDI + FatFs stack.

KOS remains only at the Stock ownership boundary. Insertion and write-protect state are sampled once before takeover because repeated `CHECKINSERT` is not return-safe on the HP49g+ path. On shutdown hpclang closes direct descriptors, unmounts FatFs, releases the controller, invokes KOS syscall 19 (`ResetSD`), invalidates the cached media sample, and returns to Stock. Ordinary file data and namespace operations never alternate between KOS and FatFs within the process.

The focused Storage payload covers:

- missing-file `ENOENT`;
- create/truncate, read/write/flush, seek/tell, reopen and append;
- path size/type/portable FAT attributes and fixed last-modified civil time;
- free bytes and direct block-device total capacity;
- rename, mkdir and rmdir;
- UTF-8 long filename enumeration with preserved stored case;
- case-insensitive FAT lookup;
- two simultaneously live independent directory iterators;
- ordinary `fopen()` while an iterator is live;
- final stream access before runtime teardown;
- deterministic `ENOSYS` for HP39/40 and HP48 byte-stream operations.

Storage #22 (`35378151818`) remains the historical M7.2b evidence for the earlier KOS-backed metadata/stream bridge. Storage #23 independently established the Stock `ResetSD` reinitialization half. The later M7.2c runs, culminating in Storage #30, established the direct SDIO/FatFs backend, owned iterators, namespace mutation, capacity and process-wide lease. Final M7.2 closure is **Storage #53 (`35428824540`)** on `c4b827fe`: Build, Direct hp39/hp48/hp4950 and Stock hp4950 all pass after ordinary KOS file I/O is removed, mounted-volume capacity replaces whole-card capacity, the HPGCC filesystem adapter flow is exercised, and POSIX `ftruncate()` extension is zero-filled. **Core #254 (`35428824495`)** also passes on the same head.

The timestamp fixture pins `HPCLANG.TST` to `2024-01-02 03:04:06`; only the resulting FAT last-modified civil fields are public. Creation/access timestamp semantics remain outside the public contract.

## Evidence retention

Machine-readable assertions remain the gate. Images are review/debug evidence rather than substitutes for assertions.

Canonical summaries are published to `ci-evidence` and standard Actions artifacts retain per-run evidence. Keyboard progression and Font Gallery contact sheets/summaries use separate paths. Concurrent `ci-evidence` publication still needs a race-safe update strategy; an evidence-branch push conflict does not invalidate an already-uploaded Actions artifact or a passed calculator assertion.