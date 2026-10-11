# HPGCC compatibility inventory

This page records the M7 source-compatibility inventory. Historical names are inputs to the compatibility layer, not automatically names for the hpclang core API. For current implementation status, see [API reference and HPGCC parity](api-reference.md).

## Sources and method

Primary function-level evidence comes from complete historical distributions:

- HPGCC 1.0.3 beta;
- HPGCC 1.1 with 39g support;
- HPGCC 2.0 SP2;
- HPGCC3 R004.revA.

Installed/public headers were mechanically inventoried, while source trees and examples were used to distinguish declarations from actual platform behavior. Older SourceForge tags remain useful cross-version evidence. HPGCC3 is a design/compatibility data point, not an hpclang ROMLIB ABI target.

Primary references include the SourceForge HP-GCC releases, hpcalc's HPGCC 2.0 SP2 archive, and HPGCC3 documentation.

## Public API stability

The historical releases are more source-stable than their runtime implementations suggest.

| Public surface | Comparison | Exact common signatures |
| --- | --- | ---: |
| `hpgraphics.h` | 1.0.3 → 1.1 | 55 / 55 |
| `hpkeyb49.h` | 1.0.3 → 1.1 | 11 / 12 |
| `hpscreen.h` | 1.0.3 → 1.1 | 9 / 9 retained |
| `kos.h` | 1.0.3 → 1.1 | 15 / 15 |
| `syscall.h` | 1.0.3 → 1.1 | 9 / 9 |
| `hpgraphics.h` | 1.1 → 2.0 | 59 / 59 |
| `hpgraphics.h` | 2.0 → HPGCC3 | 59 / 59 |
| `hpkeyb49.h` | 1.1 → 2.0 | 17 / 17 |
| `hpscreen.h` | 1.1 → 2.0 | 9 / 9 |
| `ggl.h` | 1.1 → 2.0 | 28 / 28 |
| `hpsys.h` | 1.1 → 2.0 | 40 / 40 |
| `saturn.h` | 1.1 → 2.0 | 49 / 49 |
| `satdir.h` | 1.1 → 2.0 | 29 / 29 |
| `hpstdio.h` | 1.1 → 2.0 | 37 / 37 |
| stdio surface | 2.0 → HPGCC3 | 40 / 40 |
| `fsystem.h` | 2.0 → HPGCC3 | 41 / 41 |
| `kos.h` | 2.0 → HPGCC3 | 33 / 33 |

HPGCC3 adds two public filesystem handle helpers beyond HPGCC 2.0: `FSGetFileFromHandle` and `FSGetHandle`.

Compatibility consequence:

- HPG is a strong broad source-compatibility target;
- standard C I/O should use standard C names rather than hpclang-specific duplicates;
- raw KOS/Saturn/MMU behavior stays below cleaner hpclang services;
- keyboard and HPG mutable-state semantics belong in adapters above the clean core.

## HPGCC 1.0.3 → 1.1

The official 1.0.3 distribution already ships HPG with 55 public functions. HPGCC 1.1 preserves all 55 declarations and adds only `hpg_font_get_advance`, `hpg_font_get_height`, `hpg_get_height`, and `hpg_get_width`.

Keyboard grows from 12 to 17 public functions. Eleven old declarations remain exact; `keyb_getkey(int wait)` changes return type from `char` to `int`, and matrix helpers are added.

The legacy screen API drops the three older minifont helpers `getMFData`, `writeLetMF`, and `writeStringMF`. KOS expands significantly while retaining its prior common declarations.

This confirms that the principal HPG compatibility surface is already mature by 1.1, while keyboard/system compatibility needs more care around the 1.0 boundary.

## Historical example usage

HPGCC 1.1/2.0 examples heavily use:

- HPG geometry/text: `hpg_fill_rect`, `hpg_set_color`, `hpg_draw_text`, `hpg_clear`, `hpg_draw_line`, `hpg_flip`;
- keyboard polling: `keyb_isAnyKeyPressed`, `keyb_waitKeyPressed`, `keyb_isKeyPressed`, `keyb_isON`;
- timing/power: `sys_waitRTCTicks`, `sys_RTC_seconds`, `sys_slowOn`, `sys_slowOff`;
- low-level GGL in performance-sensitive code.

The HPGCC 1.1 Programming Guide documents normal SD-card `fopen/fread/fwrite/...` use for HP49g+ and warns against treating HP48gII/39g+ as the same filesystem platform. Public declaration stability therefore does not imply universal platform semantics.

## Keyboard

