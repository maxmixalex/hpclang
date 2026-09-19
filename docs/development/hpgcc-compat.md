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

Historical mutable color/font/pattern/clip/PAINT-XOR state belongs in the HPG adapter. Physical gray4/gray16 and `hpg_flip` remain separately gated because they require contiguous LCD DMA memory and reliable stock-state restoration.

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

This replaced an experimental long-lived high-L1 section alias. The alias mechanism itself was valid — the retained HP39g+ NOR dumper remains useful evidence for a transient `L1[A0]`/section mapping — but it was not safe as persistent runtime storage on HP39gs/40gs. Diagnostics proved that the active LCD framebuffer can occupy bytes inside the physical L1-table region and overwrite a descriptor word during ordinary drawing. A descriptor that reads zero is therefore not necessarily a safe long-lived slot.

The retained dumper must also not be over-interpreted as HP39g+ firmware-identity evidence. HP39g+ remains excluded from the gs/40gs physical-font layout until a trustworthy real-hardware full-flash fixture establishes its own mapping.

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
- tone/beep: future service if justified.

## File I/O and standard C I/O

The stable historical standard-C names remain standard C where semantics fit. HP49g+/HP50g now route ordinary `open/read/write/lseek/unlink` and picolibc `FILE *` operations through one process-wide hpclang-owned FatFs namespace. Stock KOS is retained only for insertion/write-protect sampling before takeover and `ResetSD` after release.

M7.2's clean core covers path metadata, FAT attributes and last-modified civil time, media state, free/total capacity, rename/mkdir/rmdir, UTF-8 LFN, and independent owned directory iterators. HP39/40 and HP48 continue to return clean `ENOSYS` rather than treating calculator-object stores as files.

The M7.2d source adapter now provides a deliberately bounded historical surface:

- `hpstdio.h` delegates standard I/O to libc and maps `fdelete()` to the clean stream namespace;
- `fsystem.h` covers useful file, seek/read/write, rename/delete, mkdir/rmdir, volume-space, metadata and directory-iteration patterns over the clean core;
- common visible `FS_FILE` metadata fields are retained for source ports, but the historical binary layout and direct FAT/controller state are not reproduced;
- the single real hpclang SD volume accepts legacy SD path aliases while multi-volume mutable state is not reintroduced.

Raw KOS filesystem calls, raw filesystem handles, global `chdir/getcwd` state, FSystem sleep/wakeup/controller ownership, and unverified timestamp/attribute mutation remain Migration/Internal rather than canonical compatibility APIs.

## HP39 calculator objects

HP39gs/40gs have real storage, but it is typed calculator-object storage: directory, Zaplet/Aplet, Note, Program, Variable, List, Matrix, Library and related records, with `HP39DIR.000` / `HP39DIR.CUR` metadata.

Historical HP39 ROM maps expose System-RPL-side operations such as `DoGetObFrSto`, `DoSendObToSto`, `OpenIO`, and `StdIOProc`. A future implementation should reach them through the typed Saturn/object bridge rather than publish ROM addresses or flatten objects into `FILE *`.

## Saturn / RPL object bridge

The common 1.1/2.0 `sat_*` surface is broad: raw memory/object inspection, stack operations, push/pop conversions, directory/variable access, and temporary object construction.

Compatibility decision: **Adapter**, deferred until the canonical ownership/lifetime model exists. The core should use explicit Saturn address/object types or opaque handles. HPGCC3's `SAT_OBJECT` direction is useful design evidence but not an ABI target.

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
| HPGCC2/3 `FS*` | future adapter above storage/object services | Migration/Adapter | later |
| Saturn/RPL `sat_*` | typed object/stack bridge | Adapter | M7.3/M8 |
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

`E2E / Font Gallery` **35298949457 / #112** is the current fully-green closure gate. It is green across Build, universal x50ng, all three Direct families, and all four Stock Gallery targets. HP39 summaries now show minifont, main, EditLine, and generated control; HP48/HP50 remain minifont, bigfont/default, and generated control.

The font implementation/evidence slice is closed. Remaining repository integration and bounded Stock-harness closeout are not HPGCC/font compatibility blockers.
