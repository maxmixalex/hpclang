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

## Runtime availability and source compatibility

A historical declaration or an available compatibility header does not establish
that an operation can run in the current calculator context. Applications should
use the [object-service discovery matrix](api-reference.md#object-service-discovery)
to distinguish classic RPL reads, classic RPL mutations and HP39 Aplet services.
The mutation queries additionally require the launcher's safe memory bounds;
HP39 queries require its native service portal. Direct launches do not gain
Stock object access merely by including `saturn.h` or `satdir.h`.

These queries describe the clean services beneath adapters. They do not claim
that every historical signature has an adapter, that native Port services exist,
or that a supported operation has sufficient memory/media at the instant of use.
The broader declaration/implementation/adapter/application-evidence ledger
is tracked below. Rows distinguish a working service from the remaining source
adapter and application evidence; they are not a blanket parity claim.

### Capability and application evidence

`D/S` means Direct and Stock execution. Native calculator objects require Stock;
SD streams exist only on HP49g+/HP50g. The source examples below are behavioral
ports to public hpclang APIs, not claims that unchanged HPGCC binaries run.

| Application capability | Historical declaration and implementation evidence | Clean service / context | Source adapter | Application evidence / remaining work |
| --- | --- | --- | --- | --- |
| Drawing, gray pages and animated presentation | HPGCC 1.1 `hpgraphics.h` and `sources/hpg/examples/testdoublebuf.c`; HPGCC3 `hpgraphics.h` mixed-font example | Owned surfaces, display sessions and frame synchronization; all six models D/S | Installed HPG names remain M8; the existing HPG prototype covers clip/style/pattern/XPM composition | `display/historical-clients.c` preserves five animation cycles and the mixed-font/shape layout. Direct and Stock pass on all six models; formal CI is accepted on `7303a3d6`. Busy loops become frame synchronization; captures replace manual pauses |
| Native or packaged text | Four releases' native-font helpers and HPG examples; HP39 ROM tables independently verified | Native font accessors plus `hpclang_font_available`, packaged fallback selected explicitly by the caller | Historical font-role/global selection stays in adapters | Gallery acceptance `8f79c156`, native symbols `41d75dec`; mixed-font application additionally detects missing native fonts in Direct classic launches. Availability-query acceptance is complete on `7303a3d6` |
| Physical keyboard and bounded console input | HPGCC 1.0.3 `examples/libtests/keyboard.c`; 1.1/2.0 `hpkeyb49.h` implementations | Physical snapshots/events, separate native text mode, editor and owned console; all six models D/S | Historical modal/blocking spellings remain M8 | Text/input acceptance `c7451364` and `41d75dec`; `system-services/console.c` additionally ports the eight-column matrix/ON display and Alpha exit, with real GPIO transitions. Direct and Stock six-model checks and formal CI pass on `7303a3d6` |
| Civil time, elapsed time, deadlines and sound | 1.1/2.0 `hpsys.h` implementations; HPGCC3 RTC/timer sources; benchmark and game examples | Stable civil snapshots, explicit conversion/set, monotonic time, polling timers and owned PWM; all six models D/S | Ordinary callback dispatch belongs to the application loop; selected delay, stopwatch, RTC, tone and BCD names now have adapters, with raw timer setup still open | M7.4 System Services covers forward/backward clock adjustment without deadline drift, cancellation and shared-register restoration. The HPGCC 1.1/2.0 sieve client passes six-model Direct/Stock locally; its additional formal CI is accepted on `2988e1de` |
| Files, directories, metadata and partition selection | HPGCC2 `fsystem/examples/example2.c`/`example3.c`; HPGCC3 `fsinit.c` implements four primary volumes | libc streams, owned directory snapshots, masked FAT metadata and explicit volume paths; HP49g+/HP50g D/S, other models `ENOSYS` | Bounded `fsystem.h` adapter, local cwd and historical MBR-slot aliases | `historical-files.c` ports the create/append/modify/move/read/enumerate/delete flow on all four FAT volumes. HP49g+/HP50g D/S and formal CI pass on `7303a3d6`. The original example's excessive literal write length is corrected |
| Media removal and card identity | HPGCC3 `FSVolumePresent` compares CID after reinitialization; GPIO/SD driver sources | Removal latch, remount and stale-object rejection accepted on `cada0f2e`; `hpclang_stream_media_identity_query` now exposes stable CID (accepted on `7303a3d6`) | `FSVolumeInserted` currently reports presence/volume errors, not the full historical `FS_CHANGED` distinction | Raw A→B→A image replacement covers old dirty streams and directory handles. CID comparison now distinguishes another card from a returning card in HP49g+/HP50g Direct/Stock raw-image tests; formal CI is accepted on `7303a3d6`. Physical SD test is a maintainer-approved nonblocking follow-up |
| Calculator arguments, composite values and stack results | 1.0.3 `examples/libtests/stack.c`, 1.1 `examples/stack/rplstk.c`, 2.0 `examples/stack.c`, HPGCC3 `sat3.h` list example | Owned typed objects, explicit numeric precision and transactional stack operations; classic Stock with capability checks | Bounded `saturn.h`/`satdir.h` adapters; raw pointers and remaining names are not public clean APIs | Accepted on `6a7f9a28`: `object-composite/historical.c` ports all four generations' representative flows; nested directories, numerical composites and reorder have additional focused tests |
| Persistent HP39 Aplet and native catalogs | HP39-enabled 1.1 Aplet template, checked against native ROM behavior | Aplet data/Reset and Program/Note catalogs; HP39 Stock with portal validation | Aplet packaging/template integration; no invented HOME namespace | Accepted on `6a7f9a28`: 15-invocation Reset and eight-invocation composite lifecycles, independent instances, SAVE and GC boundaries |
| Native Port 0/1/2 storage | Historical backup/library object semantics, distinct from FAT and HOME directories | M8.0 in progress: classic Port 0 owned reads and backup mutation; HP49g+/HP50g Port 1/2 owned reads | No historical adapter claims | Port 0 three-launch backup persistence passes locally on all classic models; Port 1 native backup readback passes locally on HP49g+/HP50g. Port 1 backup creation and native RPL recall also pass locally on both models. Port 2 owned enumeration and backup/library reads also pass both models across three launches. Port 2 backup CRUD uses native Flash records, with journaled replacement. Complete library mutation, Port 1 replacement/removal, Flash reclamation and automatic startup recovery remain required |
| Ordinary application termination | HPGCC3 `hplib/sys/exit.c` distinguishes normal saved-stack return from reset/warm recovery; picolibc supplies standard exit callbacks | `main` return / `exit` execute callbacks, close registered streams and return through the launcher; `_Exit` skips callbacks/buffer flushing but releases SDK-owned hardware | Standard libc names | 72 host tests, sanitizer checks and six-model Direct/Stock termination evidence pass. HP49g+/HP50g Direct/Stock additionally verify buffered exit-time writes after native return. Formal CI is accepted on `2988e1de` |
| Whole-calculator reset and hardware serial number | `kos_Reset` is reset/panic recovery; all four releases warn that `kos_getSerialNumber` may return garbage / Kinpo copyright data | Not implemented; no reliable serial-number claim | No adapter claims | Retained research goals, explicitly approved as nonblocking for M7 on 2026-10-12; ordinary `exit` remains required in M7 |

Historical `sys_slowOn/sys_slowOff` is also used by real applications to select
CPU speed and reduce power consumption. Monotonic delays and display frame
synchronization cover timing, but do not by themselves provide that power
behavior. The clean policy and its hardware ownership/restoration completed
review and formal acceptance on `2988e1de`, closing this M7 capability gate.

The current implementation provides typed HOST/ECONOMY profiles, preserving
PLL and bus configuration while adjusting LCD timing and rebasing monotonic
time. Active sound excludes clock transitions. All 72 host tests and all six
Direct/Stock System Services targets pass, including runtime-exit restoration.
Stock native-object coexistence passes on all six models, and SD file
coexistence passes on both supported models in Direct/Stock. Formal CI is accepted on `2988e1de`. Historical
raw speed-register and overclocking interfaces are not exposed by this policy.
The HPGCC 1.1/2.0 sieve client is now ported in
`hpgcc-system/historical-system.c`, with prime-count/checksum and visible
report checks. The earlier clean-service version passed Direct and Stock on
all six models and formal CI on `2988e1de`. The M8 version adds historical
timer/delay calls and runs as a separate Runtime-suite payload; its acceptance
is tracked in M8.4 rather than inherited from that earlier result.

## Public API stability

### Retained M8 deliverables

The capability ledger does not complete the following source-compatibility and
SDK delivery work:

- An installable SDK, public examples and a fresh LLVM-only downstream build.
- Maintained HPGCC adapters, including console/text spellings, further C++
  wrappers and migration guidance for raw KOS/Saturn/MMU callers.
- Representative HPGCC3 applications built through those adapters with minimal
  mechanical changes; M7's clean-service behavioral ports remain their evidence
  baseline, rather than being relabeled unchanged-source compatibility.
- The renderer-independent declarative UI and post-HPGCC extensions retained in
  the [M8 roadmap](roadmap.md#m8-hpgcc-source-compatibility-sdk-ux-and-post-hpgcc-extensions).

### Historical declaration stability

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

The hpclang core supplies raw state, 51-key physical identity, DOWN/UP/REPEAT,
and modifier state. Separate caller-owned text mode, model-native character
lookup and a bounded synchronous editor are accepted on `c7451364` and
`41d75dec`. Historical names and modal-screen policy remain M8 adapter work.

HPGCC3's asynchronous queue is useful comparative evidence but does not require hpclang's stock-hosted backend to install global keyboard IRQ ownership.

## Graphics: HPG and GGL

HPG is the principal graphics compatibility target. Its 59 public function signatures are identical across HPGCC 1.1, 2.0 and HPGCC3.

Compatibility decision:

- **Broad source-compatible Adapter** for HPG after clean primitives are stable;
- **Migration/adapter-later** for raw GGL and representation-specific screen helpers.

The hpclang core already provides opaque physical/software surfaces, clipped pixel/line/rectangle/circle/polygon operations, cross-level blit, native/generated fonts, and UTF-8/codepoint text drawing. That is enough clean machinery for many HPG wrappers without reproducing `hpg_t` as the core state model.

Historical mutable color/font/pattern/clip/PAINT-XOR state belongs in the HPG adapter.

`hpg_set_mode_mono/gray4/gray16` now acquire one scanout page when `dbuf` is
zero, or two pages otherwise. Single-page drawing publishes CPU writes at the
end of each adapter operation; double-page drawing stays hidden until
`hpg_flip()` exchanges pages. A failed mode replacement keeps the old pages,
pixels, and LCD state. Drawing state survives successful replacement, while
new pages start white. `hpg_cleanup()` restores the original host LCD only
when HPG acquired ownership and is registered for normal application exit.
An already-owned clean display session causes initial HPG acquisition to
fail with `EBUSY`; it is not released by HPG cleanup. `hpg_set_indicator()`
uses the six historical segment constants and follows PAINT/XOR and page
exchange. Its first call without an HPG-owned session acquires a single page
and preserves the visible Stock pixels and other status segments. Printed
icons differ by calculator; setting a segment does not change keyboard flags.
All 59 historical HPG function names are implemented. The integrated Display
suite passes all six models in Direct and Stock locally, including independent
pixel checks for single/double buffering, fonts, XPM and status segments.
Formal CI acceptance is complete at `1e46aa3d`:
[Core](https://github.com/maxmixalex/hpclang/actions/runs/38419627444),
[Quality](https://github.com/maxmixalex/hpclang/actions/runs/38419627345),
[Runtime](https://github.com/maxmixalex/hpclang/actions/runs/38419627334),
[Objects](https://github.com/maxmixalex/hpclang/actions/runs/38419627393) and
[Storage](https://github.com/maxmixalex/hpclang/actions/runs/38419627414) pass.

The original HPGCC 1.1 `testtext`, `testbigtext`, `testimage`, `testshapes`,
and `testdoublebuf` examples compile and link against this adapter locally.
Their HPG drawing calls remain unchanged. Required migration edits are:
replace `hpkeyb49.h` with a bridge polling the clean physical keyboard service
for `keyb_isON`/`keyb_waitKeyPressed`; replace the unused `hpsys.h` include with
`errno.h`; and remove application definitions of `int errno`. This is a
graphics-source build check, not yet a claim that the historical keyboard
subsystem is installed. The five original drawing bodies also pass HP50g
Direct execution: the test driver renames their entry points and replaces
blocking key waits with screenshot handshakes. Pixel anchors cover numbered
text, three image circles, the shape scene and the final animation line;
the 210-character big-font grid is captured and visually checked. This does
not test the original keyboard waits or assert pixel identity with HPGCC's
bundled fonts. The integrated Display E2E is a separately authored adapter
witness covering the wider model/launch matrix.

The adapter retains HPG's byte-indexed custom fonts, fixed character advance,
newline handling, and borrowed bitmap storage. This differs from the clean
Unicode font API. Default-font integration prefers live native
mini/big fonts on HP48gII/49g+/50g and the native main font on HP39-family models.
HP39 small text uses newRPL's `Font6_menus`, the closest reviewed small-face
candidate fitting the legacy 4-by-6 ASCII cell. When native big text is
unavailable, `Font8_StyleB` supplies a 6-by-8 fallback whose ASCII glyphs fit
without cropping. The adapter keeps legacy advance independently of a
packaged face's proportional advance. HP symbol bytes are mapped to Unicode
for packaged fonts; angle is constructed from two rays, x-bar adds an overbar,
and oversized symbols are compressed into the cell while retaining source ink.
Native tables are read from the running calculator, not embedded in the SDK.
The portable asset test renders all 256 bytes in both fonts for six models
without native mappings and compares the pixels with the packaged source.
Default-font pixel identity is not an adapter guarantee.

The installed HPG adapter loads compiled XPM arrays into owned mono/4/16-level
images. Palette keys may use multiple characters; `#RRGGBB` entries prefer
`g4` over `g` over `c`, with historical HPG luminance conversion. Unsupported
names, `None`, and unknown pixel keys produce opaque white, as in historical
HPG; these images do not carry an alpha plane. The caller supplies the
header-declared palette and rows as terminated strings. Invalid headers,
duplicate keys, and short rows fail with `EINVAL` and release partial storage.
This adapter is distinct from the earlier transparent-drawing feasibility
probe described below.

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

The verified 1 MiB HP39g+ D1.55 full-NOR fixture independently establishes main/mini/EditLine at physical nibble addresses `0x0DC096` / `0x0DE0DA` / `0x0DCE2F`, in pages `0x6e`/`0x6f`. The runtime uses those pages through the same private window. All three complete tables match the reviewed gs atlas, including the mini extended-character map. The model-specific ROM pixel oracle and new Stock USB Gallery entry cover this extension; formal acceptance is complete on `8f79c156`. These offsets come from ROM analysis, not historical HPGCC declarations.

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
The clean temporary setter now uses the corresponding KOS setter with readback,
retains the original setting and restores it explicitly or during display/runtime
cleanup. This supplies the ordinary application behavior of `kos_LCDSetContrast`
and HPGCC3 `lcd_setcontrast` without exposing GPIO/DAC wiring. Local unit and
six-model Direct checks pass; local Stock checks cover all six models. This establishes
setting ownership, not physical-panel luminance calibration.
USB voltage detection is read-only and deliberately does not claim battery
percentage or power-source selection. Capability discovery distinguishes model
support from a missing RPL launch context; resource/media errors remain the
responsibility of the individual service call.

Bounded UTF-8 editing and length-bounded message rendering provide the clean
primitives for historical line-input/error helpers. Buffer ownership, cancellation
and errors are explicit. Historical modal screen policy and
`readline` spelling remain adapter/UI work; `readint/readdouble` now have bounded
console adapters described below. Model-native Alpha/Shift
translation is implemented in the clean input layer. Explicit standard-stream
routing is available through `hpclang/stdio.h`.

### Bounded numeric input

All four distribution sources implement `readint()` and `readdouble()` using a
32-byte local buffer followed by `atoi(gets(buf))` or `atof(gets(buf))`:
`sources/hplib/stdio/readint.c` / `readdouble.c` in HPGCC 1.0.3, 1.1 and 2.0,
and `libs_workspace/hplib/stdio/` in HPGCC3. These implementations establish a
numeric-input application need; they do not establish bounded reads, full-line
validation, cancellation or distinguishable conversion failures.

| Historical capability | Clean equivalent / model and context | Source adapter | Validation |
| --- | --- | --- | --- |
| Read an integer from an editable line | Bounded `hpclang_text_input` plus `hpclang_text_input_parse_int64`; all six models, Direct and Stock; no native-object context required | Blocking `readint` now uses the bounded compatibility editor with checked `int` range; acceptance below | Clean service: host exact-boundary/error/re-edit tests and ARM System Services probes; CI accepted on `b92e2a7d` |
| Read a real number from an editable line | Same editor plus `hpclang_text_input_parse_double`; decimal/exponent syntax, explicit invalid/overflow/underflow errors | Blocking `readdouble` now uses the bounded compatibility editor; acceptance below | Clean service: host and ARM finite-subnormal, range and correction probes; CI accepted on `b92e2a7d` |

The clean functions preserve the line and output on errors rather than silently
accepting a numeric prefix. They do not implement Alpha-lock, a blocking console,
global stdin/stdout routing or arbitrary-precision calculator numbers. Those
separate capabilities retain their existing roadmap destinations.

The console extension now supplies bounded Unicode output and explicit C
standard-stream routing: `printf/fprintf/fflush` can feed an owned console, and
`getchar/fgets` can consume an application-supplied byte source. EOF, cancellation,
output failures and ownership changes are separate tested cases. This supplies
console primitives without importing historical global screen state or raw-key
numbering. Applications can use the accepted model-native Alpha/Shift state and
default keyboard editor; they supply the event source and rendering/presentation
policy. Historical blocking `readint` and `readdouble` now use the bounded
compatibility editor described below. The existing
file-backed stream implementation is not replaced.

The owned console additionally exposes grid/cursor snapshots, zero-based cursor
movement, row-tail erasure and configurable tab widths. These are prerequisites
for the historical `getx/gety`, `gotoxy/home`, `clreol` and tab controls now
provided by `hpconio.h`. Movement validates the destination before finishing pending
UTF-8; erasure does not wrap or scroll merely to erase the last column. Very
large tab widths use work bounded by the visible grid. Historical global console
selection, byte encoding and automatic presentation remain adapter responsibilities.

### Historical text-input policies

The clean `hpclang_text_input_run()` extension supplies a bounded synchronous
line session with an explicit event source, editing policy, acceptance validator
and rendering callback. It retains the line after cancellation and permits
correction after capacity/validation errors. It is accepted on `c7451364`;
`41d75dec` adds native symbols and the default keyboard editor. A ready-made
modal screen remains UI policy above these services.

The distributions do not establish one portable Alpha/Shift policy:

| Source | Observed behavior | Remaining evidence / clean destination |
| --- | --- | --- |
| HPGCC 1.0.3 `sources/hplib/stdio/getchar.c` and `_gets.c` | The former contains an unfinished reader; the latter defines a reader using `keyb_getkey(1)` and a CR-terminated echo/backspace loop | Determine the selected build implementation before treating either definition as the shipped behavior |
| HPGCC 1.1 and 2.0 `sources/hplib/stdio/getchar.c`, `gets.c` | Explicit HP49 character table, left-shift lowercase conversion, LF-terminated echo/backspace loop without a capacity argument | Model-specific text maps and bounded line input belong above the physical-keyboard core; the HP49 table does not prove HP39 support |
| HPGCC3 `libs_workspace/hplib/stdio/getchar.c`, `gets.c` | The old console retains the HP49 table; `gets` dispatches by console type, with a separate grayscale GUI implementation | Validate old and GUI console paths separately; their declaration alone does not establish six-model behavior |

Acceptance on `c7451364` and `41d75dec` covers explicit text-plane state,
model-specific character mapping, editing/accept/cancel, bounded input and
rendering through actual GPIO-driven keyboard flows. Scripted standard-input
coverage separately verifies stream routing. Historical blocking spellings
remain M8 adapter work; the clean input behavior has passed M7 acceptance.

`hpclang_text_alpha_lookup()` now supplies the printed Latin letters and
Alpha-space for explicit model/key/case inputs, accepted on `c7451364`. It does not copy a
historical raw scan table or infer a global modifier mode. Letter positions were
checked against the keyboard diagrams in the official
[HP39g+ guide, 1-3](https://h10032.www1.hp.com/ctg/Manual/c00045564.pdf),
[HP39gs guide, 1-3](https://h10032.www1.hp.com/ctg/Manual/c00751802.pdf),
[HP40gs guide, 1-3](https://h10032.www1.hp.com/ctg/Manual/c00748685.pdf),
[HP48gII manual, 1-5](https://literature.hpcalc.org/official/hp48gii-um-en.pdf),
[HP49g+ manual, 1-6](https://literature.hpcalc.org/official/hp49gplus-um-en.pdf), and
[HP50g manual, 1-5](https://literature.hpcalc.org/official/hp50g-um-en.pdf).
The N/O positions use the SDK's existing model-specific physical identities
(HP49 CAT/EQW versus HP50 EVAL/QUOTE). The manual diagrams establish letter
positions, not a change to that accepted physical-key ABI. All 26 letters in
both cases and space are tested per model through the existing electrical map.
HP39 letters are printed below and to the right of their keys: A is VARS,
E is SIN, Y is 2, Z is 3, and Alpha-space is Plus. Enter is not Alpha-space.

The new caller-owned `hpclang_text_mode` follows native one-shot/held/locked
Alpha policy above the physical event API, accepted on `c7451364`. HP39 Notes-style locking
is selected explicitly by the application; classic double-Alpha locks and
left-Shift case changes follow the native keyboard. Right-shift Alpha symbols
remain a distinct plane, not an alias for lowercase. Source-specific historical
console policies are not copied into the physical keyboard service. Complete
symbol lookup and the ready-made physical keyboard editor are accepted on
`41d75dec`; historical blocking compatibility spellings remain M8 adapter work.
System Services now drives physical GPIO input through the native mode,
editor and console for single/held and locked Alpha, with independent pixel
references and strict return checks (accepted on `c7451364`). Punctuation/symbol
planes and the complete ready-made interactive editor extend that acceptance
on `41d75dec`.

The current extension adds a default mode-aware editor and native Unicode
symbols. ROM probes across all six models distinguish HP39's π/∠ shortcuts
from classic Alpha-right-Shift Π and other Greek/math characters. Native font
rendering now maps those Unicode scalars back to the verified HP glyph bytes.
This avoids passing an HP character byte to a UTF-8 editor as if it were a
Unicode value. Shifted/held punctuation and the default keyboard editor are
accepted on `41d75dec`, with all six CI workflows green and integration into
main at `cada0f2e`.

## File I/O and standard C I/O

The stable historical standard-C names remain standard C where semantics fit. HP49g+/HP50g route ordinary `open/read/write/lseek/unlink` and picolibc `FILE *` operations through one process-wide hpclang-owned FatFs namespace. Insertion/write-protect sampling reads configured GPIO inputs before and during ownership; Stock KOS is retained for `ResetSD` after release. Observed removal or a block-transfer failure invalidates old file/directory objects, and new path operations can remount. Old handles must be closed after `EBADF`; their pending writes are not flushed to a newly mounted volume. The owned EINT3 removal latch now detects complete removal/reinsertion between observations. It temporarily borrows only the card-detect pin and its interrupt fields; it acknowledges card events and restores those fields before Stock ResetSD rescans. Physical hot-plug evidence remains a separate, nonblocking hardware check.

M7.2's clean core covers path metadata, masked FAT attribute mutation, modification/creation civil time and stored access-date queries, media state, free/total capacity, rename/mkdir/rmdir, UTF-8 LFN, and independent owned directory iterators. HP39/40 and HP48 continue to return clean `ENOSYS` rather than treating calculator-object stores as files. Access-date queries do not imply automatic atime maintenance: reads leave the stored date unchanged and FatFs clears it on writes. Creation/modification queries expose two-second precision; missing dates are explicitly invalid.

The M7.2d source adapter now provides a deliberately bounded historical surface:

- `hpstdio.h` delegates standard I/O to libc and maps `fdelete()` to the clean stream namespace;
- `fsystem.h` covers useful file, seek/read/write, rename/delete, mkdir/rmdir, volume-space, metadata and directory-iteration patterns over the clean core;
- common visible `FS_FILE` metadata fields are retained for source ports, but the historical binary layout and direct FAT/controller state are not reproduced;
- up to four primary FAT partitions are exposed through the clean volume API;
  legacy `C:` through `F:` and `:3:` through `:6:` select historical MBR slots,
  including holes. An unpartitioned FAT card uses slot zero. Current-volume
  selection and per-volume working directories belong only to the source
  adapter; they do not change Stock state or unqualified standard-C paths.

Clean `N:/path` indexes enumerate usable FAT partitions densely in MBR order.
For example, primary slots 1 and 3 become clean volumes 0 and 1, but retain
historical aliases `C:` and `E:`. Cross-volume rename fails with `EXDEV`
(`FS_BADVOLUME` through the adapter). Card replacement invalidates handles on
every volume. MBR overlap and out-of-card partition extents are rejected;
extended partitions, GPT and exFAT are outside this FAT primary-partition API.

Raw KOS filesystem calls, raw filesystem handles, global `chdir/getcwd` state, FSystem sleep/wakeup/controller ownership, and timestamp setters remain Migration/Internal rather than canonical compatibility APIs.

## HP39 calculator objects

HP39gs/40gs have typed internal calculator objects, but the native storage model is split across Stock catalogs/contexts rather than one HOME-like directory hierarchy. ROM evidence establishes the current structure: `GetApDirList` exposes the Aplet catalog; `CatPgm` enumerates `PGMCXT`; `CatNot` enumerates `NOTESCXT`; ordinary variable operations such as `StoVar` run inside the caller's active Aplet/topic context. Aplet selection/creation updates the Program/Note context pointers together with the active Aplet state.

`HP39DIR.000` / `HP39DIR.CUR`, `HP39AscA`, and `HP39BinX` are a different layer: the typed external Disk Drive/Conn3x transfer protocol used to import/export Aplets, Programs, Notes and related records. Historical `DoGetObFrSto`, `DoSendObToSto`, `OpenIO`, and `StdIOProc` belong to this transfer/I/O path and must not be treated as proof that `HP39DIR.*` is the internal calculator namespace.

Compatibility code may preserve that transfer protocol privately, while canonical internal object access goes through the Saturn/System-RPL portal and native catalog/context primitives. The bridge normalizes Aplet, Program and Note catalogs into owned list snapshots without exposing ROM addresses or leaving its temporary RPL stack cell behind. Public Program/Note services live in `hpclang/catalog.h`; Aplet lifecycle services live in `hpclang/aplet.h`. Their M7.3 acceptance is complete on `6a7f9a28`. HP39 is not flattened into `FILE *` or a fabricated HOME tree.

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
precision and stack-reorder equivalents are accepted on `6a7f9a28`.

HPGCC3's `SAT_OBJECT` direction remains useful design evidence but is not an ABI target; hpclang deliberately replaces borrowed raw addresses with owned snapshots.

## KOS and direct stock-OS calls

Raw KOS entry numbers and calling conventions remain **Migration/Internal**. Portable device/time/storage/display services should hide them. A low-level escape hatch may be considered only for real use cases that cannot be represented cleanly.

## win widget/event library

The HPGCC 1.1/2.0 widget family remains a retained **M8.4 adapter requirement**. The display/input/time/object foundations and bounded event/timer/key adapters support that work; widget ownership, focus, layout and original-application acceptance remain unfinished.

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
| `winevent.h`, `wintimer.h`, `winkeys.h` | Bounded foreground event/timer/key service over native input, time and HPG | Adapter implemented locally; see exact validation below | M8.4, CI pending |
| `winwidget.h`, `wincore.h` | Custom component lifecycle, focus, geometry and base painting | Local sanitizer/pixel and relocated SDK checks pass; native acceptance pending | M8.4 |
| `winprogress.h` | Four-orientation progress/slider, focus, repeat and blinking cursor | Local host/SDK/reference and six-model Direct checks pass; Stock pending | M8.4 |
| Concrete `win*` controls and `win.h` | Declarative UI with retained compatibility ownership/focus/layout | Required adapters remain open | M8.4 |
| raw MMU/interrupt APIs | internal only | Unsupported normal API | none |

## Historical window adapter feasibility

The 2026-10-24 review reads the retained HPGCC 1.1/39g distribution's
`sources/win` headers and implementations (`/work/hpgcc39-review` locally),
then compares all four original release archives. This is static source
evidence, separate from the subsequent implementation and runtime checks below.
The installed `winevent.h`, `wintimer.h` and `winkeys.h` now expose working
event/timer/key adapters. `winwidget.h`/`wincore.h` now add custom components;
concrete controls and the full `win.h` umbrella remain open.

| Release | Source/header files scanned | Window-family evidence |
| --- | ---: | --- |
| 1.0.3 beta | 309 | No `win_` references or installed `win*` headers in the scanned C/C++ sources/headers. |
| 1.1 with 39g support | 685 | 104 window implementation/header files and 13 installed window headers. Also includes `examples/win/simple.c`. |
| 2.0 SP2 | 712 | All 104 window source/header files and all 13 installed headers match 1.1 after newline normalization; neither comparison has missing or changed files. |
| HPGCC3 R004.revA | 552 | No `win_` references in the scanned sources/headers. `libs_workspace/headers/gui.h` instead exposes C++ `gApplication`, `gForm`, `gControl` and related classes. This is a distinct GUI API, not evidence for `win*` aliases. |

Reproduce the comparison with Python 3.11 or newer:

```sh
python tools/audit-hpgcc-windows.py /path/to/original-archives > window-audit.json
```

The tool reads caller-supplied archives without extracting their paths or
redistributing source. Its report contains archive SHA-256 identities, the
scanned file counts, reference filenames and exact normalized comparisons.
The reviewed archives are:

```text
hpgcc-1.0.3-beta-Linux.tar.bz2
7041e368bcc47b2d4014f49a8e4ef8039716057415a0091cf80912c0fa115500
hpgcc1.1-with-39g-support-windows.zip
572fd81addd205a7e2bdc1a5ff3f893abaa2ea7ba990766c7f09da6a36226020
hpgcc-2.0-SP2-Linux.tar.bz2
e4f9d632e65316101717f39c00523c2f43608a11156d4a4d4e8fae3195771029
hpgcc3_R004.revA.tar.gz
a984795493f2e4cf79001e0f95824b9c76f20df6d38a071eb8f9deb7a650165d
```

| Historical behavior and primary source | Existing service and required bridge |
| --- | --- |
| `common/win_post_event.c` snapshots the current handler chain and invokes it synchronously until a handler returns nonzero. `win_add_event_handler.c` orders higher priorities first and newer equal-priority handlers first. Changes to registration affect nested/future dispatch, not the copied outer chain. | UI actions are application messages, not this dispatcher. Preserve synchronous return values, nested dispatch and snapshot lifetimes in a separate compatibility event layer. Do not defer an event carrying borrowed stack data. |
| `common/win_init.c` installs keyboard, repaint and timer generators on the idle event in that priority order. `win_event_loop.c` repeatedly dispatches idle until quit. | A compatibility loop can drive existing keyboard, UI and clock services, but must preserve ordering, callback consumption and quit behavior. Rendering must occur outside the declarative view's pure construction phase. |
| `common/_win_timer_generator.c` invokes due timers from the idle handler. Periodic timers advance the previous deadline and may catch up repeatedly; delay timers schedule from the time after the callback. Nonzero callback results remove the timer. | Use monotonic time in foreground dispatch. The RTC coalescing adapter and asynchronous audio IRQ are not substitutes for these timer semantics. Define and test invalid intervals and callback-side cancellation before publishing adapters. |
| `common/win_remove_widget.c` deinstalls handlers and updates focus; `win_widget_destroy.c` invokes destruction and frees memory separately. | Retained compatibility objects must own state and stable UI keys, with distinct install/remove/destroy operations. Removing a declarative node alone does not satisfy old pointer ownership or an in-progress event snapshot. |
| `text/win_text_new.c` maintains editable byte text, cursor state and a blink timer; `win_text_set_text.c` copies replacement text. `checkbox` owns selection and updates menu bindings. | Labels, controlled `hpclang_ui_text_input` and `hpclang_ui_toggle` provide rendering/input foundations. A bridge must accept proposals into its own state before redeclaration, translate legacy byte text explicitly and preserve getter lifetimes. Direct aliases would change who owns the model. |
| `choose/win_choose_set_choice.c` uses a past-end selection for NULL or an unmatched choice; `win_choose_get_choice.c` then returns NULL. `winsoftmenu.h` provides up to 30 entries with enabled states and associated events. | Compose selection/menu views from existing controls while retaining no-selection, dynamic option identity, menu paging and model-specific function-key routing. A fixed six-button view is insufficient. |
| `progress/win_progress_set_value.c` clamps a double to its range. `win_progress_new.c` implements four orientations, slider key-repeat, focus changes at limits and change events. | The canvas callback can render a progress view, but the compatibility layer must retain numeric state, orientation, repeat timing and focus behavior. A painted rectangle alone is not a slider adapter. |
| `winbox.h` exposes positioned/spanning children and alignment; `winimg.h` combines an HPG pattern and image. | Compare legacy layout results with row/column/scroll composition, and preserve borrowed image lifetimes and clipping through the canvas/surface backend. Do not silently discard positioning or spans. |

Header/source discrepancies also need explicit treatment: this distribution's
`wincheckbox.h` names `win_text_get_text` and `win_text_is_selected` in its
checkbox declarations. A declaration inventory must be checked against actual
exported implementations and application use before generating aliases.

Source fingerprints for the dispatch/timing findings (SHA-256):

```text
common/win_post_event.c
19399f4eb254d1175eedc230a4f786dfe1e27a8b9013eba91ed95f000b9e06d9
common/win_add_event_handler.c
be05eb8b7ecc671078ad2a2cb0547433c319a55d9f74235098e1727d9c30331a
common/_win_timer_generator.c
04033bf94a0c72d7df31d10ee0c39da56cd7c313c01cbe933a5436d77488c0cf
```

The internal `win-timers` scheduler now preserves the original queue ordering,
periodic catch-up, post-callback delay, cancellation and nested processing.
It uses caller-owned slots and an injected monotonic clock; no hardware IRQ or
allocation is involved. A callback budget retains outstanding due work. Event
preflight failures retain the undelivered timer; clock/rescheduling failures
after delivery release that timer without replaying its callback. Both 1.1
and 2.0 original implementations match eight scenario traces under ASan/UBSan.
Additional focused tests cover capacity, budget, clock and overflow failures;
host/ARM analysis and six-model C/C++ launcher builds pass. The public interface
and device-clock integration are described below; widget integration remains open.

The internal `win-loop` now dispatches idle through this event bus. It retains
the original entry-time quit reset and shared quit flag for nested runs. A
handler consuming idle does not terminate the loop; requesting quit allows the
current snapshot to finish. Bounded runs leave pending timers and registrations
intact. Host integration checks the keyboard/repaint/timer priority sequence,
timed quit, consumption, nesting and failed-dispatch retry. Those generators
are controlled test callbacks. Subsequent device-input/rendering/public adapter
integration and its native evidence are described below.

The internal `win-keyboard` generator now uses the existing model-aware
historical matrix reader, with an explicit checked read result. As in the
original `_win_key_generator.c`, it updates the baseline before callbacks and
dispatches changed bit numbers in ascending order. Nested polling sees that
baseline; consumption applies to one key event. Codes remain physical matrix
positions, including HP39's distinct modifier labels, rather than text. Read
failure leaves the baseline unchanged. Dispatch failure is latched and requires
the owning loop to stop and unwind before reinitialization, since an earlier
key may already have been delivered. Host tests exercise all 60 physical inputs
on all six model mappings, mixed transitions, nesting and partial failure.
The internal `win-drivers` idle callbacks now connect this generator and the
timer pump to the loop. Their device-clock callback uses checked
`hpclang_time_read_us`, with timer intervals in microseconds. A dedicated loop
failure latch preserves the first errno across nested runs, quit and later
callbacks. This is separate from historical handler consumption; an ordinary
nonzero callback still does not quit or fail the loop. Driver failures consume
idle and propagate to the caller; the owner must unwind before reinitializing.
Host integration covers timed quit, keyboard/clock failures and insufficient
snapshot capacity during nested key dispatch. Subsequent native interaction,
repaint and public API validation are described below; widgets remain open.

The internal repaint coordinator preserves the original request lifecycle:
requests coalesce, pending clears before target preparation, repaint dispatch
is synchronous, and presentation follows even when a handler consumes the
event or requests ordinary quit. Requests raised during painting remain pending
for another frame; explicit nested repaint retains that sequencing. Checked
prepare/present hooks allow the future rendering owner to propagate failures
through the loop latch, skipping later stages without replaying drawing.
Host tests cover coalescing, re-request, nesting, consumption/quit and each
failure stage. Their hooks record calls rather than draw pixels; surface/UI
integration and calculator rendering acceptance remain unfinished.

Checked HPG frame hooks now provide a concrete repaint backend: acquire or
reuse HPG's display lease, clear the current surface, and publish/flip after
the callbacks. Existing grayscale and buffering choices remain intact, and a
foreign clean display lease returns `EBUSY`. HPG cleanup retains ownership of
release. Pixel tests use actual HPG/surface code across 2/4/16 levels and both
buffering modes, checking successive frames, stale-pixel clearing, failed-flip
front preservation and ownership conflicts. Their display session is a host
fixture; the separate native event/timer/repaint acceptance is described below. Legacy
single-buffer draw calls can publish before the final hook; no atomic-frame
guarantee is introduced. Declarative widget bridging remains separate work.

`winevent.h`, `wintimer.h` and `winkeys.h` expose event/timer/key services and the repaint
request. They preserve user IDs starting at 8, synchronous callback results and
positive millisecond timer intervals. The checked `hpclang_win_error`,
`hpclang_win_step` and `hpclang_win_reset` extensions make failures of historical
void functions observable. Default limits are 64 handlers (six reserved),
256 concurrent snapshot entries and 32 timers; exhaustion latches `ENOSPC`.
Reset is rejected during callbacks and does not release HPG's shared display.
The shipped `hpgcc-window` example builds and links all six launcher variants;
native execution remains pending. The installed/relocated SDK gate now passes
for this client in C and C++23, all six launchers and three HP39 Aplet variants;
453 complete-gate release artifacts match across two clean builds.
The maintained `tests/e2e/hpgcc-window` client now passes all six Direct and all
six Stock targets: four real 30 ms callbacks, synchronous consumed user events,
four repaint cycles, independent final-frame pixels and settled native return.
This native fixture uses HPG mono double buffering. Its Runtime CI manifest now
runs the same client and pixel verifier; formal CI acceptance remains pending.
Custom components now use `win_wcore_alloc` with installation, removal,
destruction and preferred-size callbacks. Geometry, borrowed fonts, colors,
transparency, focus notifications and base painting follow the historical
interfaces. Base painting retains the preceding pattern/XOR mode for the
background, then selects foreground/font, solid pattern and PAINT mode. Clip
right/bottom endpoints remain exclusive. Host tests exercise production HPG
pixels as well as lifecycle callback ordering and event-snapshot lifetime.

Remove components before destroying them, and wait until event/timer dispatch
returns before destruction. Destruction inside callbacks and reset with installed
components return EBUSY. Membership changes inside lifecycle callbacks also
return EBUSY. Cleanup/unregistration stays available after a latched error.
Invalid geometry is rejected without changing the previous bounds; failed
construction leaves application data ownership with the caller. This is local
custom-component implementation, not acceptance of concrete historical controls,
the complete `win.h` umbrella, or original multi-control applications. Native execution gates remain open. The optional
`tests/compat/check-win-widgets.py` compiles 21 original functions from each
caller-supplied HPGCC 1.1/2.0 archive. Both match 86,172 trace lines across 384
scenarios, including focus callback order, safe current-node removal, component
properties and painting calls. It excludes the original dangling-cursor UB;
separate sanitizer tests cover the repaired removal. An intentionally wrong
transparency conversion is rejected by the comparison. The expanded
relocated SDK gate passes custom components in C/C++23, all six launchers and
three HP39 Aplet variants, with 517 byte-identical release outputs across clean
builds (process 14783, exit 0).


An internal `win-key-state` engine now reproduces the original default key
handler's Alpha/Shift latches, held-state metadata and character tables. Its
injected synchronous post preserves one-shot clearing before typed callbacks,
modifier snapshots after those callbacks, and nested modifier notification order.
Failures of typed/modifier/repaint posts are latched after partial delivery;
a callback returning -1 remains a completed dispatch result. The public window
service now installs the original-priority (2000000000) press/release translators.
`winkeys.h` provides the historical event/state constants and typed payload fields;
callback payloads are borrowed synchronously, including independent nested payloads.
Earlier physical handlers can consume events before translation. Invalid key codes
and nested dispatch failures latch the window error; reset clears modifier state.
Character mapping retains the historical `hpkeyb49.h` numeric namespace on every
model, including HP39, rather than interpreting each model's printed key legends.
Use the canonical text-input service for model-native text interpretation.
Modifier notifications synchronously post repaint event 7, as in the original;
these posts do not run the idle frame begin/present hooks. A default repaint
handler at priority 2000000000 draws Alpha/Shift segments through HPG: white
when off, black when on/held, and gray 9 for locked Alpha. HPG's PAINT/XOR and
buffering rules apply, and earlier handlers may consume repaint. A checked
internal indicator hook propagates acquisition/segment/publication failures to
the window loop; the public void HPG function keeps its existing contract.
`tests/compat/check-win-key-state.py <archive-directory>` compiles caller-supplied
1.1/2.0 sources under ASan/UBSan and compares 94,253 trace lines, including held
Alpha, crossed shifts and nested callbacks. Both references match; a deliberately
wrong alpha mapping is rejected. Focused failure tests, host/ARM analysis and
six-model launcher compilation pass. Public integration additionally passes
ASan/UBSan checks for polled matrix transitions, one-shot Alpha, nested payload
lifetimes, preempted physical events, callback -1 results and error propagation.
The public adapter passes host analysis and six-model ARM compilation. Native
physical input and native modifier-indicator acceptance remain required; the preceding native
window evidence predates this integration. The installed window example now
uses typed digits to move the rectangle and ON to exit. Its relocated C/C++23
SDK builds, six-model launchers and three HP39 Aplet variants pass, with 453
byte-identical outputs from two clean builds (`/work/oct24-win-keys-sdk.log`).
This validates header delivery and application packaging, not physical input. The complete
SDK gate was rerun after checked indicator integration and passes the same 453
reproducible outputs, six-model launchers and three HP39 Aplet variants
(`/work/oct24-win-indicator-sdk.log`, process 71681 exit 0).

The maintained window fixture additionally passes all six Direct models with
real GPIO input: 22 press/release transitions and five expected typed callbacks
in each of single/double-buffer gray16 modes. An independent verifier checks all
pixels in 44 LCD/status captures per model and actual page exchange. One-shot
and locked Alpha, digits, left-shift lowercase and right-shift unmapped keys are
covered; a corrupted locked-Alpha status pixel is rejected. The original timer,
final-frame and settled-return checks still pass. Stock execution of this new
fixture found an HP39g+ launch-boundary failure: the first poll also observed
Stock's still-held Enter key. Saved RAM proves matrix bits 7/60 and two edges
for the first requested Alpha. The test now announces payload entry, waits for
launch-key release before installing observers, and retains the first failure
code. The fixed HP48gII Direct and HP50g Stock runs pass all input, status-frame,
page-exchange and return checks (59175 exit 0). Fixed HP39g+ Stock also passes
(69002 exit 0), including all 44 independently checked status frames. Fixed
All six corrected Stock models now pass the same checks (final process 68792
exit 0); each model retains its JSON result and all 44 frame/register pairs.
The original three-model HP39 failures remain preserved. Shipped-example native
execution, CI acceptance and full widget compatibility remain open. The integrated
136-test ASan/UBSan suite also passes (671.25 seconds), as do full formatting
and lint checks; whole-repository host/ARM analysis is still running. This is emulator GPIO evidence, not a physical-calculator test.

The actual installed window example now also passes all six Direct models
(81150 exit 0). A separate witness compiles its unchanged source with only main
renamed, captures the display before its real cleanup, and independently checks
the default animation's final rectangle and settled return. Sources and captures
remain under `/work/oct24-window-example-native` and
`/work/oct24-window-example-direct-<model>-evidence`. An initial witness reported
PASS after cleanup and correctly failed the return screen-transition gate; that
negative evidence remains preserved. This is default-animation execution, not
scripted digit/ON interaction or shipped-example Stock acceptance.

The useful implementation path is an owned compatibility state/event layer
above the accepted UI, beginning with dispatch and widget lifetimes, then
text/checkbox and the more stateful controls. These remain retained adapter
work, not approved omissions. The cross-version comparison establishes a common
1.1/2.0 implementation baseline; it does not establish a binary ABI or cover
HPGCC3's different GUI object model. Before acceptance, run original widget
applications with documented edits. The retained 1.1 `examples/win/simple.c`
is a concrete candidate: it combines a four-action soft menu, label, editable
text, menu-bound checkbox and all four slider orientations. Its bottom slider
starts at y=65, so 64-line models require an explicit layout/scrolling adaptation;
dropping that control would not preserve the example. Required
tests include priority/tie ordering, nested dispatch during removal, borrowed
payload lifetime, timer catch-up versus delay, removal during callbacks,
focus/menu transitions, exhausted storage and exit cleanup. Cross-model UI
pixels and Stock return must follow; passing current clean-UI tests alone is
insufficient evidence for historical semantics.

The initial internal `runtime/core/win-events` foundation now implements
synchronous snapshot dispatch with caller-provided registry/snapshot storage.
It preserves higher-priority/newer-tie ordering, nonzero callback results and
outer-snapshot stability while nested dispatch observes registry changes.
Registration or snapshot exhaustion reports `ENOSPC` before partial delivery;
reset during a callback reports `EBUSY`. Callback data is borrowed, so removal
does not permit freeing data still captured by an outer dispatch. Duplicate
removal follows the first matching callback/data in dispatch order, as the
historical implementation does even across different priorities.

The `win_events` host test passes ASan/UBSan, including callback-side mutations,
self-removal, nested exhaustion, short-circuit cleanup, extreme priorities and
borrowed stack arguments. Host and ARM analysis and C/C++ six-model launcher
builds pass. This initial module establishes dispatch semantics; the later
installed interfaces, event namespace, idle loop, timers and native validation
are described above. Complete original-widget-application compatibility remains
open.

The optional POSIX-host differential gate also compiles the original
`win_add_event_handler.c`, `win_remove_event_handler.c` and `win_post_event.c`
from each caller-supplied 1.1/2.0 archive:

```sh
python tests/compat/check-win-events.py /path/to/original-archives
```

It runs the same callback driver against the historical and new dispatchers,
under ASan/UBSan. The host glue preallocates event-table rows, replaces hardware
initialization with an empty function and supplies the host standard library.
The sole edit to the historical function sources replaces their
`alloca(int)` declaration with the host `<alloca.h>` declaration. It does not
alter dispatch, registration or removal logic. No historical source is bundled
with hpclang, and this optional gate needs the external archives and a host C
compiler with sanitizers.

Both original versions match all 4,836 trace lines from priority/duplicate,
nested mutation, self-removal and 32 deterministic 120-operation sequences.
The trace SHA-256 is
`2d552d8e4d5fce5a18f9e411c56ccb4582820c600543947073d21ec5db9f5664`.
A negative-control candidate that reverses equal-priority insertion is rejected
with a trace mismatch. This strengthens the event-order evidence; event-number
registration, hardware initialization, timers, rendering and complete original
widget applications remain outside this comparison.

The same optional gate now also compiles seven original timer functions from
each archive and supplies a controlled nanosecond clock. Eight sanitized host
scenarios establish the timer contract before implementing its bridge:

- With a 10 ms period, dispatch first reached at 35 ms and callbacks taking
  6 ms, periodic callbacks run at 35, 41 and 47 ms until the callback stops them.
  Delay-mode callbacks instead run at 35, 51 and 67 ms.
- Equal deadlines execute the most recently inserted timer first. Duplicate
  callback/data or event/data removal cancels the earliest queued match.
- The currently executing timer is absent from the queue. Removing its own
  callback/data does not prevent its rescheduling; returning nonzero does.
- A nonzero event-handler return also cancels an event timer. Nested timer
  processing can execute another due timer while the outer callback is active.

Both versions produce timer trace SHA-256
`4d5c9cdf912ba42e47307c7035fd009e1c50869512caa17b750b528d49504647`.
The clock and table setup are host glue; timer algorithms are compiled unchanged.
These scenarios use positive intervals and valid registered events. They do not
establish invalid-input, allocation-failure or hardware clock behavior, nor do
they claim that a new timer adapter exists. A bridge must retain distinct queued
and executing timer lifetimes rather than silently changing self-cancellation
or rejecting all nested timer processing.

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
| `hpgraphics.h`, 1.1 → 2.0 → 3 | 59 → 59 → 59 | 59 at each transition | All 59 installed adapter functions implemented; six-model Direct/Stock local checks and formal CI pass at `1e46aa3d` |
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
| Lists, real/complex arrays and matrices | 1.1/2.0 `saturn.h` construction/push helpers; HPGCC3 `sat3_createlist`, list operations, `sat3_createrarray/carray/matrix`; corresponding list/matrix sources exist | M7.3i, accepted on `6a7f9a28`, implements owned lists, homogeneous numeric arrays and heterogeneous symbolic matrices, with construction, access and independent edits. Raw historical signatures are not thereby declared compatible. See [composite object progress](object-service.md#m73i-composite-objects-and-stack-reorder-accepted). |
| Complex, identifiers/tags, binary and wider numeric values | HPGCC3 `sat3_createcplx_*`, `sat3_createid_str`, `sat3_createtag_str`, `sat3_createhxs_*`, decNumber/XREAL conversion families | M7.3i adds clean complex, identifier, tagged and width-preserving binary objects, arbitrary-length ZINT decimal exchange and exact 12-digit REAL coefficient/exponent exchange. HPGCC3's reviewed REAL decNumber/XREAL adapters still target classic REAL; distinct EREAL/LONGREAL formats and arithmetic are not implied. |
| Stack rearrangement and Boolean exchange | 1.1/2.0 `sat_stack_swap/nip`; 2.0 `sat_pushtruefalse`; HPGCC3 `sat3_swap`, `sat3_pushtruefalse` | M7.3i implements allocation-free transactional swap/nip and genuine SysRPL TRUE/FALSE exchange. The 1.1/2.0 reorder and 2.0 Boolean push signatures have bounded adapters; formal CI is complete on `6a7f9a28`. |
| HP39 named Program/Note/Aplet lifecycle | Native catalogs and ROM descriptor/context evidence; public generic HPGCC variable-definition CRUD has not been established | `hpclang_catalog_*` implements Program/Note CRUD, including compiler/editor records. `hpclang_aplet_list/clone/remove` provides owned name lists and named native copies, preserves caller contexts, protects active/default Aplets and maintains the cached display list on removal. The dedicated catalog fixture passes on all three HP39 models; CI acceptance is complete on `6a7f9a28`. |
| Port namespaces | HPGCC3 `libs_workspace/headers/sat3.h` describes Port 0 main RAM, Port 1 extended RAM and Port 2 Flash; only libraries/backups, without subdirectories | On 2026-10-04 the maintainer accepted each port's lack of read/write services as an M7.3j limitation. All three retain explicit implementation goals; HOME CRUD does not imply port support. |
| File attribute mutation and timestamp coverage | 2.0 `fsystem.h`: `FSChAttr`, `FSGetCreatTime`, `FSGetAccessDate` | Implemented over the clean metadata API; `FSCreate` accepts read-only/hidden/system/archive. Stored FAT access-date queries do not maintain atime. Persistence/error unit tests and six-model Direct plus HP49g+/HP50g Stock Storage pass locally; formal CI accepted on `6a7f9a28`. |
| Clock, sound, contrast and scheduled events | 1.1/2.0 `hpsys.h`; HPGCC3 `sys.h`, `time.h`: `sys_tm_RTC`, `lcd_setcontrast`, `tmr_eventcreate/kill` | M7.4 accepts civil clock read/set/conversion, cancellable tones, contrast observation and cooperative timers. Temporary contrast mutation/restoration is accepted on `41d75dec`. Low-level interrupt callbacks remain a migration boundary |
| Text input and console behavior | Historical `readint/readdouble/_gets`, key-plane helpers; HPGCC3 `keyb_setalphalock/shiftplane` and message API | Bounded UTF-8/numeric input, owned console/std-stream routing and model-native Alpha/text planes are accepted on `b92e2a7d`, `c7451364` and `41d75dec`. Physical DOWN/UP/REPEAT remains the separate lower layer; historical spellings remain M8 |
| Physical grayscale, flip, compositing and status | HPG/GGL/display headers and implementations | M7.5 implements owned gray2/4/16 sessions, page exchange, bounded frame waiting and six status segments; six-model Direct/Stock pixel/register gates pass locally. Public-API-only prototypes exercise clipping, PAINT/XOR, transparent anchored/relative patterns and RGB XPM. Formal CI accepted on `6a7f9a28`; broad HPG spelling/parser compatibility remains M8 |

HPGCC3 declarations, especially ROMLIB-dependent functions, are evidence of
application requirements, not proof they work on all six Stock ROMs. Likewise,
the HP39 template proves a saved-data channel, not arbitrary creation of system
variable definitions. Every proposed cross-model API needs its own execution
and restoration evidence.

### Owned LCD enable and historical delays

HPGCC3's `lcd_off/lcd_on` implementation stops and resumes the LCD controller's
ENVID bit. The clean equivalent is `hpclang_display_set_enabled` within an owned
display session: bounded frame waiting, retained pages, and host-state restoration
on exit. It does not change the application clock or put the CPU to sleep.

HPGCC 1.1/2.0 `sys_sleep(duration)` is a timer-based milliseconds delay in the reviewed
implementation. Its ordinary behavior maps to the clean monotonic delay service;
the historical name is not evidence of a missing low-power CPU suspend service.
The implementation multiplies duration by 1,000,000 because `sys_updateTimer`
accumulates **nanoseconds**, not microseconds: its 12 MHz base-clock conversion
uses 250/3 nanoseconds per tick. This confirms the original `millis` declaration.

The `hpsys.h` adapter now provides `sys_sleep`, `sys_waitTicks`, `sys_updateTimer`,
`sys_RTC_seconds`, `sys_delta_seconds` and `sys_tm_RTC`. `sys_sleep` preserves milliseconds
without the historical 32-bit intermediate multiplication overflow.
`sys_waitTicks` implements the documented 750 kHz duration, rounded up to
microseconds, without requiring slow mode or changing interrupt/PWM registers.
Both delays inherit the clean service's early return if the clock is unavailable.
`sys_RTC_seconds` uses a coherent civil snapshot and reports failure as `-1` with errno.
The one-day difference helper accepts seconds in 0..86399 and reports invalid
inputs as `-1/EINVAL`. `sys_timer_t`, its two initializer macros and
`sys_updateTimer` preserve elapsed/accumulated nanoseconds and restart through
`inited = 0`. Timer indices 0..4 are logical stopwatches sharing the clean
monotonic source; they do not select or reconfigure hardware PWM channels.
The private `last_val` field is widened for a monotonic sample, so the structure
is source-compatible for ordinary stopwatch use, not binary-compatible or a raw
PWM-counter view. Failed samples, regression and overflow leave state unchanged.
Timer setup/restore, clock changes and raw hardware helpers are not yet supplied.
Host tests cover unit conversion, maximum unsigned durations, midnight wrap and
RTC errors; the standalone HPGCC System E2E also exercises both delay spellings and RTC
access, with both lower and upper delay bounds to catch unit mismatches.
All six Direct and all six Stock targets pass locally with the corrected
milliseconds conversion and stopwatch checks, including independent report
pixel verification. The separate System Services payload also passes that
full matrix. Runtime and Quality CI are accepted on `c2fb66fe`.
The relocated installed SDK also builds its C/C++ examples, all six native
launchers and three HP39 Aplet families, including `examples/hpgcc-timer`. Historical
integer aliases are shared across `hpsys.h`, `hpkeyb49.h` and `saturn.h`.

The additional `sys_tm_RTC(struct tm *)` adapter uses the same civil RTC service,
computes weekday and year-day, and reports unknown DST through `tm_isdst = -1`.
Its historical void signature is retained: failure sets errno and leaves the
output unchanged. `hptime.h` forwards standard SDK libc time types and this
adapter; it does not promise the historical time library's binary ABI. Host
tests cover leap/century dates and failure preservation. The calendar check
passes all six Direct and all six Stock targets with independent report pixel
verification and normal Stock return. The relocated installed-SDK build also
passes with the calendar example; Runtime and Quality CI are accepted on `c2fb66fe`.

### Historical console output

Including `hpconio.h` selects a lazy default output backend for `printf`, `puts`,
`putchar` and other standard-stream writes. Header initialization registers the
backend without allocating a console or acquiring the LCD. An explicit
`hpclang_stdio_bind` takes precedence as a whole; unbinding resumes the default.
`hpstdio.h` alone retains its existing file/standard-C interface and does not
select a display backend. Define `HPCLANG_HPCONIO_NO_AUTOINIT` before including
`hpconio.h` to opt out of automatic registration, then call
`hpclang_hpgcc_console_enable()` when appropriate.

The output group adds `home`, `getx/gety`, `gotoxy`, `clreol`, `_clear_screen`,
`screen_width/height`, `get/set_tabsize` and `beep`. Coordinates are zero-based;
queries return `-1` on failure, and void calls report errors through errno.
Invalid coordinates and nonpositive tab widths are rejected. Tabs advance to
the next configured stop without reproducing the historical modulo bug; very
large widths take bounded work. Output uses calculator byte glyph IDs, including
0x80–0x9f, with the existing HPG 4×6 native/fallback font policy. It is not UTF-8.
Geometry fits complete cells within the visible display (32 columns on the
current 131-pixel targets), avoiding the old partial cell/status-column overlap.

Text and HPG share a display lease. Individual characters preserve surrounding
graphics, and scrolling moves graphics together with text. Output wraps
immediately at the final column, including scrolling after the last screen cell.
Single-buffer output publishes immediately; HPG double-buffer modes still need
`hpg_flip`. A separately owned clean display session yields `EBUSY`. Normal HPG
cleanup restores the borrowed display. Historical `gets/_gets` spellings use
the bounded adapters described below; decimal-format policy remains
M8 work. Host sanitizers, ARM analysis, all six Direct targets and HP39g+,
HP48gII, HP49g+ and HP50g Stock pass locally. The installed mixed C/C++ console
example also passes relocation, six-model launchers and three HP39 Aplet
packaging families. HP39gs/40gs Stock also pass; CI acceptance is pending.

The default read callback now supplies blocking `getchar` input without echo.
It consumes the existing physical event stream and applies the current model's
Alpha/Shift rules, retaining text mode across calls without resetting keyboard
timings or its queue. Enter returns LF and deletion returns BS. ON cancellation
reports `EOF` with `ferror`, errno `ECANCELED`, and no `feof`; `clearerr(stdin)`
allows subsequent input. Unrepresentable Unicode reports `EILSEQ` instead of a
silently substituted byte. This intentionally replaces the old hardcoded 49
keyboard table. An explicit standard-stream binding bypasses the default read
callback entirely. Six-model host mapping tests, font regressions, ARM analysis
and all six Direct and all six Stock physical-key checks pass locally,
as does the updated installed-SDK relocation/packaging gate.
`fgets` can
consume these bytes, but it does not implement an interactive line editor.

`hpclang_hpgcc_gets(buffer, capacity)` adds a bounded interactive byte line.
Capacity includes the terminating NUL. It starts empty, echoes accepted bytes,
accepts CR/LF, permits backspace, and rejects excess input while still allowing
correction. Cancellation/error returns NULL with terminated partial text;
nonempty ordinary EOF returns that text. Invalid or unknown capacity fails
before consuming input. The default console temporarily saves its visible
image so deletion can undo scrolling and restore earlier graphics. Each edit
is published even in an HPG double-buffer mode, synchronizing its back page.
Allocation failure can occur before editing. With explicit standard-stream
callbacks, it uses those streams and emits BS-space-BS for deletion without
acquiring a display. Host sanitizers, installed-SDK relocation/packaging and
HP39g+/HP50g Direct and Stock physical-key editing checks pass, including independent
pixel comparison after overflow rejection, backspace and scrolling. Wider
native acceptance is pending.

Including `hpconio.h` supplies `gets(array)` and `_gets(array)` call macros for
fixed `char` arrays, including struct members, in C and C++. Their declared
array extent becomes the editor capacity; the argument is evaluated once.
Pointer expressions (including interior pointers, function results and a choice
between buffers) have no declared array extent and fail with `EINVAL` before
consuming input or changing the buffer. Use `hpclang_hpgcc_gets(pointer, capacity)`
for those cases and variable-length arrays. This avoids relying on compiler
object-size inference, which can lose member bounds through pointer expressions.
These aliases do not export an unbounded function symbol or support taking its
address. Define `HPCLANG_HPCONIO_NO_GETS_ALIASES` before including the header to
disable both macros. The maintained console E2E now calls the historical name
for its guarded four-byte buffer, overflow rejection, backspace and scroll-undo
checks; all six Direct and all six Stock targets pass locally with independent
console/editor pixels and normal return. The installed-SDK C++ input client
exercises both spellings and passes relocation/packaging validation.

`hpstdio.h` exposes `readint()` and `readdouble()` without enabling default
stdio merely by including the header. Calling either uses the bounded editor
and respects explicit stream ownership. Their 32-byte local storage admits 31
input bytes; excess characters are rejected while backspace remains available.
Both accept a numeric prefix, matching the historical `atoi/atof` use:
`123tail` reads as 123. Integer conversion is decimal; double conversion uses
C `strtod` syntax. Unlike the reviewed historical floating parser, this accepts
leading whitespace and standard C hexadecimal/infinity/NaN forms, and applies
the exponent of a valid prefix even when trailing text follows it. Historical
floating-parser arithmetic and malformed-exponent quirks are not reproduced.
No conversion returns zero with `EINVAL`. Integer overflow
returns `INT_MIN/INT_MAX` with `ERANGE`; double range errors follow `strtod`.
Input error/cancel returns zero with its error; empty EOF returns zero with
stream EOF available to the caller. Successful conversion preserves errno.
For full-line validation and separate result/status, use the clean text-input
parsers. Host sanitizer acceptance covers prefix/boundary/range/error behavior;
ARM build/analysis, HP39g+ Direct and HP50g Stock physical-key numeric input,
and installed-SDK relocation/packaging pass locally. Wider native and CI
acceptance of these two adapters remains pending.

The integer family also supplies `readuint`, `readhex`, `readlonglong` and
`readulonglong`, with the shared `LONGLONG/ULONGLONG` typedefs. Source review of
HPGCC 2.0's `atoui_base/atoull_base/atoi_base/atoll_base` establishes digit-prefix
parsing from the first byte, without skipping whitespace or accepting `+`.
Signed decimal forms accept a leading `-`; unsigned forms do not. `readhex`
accepts mixed-case bare hexadecimal digits: `0xff` therefore converts the
initial zero only. These rules also correct the initial `readint` adapter's
overly permissive use of `strtol`. All integer forms saturate at their type's
limit with `ERANGE`, replacing historical overflow behavior. Host sanitizers
cover signed/unsigned limits, invalid prefixes and read errors. ARM analysis
and HP39g+/HP50g Direct plus HP50g Stock physical-key acceptance pass locally.
The relocated SDK builds and packages a C++ client using all six numeric entry
points and the shared historical integer typedefs. The combined Display payload
exhausted HP39g+ Stock memory in its 16-level display check before reaching
console input. Console/input tests now form a separate `hpgcc-console` runtime
suite; the split console passes HP48gII/HP49g+/HP50g Direct/Stock and HP39g+
Stock, including independent pixels and normal return. Split Display passes
HP39g+ Stock with its original allocation and gray-level assertions. Remaining
target and CI acceptance is pending.

`hpstring.h` and `hpconio.h` also expose the 1.0/1.1/2.0 `ints/uints` helpers.
Bases 2–36 use lowercase digits. Other bases retain the historical `"0"` result,
with a minus sign for negative signed input. Returned text is borrowed static
storage: `uints` and nonnegative `ints` share a buffer; negative `ints` has its
own buffer. Copy text before another call can overwrite it. Calls allocate
nothing and preserve errno. `INT_MIN` is handled without the old signed-negation
overflow. All bases and signed/unsigned boundaries pass host sanitizers and ARM
analysis; HP39g+/HP50g Direct, HP50g Stock and updated installed-example
relocation/packaging checks pass locally.
The reviewed HPGCC3 source does not provide these same helpers.

### Historical tones and packed BCD

`hpsys.h` supplies `sys_playTone2(frequency_hz, duration_ms)` and
`sys_playTone(frequency_hz, duration_cs)`, where `cs` means hundredths of a
second. Both use the owned sound service, block until completion and restore
borrowed PWM/pin state. Their historical void signatures report errors through
`errno`; no raw timer setup or slow CPU mode is required. A nonzero frequency
with zero duration is a no-op. Zero frequency is invalid. Centisecond values
that cannot fit in the service's 32-bit millisecond duration fail with `ERANGE`
before playing.

The reviewed 1.1 `sys_playTone2` implementation establishes Hz and milliseconds.
The older `sys_playTone` headers warn that parameter interpretation may be
wrong: its cycle count implies hundredths of a second, but its delay assumes
600 kHz while the historical tick service uses 750 kHz. The adapter implements
the nominal units, not that clock error, arithmetic overflow or raw pin writes.

`kos_beep` has a separate mode contract. HPGCC's wrapper waits
on `kos_CheckBeepEnd`, whose result is affected by the supplied mode flags.
HP50g ROM inspection indicates that bit 1 selects completion waiting, while
bit 0 permits a later request to replace the active tone. The admission check
uses the active request's flags, so treating `override` as an ignored Boolean
would change overlapping-call behavior. Controlled native-state probes on all
six models confirm the completion-query truth table and rejection of a new
request while the active flags prohibit replacement. The low-level driver
returns -1 in that case, but the public KOS entry discards it and returns zero.
These probes restore the native state and return normally; they do not start
tones or verify real asynchronous timing and permitted preemption. The adapter
must apply this completion/preemption policy to the owned asynchronous service
described below; blocking `hpclang_sound_play` alone does not implement it.

Current asynchronous sound implementation (local, not yet CI accepted):
`hpclang_sound_start`, `hpclang_sound_status` and `hpclang_sound_stop` now use
PWM2 output plus an interrupt-only watchdog deadline. Stock can already own
PWM3/PWM4, so the production service does not assume either channel is free.
Admission checks that the watchdog is idle and mapped correctly, disables
reset output, and borrows an isolated IRQ vector and stack. A real IRQ stops
the tone while application code is busy; status queries do not drive expiry.
The foreground collector restores the IRQ bank and vector after completion,
which cannot safely happen while the handler is executing on that stack.
Explicit cancellation and runtime exit also release the lease. Power-profile
changes and blocking playback reject an outstanding asynchronous lease.

The clock serializes monotonic state under interrupt masking, and the handler
uses a masked clock-read path while preserving interrupted `errno`. Shared
register tests cover deadline chunking, admission rollback, replacement,
cancellation and restoration. The emulator watchdog overlay now converts PCLK
cycles to microsecond scheduling and preserves fractional cycles; independent
arithmetic and register-model tests pass under sanitizers. Earlier investigation
and intermediate results are retained in the [roadmap](roadmap.md).

Public async replacement, autonomous stop, cancellation and restoration pass
all six Direct and all six Stock models locally. Normal exit and `_Exit` each
pass all six Direct targets;
both exit paths also pass HP50g Stock. The exit
fixtures inspect real cleanup before power, launcher or host restoration can
hide missing releases. A negative control that skips cleanup fails at that
boundary. Runtime CI now schedules both exit variants across all six Stock
models and all three Direct families, retaining the blocking-exit suite.
These gates are configured locally, not yet submitted or accepted. Native
emulator results do not establish audible output from physical hardware.

`kos.h` now exposes a locally implemented sound subset, `kos_beep` and
`kos_CheckBeepEnd`. `kos_beep` accepts Hz in 1..15000 and nonzero milliseconds;
the identical validation instructions in all six ROMs establish these bounds
and the historical invalid-argument return of -51. The adapter also sets
`errno=EINVAL`. Bits above the low two do not affect waiting or replacement.

| Mode low bits | Call waits | Later KOS request may replace this tone |
| --- | --- | --- |
| 0 | No | No |
| 1 | No | Yes |
| 2 | Yes | No |
| 3 | Yes | Yes |

If the old KOS request forbids replacement, the new request returns zero
without playing and waits according to the **old** mode, matching the ROM
wrapper. `kos_CheckBeepEnd` is false only while a KOS tone with its waiting bit
set is active; true does not necessarily mean silence. Completion queries
collect finished resources. Clock failures also terminate the Boolean query
with true and set `errno`; callers needing a distinct error result should use
`hpclang_sound_status` instead. `kos_beep` returns -1 with `errno` on runtime
resource/clock failure, making these additional service errors observable.

Cancellation uses `hpclang_sound_stop`. A canonical start replaces the client
identity as well as the tone, so old KOS mode bits cannot leak into later
playback. KOS will not implicitly replace a canonical tone: it returns -1 /
`EBUSY`. The same foreground-only interrupt ownership constraints documented
in `hpclang/sound.h` apply. This subset does not expose raw KOS syscalls.

Host sanitizer checks cover all sixteen low-nibble states crossed with four
incoming modes, overlapping waits, failure propagation, invalid inputs and
client identity. All six Direct models pass native KOS waiting-duration,
actual PWM replacement/rejection, busy-loop autonomous completion and restored
hardware checks. All six Stock models also pass. Both KOS-entry normal exit
and immediate exit pass all six Direct/Stock models locally, including hardware
restoration before launcher return. CI acceptance remains pending.

Packed-BCD adapters follow the 1.1/2.0 contracts:

- `sys_bcd2bin(value)` converts the low **two** nibbles. Its macro evaluates the
  argument once; a callable symbol is also available for function pointers.
- `sys_bcd2bin_nibbles(value, count)` uses the absolute count, capped at eight.
  Zero count returns zero; even `INT_MIN` is handled without signed overflow.
- `sys_bcd2str(value)` formats up to eight digits without leading zeros and
  returns a borrowed static buffer; zero produces `"0"`. Copy the text before
  another call. The function is not reentrant.

These helpers preserve historical nonvalidating nibble behavior: values 10..15
contribute their numeric value to base-ten accumulation, and string conversion
maps them to `:` through `?`. They are not hexadecimal formatting routines.
HPGCC 1.0's `sys_bcd2str` instead returns character values in ARM registers;
that ABI requires migration to the string interface. Its `sys_bcd2bin` also
omits the high-nibble shift; the adapter does not reproduce this numeric bug.
HPGCC3 declares the nibble conversion but comments out the string declaration.

Host tests cover conversion edges, borrowed text, tone units, overflow and error
propagation. The HPGCC System E2E exercises both tone spellings with measured
duration bounds, BCD results and independent report pixels. The separate sound
service tests cover hardware ownership and restoration.
Both tone spellings and BCD checks pass all six Direct and all six Stock targets
locally, including normal Stock return. The relocated installed SDK builds the
updated example for C/C++ launcher and Aplet acceptance. Runtime and Quality CI
are accepted on `c2fb66fe`.

### Historical slow-mode selection

The `sys_slowOn` and `sys_slowOff` source adapters are separate from RTC ticks.
They select the owned ECONOMY and HOST profiles respectively, matching the
1.0/1.1/2.0 application's slow/normal-speed intent. ECONOMY uses at most 12 MHz
and retains an already slower host clock. HOST restores the configuration
saved when entering ECONOMY; it does not force the historical assumed normal
speed or fixed LCD divider. Repeated calls are idempotent profile selection,
not nested scopes. The historical void signatures report service errors through
`errno`, including `EBUSY` during sound playback. Runtime exit restores the
borrowed profile even if the application omits `sys_slowOff`. This does not
provide raw `sys_clksave/restore`, PLL control or overclocking adapters.
All six Direct and all six Stock targets pass repeated profile selection,
frequency bounds, monotonic continuity and host-frequency restoration, with
independent report pixels and normal return. The relocated SDK also builds the
updated example, six-model launchers and three HP39 Aplet families. Runtime and
Quality CI are accepted on `c2fb66fe`.

### RTC tick compatibility requirements

The `sys_setRTCTickPeriod`, `sys_isRTCTick` and `sys_waitRTCTicks`
adapters form one stateful interface; `sys_waitRTCTicks` is not a fixed-unit
delay. The reviewed four-generation implementations establish these contracts:

- A period selector `n` in 1..127 means `(n + 1) / 128` seconds. The software
  default is 15 (8 Hz). Odd half-microsecond periods must not accumulate drift.
- In 1.1/2.0/3, setting a valid selector returns the previous selector; zero or
  values above 127 query the current selector without changing it. Version 1.0
  has a void setter instead. The later software selector does not establish
  what period an unrelated caller has written directly to hardware.
- `sys_isRTCTick` consumes one pending indication. Multiple expirations before
  a poll coalesce into one indication; there is no accumulated tick count.
- `sys_waitRTCTicks(count)` repeatedly consumes indications; nonpositive counts
  return immediately. A preexisting pending tick can satisfy the first count,
  so multiplying count by the period is not equivalent.

The historical implementation changes TICNT and clears the RTC bit in SRCPND.
The adapters now use the physical tick latch with an explicit polling lease.
A query does not access hardware. A valid setter returns zero on failure with
`errno`, leaving its software selector unchanged; polling returns -1 on error,
and a failed poll terminates the void wait function. Unknown models report
`ENOSYS`; conflicting RTC modes report `ENOTSUP` or `EBUSY`. The cooperative
timer's accumulated-expiration count is not substituted for this latch.

The internal physical-tick lease engine now saves the host divider/enable and
tick interrupt mask, retains a pending indication across period changes, and
consumes one indication per poll. Release restores the saved configuration;
an initial indication survives only if the application never consumed it.
Unrelated source/selected-pending bits and mask changes are preserved. FIQ
routing, RTC test/reset modes and an inconsistent selected interrupt are
rejected before writes. Host tests cover all 256 selector byte values,
coalescing, pending consumption/preservation, disabled host ticks, repeated
release and conflicts, including ASan/UBSan. The previous sub-period hardware
phase cannot be restored when reprogramming the divider. The three public
adapters are wired into the SDK, with optional cleanup on normal exit and
`_Exit`. Interface and both exit-path host tests pass under ASan/UBSan, and
ARM applications/launchers build. All six Direct targets and HP50g Stock pass
the pending-first-wait, coalescing and physical-period E2E checks; HP50g's final
Stock state restores TICNT to its host value after the app leaves selector 127
owned. A maintained Direct return gate now compares RTCCON, TICNT and the tick
source's routing/mask before frontend restoration or resumed Stock can conceal
a missing release. All six Direct targets pass this gate. An HP50g `_Exit`
variant also passes; a deliberate omission of cleanup is rejected with
`rtc-tick-state`. The relocated installed SDK builds the timer example using
all three adapters, C/C++ clients, six model-specific notebooks and three HP39
Aplet families. All six Stock models now pass the full interface/timing payload,
independent report-pixel checks and normal return. Host/ARM Clang analysis passes
across the batch's 265 sources and 777 distinct configurations. Runtime and
Quality CI are accepted on `413e5891`; the HPGCC system jobs pass in all three
Direct families and all six Stock models.

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
| Remove/replace a mounted card and recover stale handles | HP49g+/HP50g: observed absence or transfer failure invalidates the mount; new path operations remount and stale objects fail without flushing old data | Existing stream adapters inherit recovery; broad historical reinitialization adapters remain unimplemented | Host replacement/stale-handle/driver tests and six-model Direct plus HP49/50 Stock Storage pass; formal CI accepted on `56e4a9dd`. The EINT3 extension is accepted on `cada0f2e`: 68 host tests, HP49g+/HP50g Direct/Stock raw-image swap tests and all six CI workflows pass. A manual hardware-check package is provided, with physical execution nonblocking |

Pre-acquisition sampling and the owned removal latch are separate stages of
the same media lifecycle. Emulator tests establish stale-handle rejection and
remount behavior; the later physical card test remains a nonblocking follow-up.

### Progress and slider adapter (local)

`winprogress.h` implements four orientations, clamped values, configurable range,
increment and repeat delays, value-change events, focus transfer at boundaries,
and a blinking focus pattern. Setters retain HPGCC's separation from change
events and preferred-size packing. Invalid nonfinite bounds, NaN values,
nonpositive increments/delays and invalid orientations are rejected; reversed
or equal finite bounds preserve the old range. Event -1 disables notifications.
Removal cancels blink and repeat timers even from the value-change callback.
Failed preferred-size computation preserves the previous component dimensions.

Production event/timer/HPG sanitizer tests cover all four direction pixel maps,
key increments/decrements, delayed and periodic repeats, release cancellation,
focus transfer, published blink pixels, callback removal, invalid values/types
and preferred-size overflow. The first blink assertion read the back page after
presentation; the corrected check observes the front page. Failure evidence is
retained. Native execution remains open;
the installed SDK client exercises these symbols in C/C++. The complete gate
passes six-model launchers, three HP39 Aplet variants, model-specific Notebook
builds and 517 reproducible release outputs (64824 exit 0).

The optional `tests/compat/check-win-progress.py` compiles eight unchanged
original progress-control functions from each supplied HPGCC 1.1/2.0 archive.
Both match 7,011 trace lines across 24 orientation/range/focus configurations,
including preferred sizes, rendering calls, pattern bytes, key-driven events,
repeat timing, release cancellation and focus transfer. Both sides use the same
production window foundation; HPG device calls are recorded, not rendered by
this comparison. Separate production HPG tests check pixels. A deliberately
reversed horizontal increment mapping is rejected. Invalid-input extensions and
safe removal during notification are covered separately, not claimed identical
to the original implementation.

The maintained native window fixture also passes the progress controls on all
six Direct models. Each executes 16 GPIO edges and eight value-change callbacks
across both buffering modes, checks focus and resulting values, removes and
recreates controls, and captures four 60% orientations for independent complete
pixel and LCD-mode checks. Existing keyboard/page-exchange and settled-return
gates still pass. Repeat/blink timing remains covered by host tests, not this
native tap phase. Stock execution is running; CI acceptance remains pending.

### Text label and entry adapter (isolated following batch)

`wintext.h` supplies labels and editable byte strings with historical arrow-key
focus/cursor behavior, backspace, left-shift delete-at-cursor, right-shift clear,
and blinking cursor glyph 28. The entry keyboard limit is distinct from storage
capacity: programmatic replacement may exceed the limit without increasing it
or automatically repacking. Borrowed mutable strings may be shortened in place;
use the setter for growth. Aliased replacement and allocation failure preserve
valid ownership and cursor bounds. Null pointers, wrong control types, invalid limits and dimension
overflow use the window error latch.

Sanitizer tests cover editing, focus, allocation failures, aliasing and mutable
buffer shortening. Production HPG/custom-font pixel tests check glyphs, cursor
replacement and clearing after deletion. The optional source comparison compiles
all three original text source files (four public entry points) from supplied
HPGCC 1.1/2.0 archives over the same production window foundation. Both match
13,837 trace lines, including four entry limits, multiline labels, high bytes,
programmatic over-limit replacement and 512 deterministic editing actions.
Both sides use unsigned char to match the ARM byte convention. A deliberately
wrong shifted-backspace precedence is rejected. These checks exclude original
undefined allocation/alias/mutable-cursor cases, tested separately as repairs.
The installed SDK gate passes C/C++ text clients, six-model launchers, all three
HP39 Aplet variants, model-specific notebooks and 517 byte-identical release
outputs (14789 exit 0; `/work/oct24-text-sdk.log`). Native and CI acceptance
remain open; this work is isolated on `m8-widget-controls`, outside the frozen
progress/component batch.

The maintained native window fixture passes text controls on all six Direct
models (68819 exit 0). Per model it exercises 52 GPIO edges across both
buffering modes: insertion limits, cursor movement, backspace, shifted deletion,
clear and focus transfer. Independent complete-frame checks verify multiline
labels and adjacent entry glyphs with a known custom font. Existing keyboard,
progress, final-frame and settled-return checks still pass. Cursor timing
remains a host gate; Stock execution and CI acceptance remain open.

### Image adapter (isolated following batch)

`win_img_new` borrows an optional HPG pattern and image. Preferred size follows
the image, or is zero without one. Painting applies the widget background and
clip, fills the pattern, then blits the image; removal detaches repaint, and
destruction retains both borrowed resources. Allocation failures use the window
error latch and release partial state.

Production HPG ASan/UBSan tests pass pattern/image composition, source clipping,
background pixels, removal and borrowed-resource reuse, plus pattern-only and
empty construction. The initial expected pattern used screen-relative coordinates
and extended beyond the small host display; corrected expectations use the
relative pattern origin and fit the display. Failed evidence is retained in
`/work/oct24-image-pattern-origin-failure.log`; passing log is
`/work/oct24-image-tests.log`. ARMv4T compilation and analysis pass. SDK clients
include image construction; the complete gate passes (55970 exit 0): C/C++,
six launchers, three Aplet variants, notebooks and 517 byte-identical release
outputs (`/work/oct24-image-sdk.log`).
Original-source differential and native image-control acceptance remain open.

The image source comparison compiles unchanged `img/win_img_new.c` from both
HPGCC 1.1 and 2.0 over the same production widget/event foundation. All 32
resource/transparency/geometry combinations match 577 trace lines (SHA256
`4bf8c7cd9944ed93bc85a1c3b58f1a0b0182518dc87c964538f2ed0c14295bde`).
The missing-pattern negative is rejected; image reference and real HPG pixel
CTest targets pass under sanitizers. Logs: `/work/oct24-image-{reference,negative}.log`
and `/work/oct24-image-reference-tests.log`. Zero-sized drawing and allocation
failures are extension boundaries, not claimed identical call traces.

The native fixture now captures image-over-pattern pixels and borrowed-image
retention at checkpoints 700/800. Six-model Direct process 1287 uses separate
source `/work/oct24-image-native-src` and artifact `/work/oct24-image-native-arm`,
so the existing text-only Stock run keeps its original binary and evidence.
All six Direct models pass (1287 exit 0). Image Stock process 82374 runs in
that separate directory. Full 143-test control regression (63047) and host/ARM
analysis (54292) also run separately; Stock completion and CI remain open.

### Soft menu adapter (independent menu batch)

`winsoftmenu.h` implements 30 copied five-byte labels, six visible items,
forward/back paging, enabled states, active/disabled colors, insertion, deletion
with bounded packing and synchronous events carrying borrowed extra data and
shift states. Alpha and right-shift NXT fall through. Full add returns -1;
invalid indices and null labels use the window error latch. Construction reads
model geometry without acquiring a display; rendering anchors at the active
64- or 80-line screen bottom.

This intentionally repairs original defects: unsigned backward-page underflow,
the duplicate ordinary-color symbol in the disabled-color source, and deletion
packing that retained a destination occupancy bit when moving an empty slot.
The normal 80-line layout retains six 22-pixel cells and five-byte labels.
Host ASan/UBSan tests cover every entry, five pages, wraparound, modifiers,
disabling, copied/truncated labels, gaps, insert/delete shifts, full capacity,
removal and 64/80-line placement calls. ARMv4T compile/analysis pass. Logs:
`/work/oct24-menu-sanitizer-{build,tests}.log`. This is isolated on
`m8-menu-controls` using `/work/oct24-menu-src`; original-source comparison,
production pixels, installed SDK and native acceptance remain open.

Soft-menu follow-up verification passes production HPG pixels on both 64/80-line
screens and both buffering modes: six cells, copied labels, disabled colors and
page-two contents. Three sanitizer targets (state, original-source harness,
production pixels) pass. The optional original-source comparison covers all eight
entry points, counts 1..30, forward pages, safe backward traversal, modifiers,
enabling, colors and full-list insert/delete. Both HPGCC 1.1/2.0 match 6,840 lines
(SHA256 `ff26b00eaaf70e25d0df2f9b42ed9e1d4bd9d87bd061e2177529cb1d97ba3632`).
Only the duplicate disabled-color symbol and external alloca declaration are
adapted to link on the host; original executable behavior is retained. The
initial external-alloca link failure is preserved. Backward underflow and sparse
packing repairs remain separate tests. The wrong four-byte truncation negative
is rejected. Logs: `/work/oct24-menu-{reference,negative}.log` and
`/work/oct24-menu-reference-tests.log`.

Installed SDK acceptance passes (74007 exit 0): C/C++ menu clients, six model
launchers, three HP39 Aplet variants, notebooks and 517 byte-identical outputs,
log `/work/oct24-menu-sdk.log`. Native menu acceptance remains open.

The native menu phase uses 56 physical GPIO edges and ten selection callbacks
per model across both buffering modes, checking first/last visible options,
forward/back paging including the repaired wrap, disabled-item suppression and
payload shift states. Recreated menus use a known font for independent complete
page/disabled-color/glyph checks at checkpoints 900/901/1000/1001. The retained
text, image, progress, keyboard and settled-return gates all pass on six Direct
models (32162 exit 0). A wrong disabled-color pixel is rejected. Stock process
86659 runs in `/work/oct24-menu-src`, artifact `/work/oct24-menu-native-arm`;
Stock completion and CI acceptance remain open.

### Checkbox adapter (independent following batch)

`wincheckbox.h` supplies six actual exported checkbox functions, correcting the
historical header's unrelated text-function declarations. Labels are borrowed;
selection setters normalize booleans without implicit repaint/repack. Plain W
toggles, arrows transfer focus, and a borrowed soft menu exposes a CHK action
while focused. Unbind before destroying the menu. Focused rebinding immediately
moves that action; invalid indices/types/null labels use the window error latch.

Each installation allocates its toggle event afresh so removed descriptors can
survive a window reset without reusing stale global event IDs. Initial gained
notifications precede installation; menu publication waits until handlers exist.
An explicit bound flag distinguishes a lost binding from the next control's new
binding during removal. Singleton removal also clears its action. The renderer
retains the historical selected/unselected pattern and uses checked preferred
sizes and saturated off-clip offsets.

Focused ASan/UBSan tests pass borrowed labels, multiline sizes, normalized state,
modifier suppression, first-install menu activation, two-control focus/removal,
focused rebinding/unbinding, reset/reinstall with event-number reuse, singleton
cleanup and invalid inputs. ARMv4T compilation and analysis pass. Logs use
`/work/oct24-checkbox-{build,tests}.log`. Work is isolated on
`m8-checkbox-controls` with `/work/oct24-checkbox-src`; original-source
comparison, production pixels, SDK and native acceptance remain open.

Checkbox follow-up tests pass production HPG pixels for selected/unselected and
focused/unfocused states, label glyphs, clipping and both buffer modes. The
original-source comparison compiles all six unchanged 1.1/2.0 checkbox functions
over the shared widget/event/soft-menu foundation, adding only the historical
ULONGLONG type alias to host libc shims. Both match 7,218 trace lines (SHA256
`c13bbed5c0d2925615957b2ad755cd5a9f21b5193980f625fbc82f8feaa4843e`).
Six multiline/empty label cases, preferred sizes, boolean setters, keyboard
modifiers, focus and established menu bindings are covered. Lifecycle repairs
remain separately tested, not claimed identical to original bugs. The wrong
checkmark-pattern negative is rejected. Three sanitizer targets pass; logs are
`/work/oct24-checkbox-{reference,negative}.log` and
`/work/oct24-checkbox-reference-tests.log`.

Installed SDK gate passes (60235 exit 0), including all six checkbox symbols in
C/C++, six-model launchers, three HP39 Aplet variants, notebooks and 517
byte-identical outputs (`/work/oct24-checkbox-sdk.log`). Native checkbox
acceptance remains open.