The 1.1/2.0 keyboard API is source-stable but mixes layers that hpclang separates:

1. raw physical matrix/state;
2. translated key identity/events;
3. blocking/repeat/text-plane policy.

Compatibility decision: **Adapter**, with selected queries becoming effectively Direct.

The hpclang core already supplies raw state, 51-key physical identity, DOWN/UP/REPEAT, and modifier state. Historical key-plane/text translation and blocking helpers remain adapter work.

HPGCC3's asynchronous queue is useful comparative evidence but does not require hpclang's stock-hosted backend to install global keyboard IRQ ownership.

## Graphics: HPG and GGL

HPG is the principal graphics compatibility target. Its 59 public function signatures are identical across HPGCC 1.1, 2.0 and HPGCC3.

Compatibility decision:

- **Broad source-compatible Adapter** for HPG after clean primitives are stable;
- **Migration/adapter-later** for raw GGL and representation-specific screen helpers.

The hpclang core already provides opaque physical/software surfaces, clipped pixel/line/rectangle/circle/polygon operations, cross-level blit, native/generated fonts, and UTF-8/codepoint text drawing. That is enough clean machinery for many HPG wrappers without reproducing `hpg_t` as the core state model.

Historical mutable color/font/pattern/clip/PAINT-XOR state belongs in the HPG adapter.
M7.5 now provides physical 2/4/16-level sessions, double buffering, bounded frame
synchronization and six status segments through the clean display service.
The runtime owns contiguous DMA allocations, cache publication and LCD restoration.
`tests/e2e/display/hpg-prototype.c` exercises intersecting clips, PAINT/XOR,
repeating surface patterns, clipped line masks and transparent RGB XPM import
using only public surface operations. Its one-character XPM subset is a feasibility
witness; the broad historical parser and installed HPG spelling remain M8 work.
No missing low-level drawing primitive was found by these witnesses.

The source review includes HPGCC 1.0.3, HP39-enabled 1.1, 2.0 SP2 and HPGCC3 R004,
plus newRPL's HP50g LCD implementation. The S3C2410A register contract takes
precedence where old code writes status bits, forces LCD enable or spins without
a timeout. hpclang's flip stops DMA at an observed frame boundary rather than
changing live frame addresses with prefetched FIFO data. Emulator acceptance
does not establish real-panel flicker or luminance. See the
[display contract](api-reference.md#physical-display-ownership).

## Native calculator fonts: important platform split

This area needs careful terminology because historical HPG contains **two different notions of “font”**:

1. HPG's own bundled `hpg_get_minifont()` / `hpg_get_bigfont()` bitmap tables used by the HPG drawing library;
2. calculator-native font helpers under `hplib/stdio`, which locate fonts owned by the stock calculator environment.

The first is legacy HPG compatibility data. The second is the relevant evidence for hpclang's borrowed native-font implementation.

### HP48gII / HP49g+ / HP50g path

Historical HPGCC explicitly resolves calculator-native fonts through the logical Saturn address map:

- `hplib/stdio/get_minifont.c` returns `sat_map_s2a(0x812c8)` and documents the stable minifont entry in RAM; it also carries the HP48gII compatibility note;
- `hplib/stdio/get_bigfont.c` starts at `sat_map_s2a(0x84d82)` and skips the object/name header;
- `hplib/saturn/sat_map_s2a.c` implements that conversion as `_saturn_cpu->Read[sat_addr >> 12] + (sat_addr & 0xfff)/2`.

So for this family, using the launch-preserved `SAT_CPU->Read[]` logical mapping is directly grounded in HPGCC source behavior.

### HP39/40 path

HPGCC's HP39 support is not wired through those same native-font locator helpers. The HP39 Aplet/Library 275 support carries an HP39-specific ROM entry map; `SUPROM39.A` defines `Put3x5` at `#3F20A` and `Put5x7` at `#38C7E`, and the HP39 library code invokes those stock text routines. Independent HP39 UI reverse engineering additionally identifies the dedicated EditLine 6×10 table, so hpclang exposes it by its own concrete name rather than pretending HP39 has the HP48-compatible bigfont.

This is strong historical evidence that HP39's text environment is a separate stock-ROM path rather than merely “the 48/49/50 font helper with another model ID.”

However, evidence must not be overstated: HPGCC does **not** provide the hpclang physical raster constants `0x07E695` and `0x07C651`. Those exact HP39 mini/main table bases, record layouts, and the need to bypass the live logical bank for fixed raster assets were established independently by stock-ROM reverse engineering and emulator pixel validation.

The production HP39gs/40gs implementation now exposes only the two required physical NOR pages, `0x0007e000` and `0x0007f000`, as uncached 4 KiB small pages inside the bootstrap-private prefix of hpclang's own code coarse table. Runtime receives the exact private window base only through the opaque `platform_cookie`; no physical ROM address or private mapping becomes public font ABI.

This replaced an experimental long-lived high-L1 section alias. The transient `L1[A0]`/section mapping was sufficient to acquire the real HP39g+ NOR, but the one-shot dumper source has since been retired. The mechanism was not safe as persistent runtime storage on HP39gs/40gs: diagnostics proved that the active LCD framebuffer can occupy bytes inside the physical L1-table region and overwrite a descriptor word during ordinary drawing. A descriptor that reads zero is therefore not necessarily a safe long-lived slot.

The verified 1 MiB HP39g+ D1.55 full-NOR fixture independently establishes main/mini/EditLine at physical nibble addresses `0x0DC096` / `0x0DE0DA` / `0x0DCE2F`, in pages `0x6e`/`0x6f`. The runtime uses those pages through the same private window. All three complete tables match the reviewed gs atlas, including the mini extended-character map. The model-specific ROM pixel oracle and new Stock USB Gallery entry cover this extension; formal acceptance is pending. These offsets come from ROM analysis, not historical HPGCC declarations.

Compatibility consequence:

- the common hpclang font API correctly hides the family-specific lookup mechanism;
- HP39 fixed native raster access may use bootstrap-private physical flash mappings while ordinary Saturn RAM/object access still uses `Read[]`;
- HP48/49/50 native lookup should retain logical `Read[]` behavior;
- neither physical ROM addresses, private code-window VAs, `platform_cookie` meaning nor `Read[]` pointers belong in the public font ABI;
- historical HPG bundled bitmap tables are not silently redistributed as hpclang assets.

The production Font Gallery gate covers this boundary directly. Fully-green run **35298949457 / #112** covers all Direct families plus HP39gs, HP40gs, HP48gII, and HP50g Stock controls. HP39gs/40gs Direct traverse **248 entries / 496 captures** including EditLine; HP48gII/49g+/50g remain **247 / 494**. Both HP39 Stock targets pass the independent EditLine fingerprint and pixel oracle.

## Font/text compatibility boundary

Historical HPG fonts expose two standard 256-entry byte-indexed fonts plus caller-owned custom descriptors, fixed-advance text layout, and mutable font selection through `hpg_t`.

hpclang's canonical font system is intentionally broader and cleaner:

- calculator-native borrowed fonts;
- generated custom fonts with arbitrary metrics and Unicode mappings;
- `STATIC_AUTO`, `STATIC_EXPLICIT`, and `FULL` link-time repertoire selection;
- UTF-8/codepoint rendering;
- no hidden mutable graphics context in the core.

Compatibility decision: **Adapter**. HPG's exact byte-indexed/custom-descriptor behavior can be reproduced above hpclang surfaces/fonts without making it the canonical representation.

## Legacy screen API

Simple semantic operations such as clear/invert can be Direct/Adapter wrappers. Raw LCD pointers, packed sprite formats, display-plane selection, and direct memory-format assumptions are Migration/low-level concerns and must not define the opaque surface ABI.

## Time and system services

Historical `hpsys.h` mixes delays, timer setup, LCD hardware state, clock changes, interrupts/MMU/physical allocation, tones, and libc-like helpers.

Compatibility decision is split by responsibility:

- time/delay: **Adapter** to unit-explicit monotonic time;
- LCD mode/controller: **Migration**, except safe behavior exposed by display services;
- interrupts/MMU/physical allocation: **Unsupported as routine application API** or internal/unsafe;
- standard libc: **Direct through picolibc**, not duplicate hpclang wrappers;
- tone/beep: clean PWM configuration/timing/restoration passes formal M7.4 CI;
  audible output still requires a physical-hardware witness.

M7.4 provides civil-time reading/conversion and cooperative
one-shot/periodic timers. The historical RTC implementations read separate BCD
registers without a coherent-snapshot check; 1.1/2.0/3 also mark the hardware
weekday field as unreliable. The clean datetime API validates a stable sample,
computes weekdays from the Gregorian date and leaves timezone policy to the
application. Its conversion range is 1–9999; the physical RTC is 2000–2099.
Clock setting is implemented with monotonic-clock rebasing: forward/backward
adjustments preserve existing deadlines. RTC control/alarm configuration is
retained; alarms observe the newly selected civil time.

HPGCC3 timed events invoke callbacks from timer interrupts. The clean timer API
reports expiration counts when the application polls, preserving the periodic
schedule after a late poll. Applications dispatch their work in their own event
loop. This provides deadline and periodic notification without installing IRQ
handlers; it does not claim historical interrupt-callback compatibility.

The clean `hpclang_sound_play()` service uses Timer2 PWM instead of historical
whole-port GPIO bit-banging. It preserves the shared prescaler and restores the
borrowed timer, pin function and interrupt mask. Busy Timer2/DMA ownership is
reported as `EBUSY`; cancellation runs on the application stack, and nested
playback is rejected. Tests observe PWM configuration, elapsed duration,
cancellation and restoration. Emulator success does not establish audible
frequency or speaker volume on physical hardware.

Stock contrast queries use the historical KOS getter on all six validated models.
USB voltage detection is read-only and deliberately does not claim battery
percentage or power-source selection. Capability discovery distinguishes model
support from a missing RPL launch context; resource/media errors remain the
responsibility of the individual service call.

Bounded UTF-8 editing and length-bounded message rendering provide the clean
primitives for historical line-input/error helpers. Buffer ownership, cancellation
and errors are explicit. HPG Alpha/Shift translation, modal screen policy,
`readint`/`readline` spelling and global stdio routing remain adapter/UI work.

## File I/O and standard C I/O

The stable historical standard-C names remain standard C where semantics fit. HP49g+/HP50g route ordinary `open/read/write/lseek/unlink` and picolibc `FILE *` operations through one process-wide hpclang-owned FatFs namespace. Insertion/write-protect sampling reads configured GPIO inputs before and during ownership; Stock KOS is retained for `ResetSD` after release. Observed removal or a block-transfer failure invalidates old file/directory objects, and new path operations can remount. Old handles must be closed after `EBADF`; their pending writes are not flushed to a newly mounted volume. This does not establish detection of a complete remove/reinsert cycle between observations or physical hot-plug safety.

M7.2's clean core covers path metadata, masked FAT attribute mutation, modification/creation civil time and stored access-date queries, media state, free/total capacity, rename/mkdir/rmdir, UTF-8 LFN, and independent owned directory iterators. HP39/40 and HP48 continue to return clean `ENOSYS` rather than treating calculator-object stores as files. Access-date queries do not imply automatic atime maintenance: reads leave the stored date unchanged and FatFs clears it on writes. Creation/modification queries expose two-second precision; missing dates are explicitly invalid.

The M7.2d source adapter now provides a deliberately bounded historical surface:

- `hpstdio.h` delegates standard I/O to libc and maps `fdelete()` to the clean stream namespace;
- `fsystem.h` covers useful file, seek/read/write, rename/delete, mkdir/rmdir, volume-space, metadata and directory-iteration patterns over the clean core;
- common visible `FS_FILE` metadata fields are retained for source ports, but the historical binary layout and direct FAT/controller state are not reproduced;
- the single real hpclang SD volume accepts legacy SD path aliases while multi-volume mutable state is not reintroduced.

Raw KOS filesystem calls, raw filesystem handles, global `chdir/getcwd` state, FSystem sleep/wakeup/controller ownership, and timestamp setters remain Migration/Internal rather than canonical compatibility APIs.

## HP39 calculator objects

HP39gs/40gs have typed internal calculator objects, but the native storage model is split across Stock catalogs/contexts rather than one HOME-like directory hierarchy. ROM evidence establishes the current structure: `GetApDirList` exposes the Aplet catalog; `CatPgm` enumerates `PGMCXT`; `CatNot` enumerates `NOTESCXT`; ordinary variable operations such as `StoVar` run inside the caller's active Aplet/topic context. Aplet selection/creation updates the Program/Note context pointers together with the active Aplet state.

`HP39DIR.000` / `HP39DIR.CUR`, `HP39AscA`, and `HP39BinX` are a different layer: the typed external Disk Drive/Conn3x transfer protocol used to import/export Aplets, Programs, Notes and related records. Historical `DoGetObFrSto`, `DoSendObToSto`, `OpenIO`, and `StdIOProc` belong to this transfer/I/O path and must not be treated as proof that `HP39DIR.*` is the internal calculator namespace.

Compatibility code may preserve that transfer protocol privately, while canonical internal object access goes through the Saturn/System-RPL portal and native catalog/context primitives. The bridge normalizes Aplet, Program and Note catalogs into owned list snapshots without exposing ROM addresses or leaving its temporary RPL stack cell behind. Public Program/Note services live in `hpclang/catalog.h`; Aplet lifecycle services live in `hpclang/aplet.h`. Their M7.3 acceptance is still being validated. HP39 is not flattened into `FILE *` or a fabricated HOME tree.

## Saturn / RPL object bridge

The common 1.1/2.0 `sat_*` surface mixes two materially different layers: application-level object/stack/directory jobs and raw Saturn-address mechanics.

The original 1.1/2.0 `satdir.h` makes that split explicit. Its high-level API maintains an **internal C-side cwd** and provides path get/RCL/search plus REAL fetch helpers; it does not change the calculator's Stock cwd merely to navigate. Below that are low-level helpers such as `sat_skipob`, `sat_dirfindfirst`, `sat_dirfindnext` and `sat_itemgetname` that expose raw Saturn addresses and directory records directly.

Compatibility decision and current implementation:

- **Adapter** for high-level stack/object and `satdir` application behavior.
  The bounded M7.3e layer is implemented above owned `hpclang_object`
  snapshots and explicit `hpclang_object_store` handles. It preserves
  adapter-local cwd semantics, exact object lookup, ancestor RCL lookup,
  recursive HOME search and REAL fetch behavior without changing Stock cwd;
- historical `SAT_OBJ_DSCR.addr` is retained only as an adapter-private opaque
  token. It is deliberately **not** a real Saturn address; source that extracts
  it and feeds it into raw `sat_peek()`-style code is a Migration case rather
  than a hidden escape from the owned-object boundary;
- the bounded `saturn.h` application subset covers
  `sat_stack_init/exit/depth/drop/dropn`, String push/pick/pop, historical
  textual int/double pushes, exact ZINT push/pop, classic REAL push/pop, CHAR
  push, and failure-safe `sat_stack_pop_stdargs` /
  `_sat_stack_pop_stdargs`. String/ZINT/REAL creation goes through clean owned
  constructors before the clean stack mutation path;
- **Migration/Internal** remains the classification for low-level
  address-returning `satdir`, raw memory mapping/peek/poke/memmove, borrowed
  stack descriptors, native-BCD complex/vector/array helpers and raw list
  builders. M7.3i adds bounded `sat_stack_swap/nip` adapters over the clean
  transactional reorder service and HPGCC 2.0 `sat_pushtruefalse` over native
  Boolean exchange; their formal CI acceptance is complete on `6a7f9a28`;
- HPGCC 1.1/2.0 adapter parity does not itself require general named-object
  mutation because historical `satdir.h` is read/search/recall oriented.
  However, hpclang's M7 capability plan still includes that write side as
  **M7.3f**: HP48gII/49g+/50g named put/remove plus a validated HP39 generic
  descriptor-store bridge. Those clean-core mutations remain separate from the
  bounded historical adapter surface.

HP39 follows the same capability boundary without pretending to be a classic
HOME tree. Native Aplet/Program/Note structures are snapshotted directly.
Descriptor-driven Aplet Variable recall is closed by quoting the genuine
extended variable ID into Stock and invoking Stock `@`; Runtime #320 validates
the standard `NoteText` witness on HP39gs and HP40gs. The older Runtime #304
`xNoteText #2C812`-through-EVAL `B9` result remains useful negative evidence
against that discarded shortcut.

Adapter validation progresses through Core #396/#399/#400/#402/#404 and the
focused calculator witness. The historical M7.3a–e gate is **E2E / Runtime #338
(`35812005032`) on `ba2c0398`**, fully green across Build, all Direct
families, all Stock families, and evidence publication. The Stock witness
round-trips ZINT/REAL/String values without changing the incoming depth, uses
`stdargs` only on an initially empty stack, and returns cleanly to Stock.
HP48's successful named-object get uses a bounded `ZZE2E` DOCSTR rather than
copying the large launcher merely as a test fixture.

**M7.3a-e are complete at the intended HPGCC 1.1/2.0 object/stack adapter
baseline.** M7.3f public named-store and native-variable writes are implemented and accepted in the clean core.
Historical raw signatures remain Migration interfaces. Clean directory, composite,
precision and stack-reorder equivalents remain required M7.3 work.

HPGCC3's `SAT_OBJECT` direction remains useful design evidence but is not an ABI target; hpclang deliberately replaces borrowed raw addresses with owned snapshots.

## KOS and direct stock-OS calls

Raw KOS entry numbers and calling conventions remain **Migration/Internal**. Portable device/time/storage/display services should hide them. A low-level escape hatch may be considered only for real use cases that cannot be represented cleanly.

## win widget/event library

The large HPGCC 1.1/2.0 widget family is **Migration initially; adapter later if real ports justify it**. Recreating historical widgets before stable display/input/time/object services would invert the dependency order.

## Compatibility matrix

| Historical area | Core hpclang direction | Compatibility class | Priority |
| --- | --- | --- | --- |
| standard libc/string/math | picolibc/libm | Direct | implemented/core |
| keyboard `keyb_*` | raw state + event service | Adapter | M7/M8 |
| HPG graphics `hpg_*` | display/surface/font service | Adapter | M8 |
| legacy screen helpers | display service | mixed | M8/later |
| time `sys_*Ticks*` | monotonic time/deadlines | Adapter | M8 |
| standard stdio | picolibc + hp4950 storage backend | Direct | implemented on hp4950 |
| HPGCC2/3 `FS*` | bounded source adapter above owned FatFs storage | Adapter | M7.2d implemented; Storage #53 green |
| Saturn/RPL `sat_*` | typed object/stack bridge | Adapter | M7.3e bounded application subset complete; Runtime #338 green; M7.3f public mutation implemented and accepted |
| KOS/syscall internals | platform/private service | Migration/Unsupported | later |
| `win*` widgets | future UI toolkit | Migration, adapter later | later |
| raw MMU/interrupt APIs | internal only | Unsupported normal API | none |

## Design consequence

Historical source compatibility remains important, but it must sit on the clean dependency stack already established:

```text
device + time
keyboard/events
display/surfaces
native/generated fonts + text
standard stream I/O where real byte-stream storage exists
calculator-object/Saturn bridge
--------------------------------
HPGCC compatibility adapters
```

In particular, the verified HP39-vs-HP48/49/50 native-font difference belongs below the public font interface and should never leak into source-compatible application code.

### Current bitmap-only font closure status (2026-09-14)

Canonical bundled assets are bitmap-source only. The generated catalog is **245 faces / 1,639,485 mapped glyphs / 3,472 assembly sources** with **245/245** source-to-generated parity. Font Assets **#24** is green with no regeneration drift; the general TTF importer is removed.

Final curation includes Cubic 11, ChillBitmap 7px, and QuanPixel/May7 with provenance retained. Gallery labels/specimens have been strengthened for Ark Pixel regional variants, width-sensitive Latin, punctuation/digits, accents/symbols, and supported CJK/Japanese/Korean repertoires.

`E2E / Font Gallery` **35298949457 / #112** is the historical M7.1 fully-green closure gate. It is green across Build, universal x50ng, all three Direct families, and all four Stock Gallery targets. HP39 summaries now show minifont, main, EditLine, and generated control; HP48/HP50 remain minifont, bigfont/default, and generated control.

The tested font/model slice is accepted. Repository integration subsequently passed on main. The HP39g+ native-font extension has a separate Direct/Stock ROM-pixel validation and CI acceptance gate.

## Full declaration counts and remaining capabilities

The source set is the complete HPGCC 1.0.3 beta, HPGCC 1.1 with 39g support,
HPGCC 2.0 SP2 and HPGCC3 R004.revA distributions. The C-header pass removes
comments/preprocessor directives, joins multiline declarations, de-duplicates
function names, and compares whitespace-normalized declarations. Macros,
conditional availability, ABI compatibility and C++ classes require separate
review; a declaration count proves none of those.

| Header / comparison | Unique functions in each version | Identical common declarations | Finding |
| --- | --- | --- | --- |
| `hpgraphics.h`, 1.0.3 → 1.1 | 55 → 59 | 55 | Existing count confirmed; four metric/geometry helpers added |
| `hpgraphics.h`, 1.1 → 2.0 → 3 | 59 → 59 → 59 | 59 at each transition | Existing count confirmed; wrappers still not implemented by hpclang |
| `hpkeyb49.h`, 1.0.3 → 1.1 | 12 → 17 | 11 | `keyb_getkey` changes return type; five names added |
| `hpkeyb49.h`, 1.1 → 2.0 | 17 → 17 | 17 | Existing count confirmed |
| `hpscreen.h`, 1.0.3 → 1.1 | 12 → 9 | 9 | Three old minifont helpers removed; 9 is the retained subset |
| `hpscreen.h`, 1.1 → 2.0 | 9 → 9 | 9 | Existing count confirmed |
| `kos.h`, 1.0.3 → 1.1 | 15 → 32 | 15 | Existing retained-subset count confirmed |
| `kos.h`, 1.1 → 2.0 | 32 → 33 | 31 | `fdelete` replaced by `kos_fdelete`; `kos_ResetSD` added |
| `kos.h`, 2.0 → 3 | 33 → 33 | 33 | Existing count confirmed |
| `syscall.h`, 1.0.3 → 1.1 | 9 → 9 | 9 | Raw syscall equality is not six-model service availability |
| `ggl.h`, 1.1 → 2.0 | 28 → 28 | 28 | Existing count confirmed |
| `hpsys.h`, 1.1 → 2.0 | 40 → 41 | 40 | `sys_is_slowOn` added; repeated `sys_waitTicks` is counted once |
| `saturn.h`, 1.1 → 2.0 | 50 → 58 | 49 | Nine names added, `sat_moveup` removed; repeated `sat_push_string` counted once |
| `satdir.h`, 1.1 → 2.0 | 29 → 29 | 29 | High-level get/recall/search remains read-oriented |
| `hpstdio.h`, 1.1 → 2.0 | 37 → 40 | 37 | `_scanf`, `_sscanf`, `_vsscanf` added |
| `hpstdio.h` → `stdio.h`, 2.0 → 3 | 40 → 40 | 40 | Existing count confirmed; console input semantics still need their own comparison |
| `fsystem.h`, 2.0 → 3 | 41 → 43 | 41 | Two handle helpers added; existing count confirmed |
| HPGCC3 `sat3.h` | 156 | Not a same-header comparison | Must be inventoried by application capability as well as raw representation |

Common-signature counts must not be interpreted as complete capability
coverage. For example, `saturn.h`'s 49 common declarations do not describe all
58 functions in 2.0, and neither count covers HP39's Aplet template protocol.

### Confirmed gaps and source anchors

Archive-relative paths identify evidence without making the old libraries
build dependencies or copying their implementations into hpclang.

| Application capability | Historical evidence | Current hpclang / required follow-up |
| --- | --- | --- |
| Create a directory; remove an entire directory tree | HPGCC3 `libs_workspace/headers/sat3.h`: `sat3_crdir`, `sat3_pgdir`; implemented in `hplib/sat3/sat3_dir.c` | Clean `mkdir/rmdir` accepted with reference/cwd safety; see M7.3g validation |
| Aplet-owned persistent data | 1.1 `sources/HP39-Aplet/Skeleton/hpgccaplet.s`: `CustomData@`, `_Entry`, `_Action`, `_Exit`, `_Reset` | Clean `hpclang_aplet_data_get/set/clear`; all three HP39 models pass the 15-invocation local contract, including native GC, instance isolation, exit/relaunch and menu Reset cancellation/confirmation. Formal CI acceptance is complete on `6a7f9a28`. |
| Lists, real/complex arrays and matrices | 1.1/2.0 `saturn.h` construction/push helpers; HPGCC3 `sat3_createlist`, list operations, `sat3_createrarray/carray/matrix`; corresponding list/matrix sources exist | M7.3i locally implements owned lists, homogeneous numeric arrays and heterogeneous symbolic matrices, with construction, access and independent edits. Raw historical signatures are not thereby declared compatible. See [composite object progress](object-service.md#m73i-composite-objects-and-stack-reorder-in-progress). |
| Complex, identifiers/tags, binary and wider numeric values | HPGCC3 `sat3_createcplx_*`, `sat3_createid_str`, `sat3_createtag_str`, `sat3_createhxs_*`, decNumber/XREAL conversion families | M7.3i adds clean complex, identifier, tagged and width-preserving binary objects, arbitrary-length ZINT decimal exchange and exact 12-digit REAL coefficient/exponent exchange. HPGCC3's reviewed REAL decNumber/XREAL adapters still target classic REAL; distinct EREAL/LONGREAL formats and arithmetic are not implied. |
| Stack rearrangement and Boolean exchange | 1.1/2.0 `sat_stack_swap/nip`; 2.0 `sat_pushtruefalse`; HPGCC3 `sat3_swap`, `sat3_pushtruefalse` | M7.3i implements allocation-free transactional swap/nip and genuine SysRPL TRUE/FALSE exchange. The 1.1/2.0 reorder and 2.0 Boolean push signatures have bounded adapters; formal CI is complete on `6a7f9a28`. |
| HP39 named Program/Note/Aplet lifecycle | Native catalogs and ROM descriptor/context evidence; public generic HPGCC variable-definition CRUD has not been established | `hpclang_catalog_*` implements Program/Note CRUD, including compiler/editor records. `hpclang_aplet_list/clone/remove` provides owned name lists and named native copies, preserves caller contexts, protects active/default Aplets and maintains the cached display list on removal. The dedicated catalog fixture passes on all three HP39 models; CI acceptance is complete on `6a7f9a28`. |
| Port namespaces | HPGCC3 `libs_workspace/headers/sat3.h` describes Port 0 main RAM, Port 1 extended RAM and Port 2 Flash; only libraries/backups, without subdirectories | On 2026-10-04 the maintainer accepted each port's lack of read/write services as an M7.3j limitation. All three retain explicit implementation goals; HOME CRUD does not imply port support. |
| File attribute mutation and timestamp coverage | 2.0 `fsystem.h`: `FSChAttr`, `FSGetCreatTime`, `FSGetAccessDate` | Implemented over the clean metadata API; `FSCreate` accepts read-only/hidden/system/archive. Stored FAT access-date queries do not maintain atime. Persistence/error unit tests and six-model Direct plus HP49g+/HP50g Stock Storage pass locally; formal CI accepted on `6a7f9a28`. |
| Clock, sound, contrast and scheduled events | 1.1/2.0 `hpsys.h`; HPGCC3 `sys.h`, `time.h`: `sys_tm_RTC`, `lcd_setcontrast`, `tmr_eventcreate/kill` | M7.4 accepts civil clock read/set/conversion, cancellable tones, contrast observation and cooperative timers. Contrast mutation and low-level interrupt callbacks are not supplied by these APIs |
| Text input and console behavior | Historical `readint/readdouble/_gets`, key-plane helpers; HPGCC3 `keyb_setalphalock/shiftplane` and message API | Physical DOWN/UP/REPEAT is not text entry, Alpha-lock policy, numeric input or an on-screen console |
| Physical grayscale, flip, compositing and status | HPG/GGL/display headers and implementations | M7.5 implements owned gray2/4/16 sessions, page exchange, bounded frame waiting and six status segments; six-model Direct/Stock pixel/register gates pass locally. Public-API-only prototypes exercise clipping, PAINT/XOR, transparent anchored/relative patterns and RGB XPM. Formal CI accepted on `6a7f9a28`; broad HPG spelling/parser compatibility remains M8 |

HPGCC3 declarations, especially ROMLIB-dependent functions, are evidence of
application requirements, not proof they work on all six Stock ROMs. Likewise,
the HP39 template proves a saved-data channel, not arbitrary creation of system
variable definitions. Every proposed cross-model API needs its own execution
and restoration evidence.

### SD media discovery and replacement

Historical HPGCC 2.0 `sources/fsystem/fsystem.h` and HPGCC3
`libs_workspace/headers/fsystem.h` distinguish `FS_NOCARD` from `FS_CHANGED`
and describe reinitialization after a card change. Those declarations do not
establish a safe Stock syscall sequence for every model. HPGCC3's
`libs_workspace/fsystem/common/sddriver/sddriver.c` reads GPF3 for card presence;
newRPL's `firmware/sys/target_50g/sddriver.c` independently identifies GPF3
presence and active-low GPD3 write protection. x50ng models the same pins.

| Capability | Clean equivalent / model | Adapter | Validation / remaining work |
| --- | --- | --- | --- |
| Query an absent card, insert it, retry before controller acquisition | HP49g+/HP50g: fresh GPIO sampling; no pin reconfiguration or repeated Stock `CHECKINSERT`. HP39/40 and HP48: `ENOSYS` | `FSVolumeInserted` uses the clean media query | Host register-transition and ownership tests; formal regression accepted on `256e8496` |
| Observe write protection before acquisition | HP49g+/HP50g: GPD3 sampled per attempt; absent media does not require a protection read | Existing open/attribute/volume adapters inherit the mounted policy | Host polarity/change/error tests; formal regression accepted on `256e8496` |
| Remove/replace a mounted card and recover stale handles | HP49g+/HP50g: observed absence or transfer failure invalidates the mount; new path operations remount and stale objects fail without flushing old data | Existing stream adapters inherit recovery; broad historical reinitialization adapters remain unimplemented | Host replacement/stale-handle/driver tests and six-model Direct plus HP49/50 Stock Storage pass; formal CI accepted on `56e4a9dd`. Cycles entirely between observations and physical hot-plug validation remain open |

The new pre-acquisition sampling is separate from the accepted metadata and
normal file-I/O contract. It does not claim safe hot swapping of a mounted card.
