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
| Civil time, elapsed time, deadlines and sound | 1.1/2.0 `hpsys.h` implementations; HPGCC3 RTC/timer sources; benchmark and game examples | Stable civil snapshots, explicit conversion/set, monotonic time, polling timers and owned PWM; all six models D/S | Ordinary callback dispatch belongs to the application loop; historical names remain M8 | M7.4 System Services covers forward/backward clock adjustment without deadline drift, cancellation and shared-register restoration. The HPGCC 1.1/2.0 sieve client passes six-model Direct/Stock locally; its additional formal CI is accepted on `2988e1de` |
| Files, directories, metadata and partition selection | HPGCC2 `fsystem/examples/example2.c`/`example3.c`; HPGCC3 `fsinit.c` implements four primary volumes | libc streams, owned directory snapshots, masked FAT metadata and explicit volume paths; HP49g+/HP50g D/S, other models `ENOSYS` | Bounded `fsystem.h` adapter, local cwd and historical MBR-slot aliases | `historical-files.c` ports the create/append/modify/move/read/enumerate/delete flow on all four FAT volumes. HP49g+/HP50g D/S and formal CI pass on `7303a3d6`. The original example's excessive literal write length is corrected |
| Media removal and card identity | HPGCC3 `FSVolumePresent` compares CID after reinitialization; GPIO/SD driver sources | Removal latch, remount and stale-object rejection accepted on `cada0f2e`; `hpclang_stream_media_identity_query` now exposes stable CID (accepted on `7303a3d6`) | `FSVolumeInserted` currently reports presence/volume errors, not the full historical `FS_CHANGED` distinction | Raw A→B→A image replacement covers old dirty streams and directory handles. CID comparison now distinguishes another card from a returning card in HP49g+/HP50g Direct/Stock raw-image tests; formal CI is accepted on `7303a3d6`. Physical SD test is a maintainer-approved nonblocking follow-up |
| Calculator arguments, composite values and stack results | 1.0.3 `examples/libtests/stack.c`, 1.1 `examples/stack/rplstk.c`, 2.0 `examples/stack.c`, HPGCC3 `sat3.h` list example | Owned typed objects, explicit numeric precision and transactional stack operations; classic Stock with capability checks | Bounded `saturn.h`/`satdir.h` adapters; raw pointers and remaining names are not public clean APIs | Accepted on `6a7f9a28`: `object-composite/historical.c` ports all four generations' representative flows; nested directories, numerical composites and reorder have additional focused tests |
| Persistent HP39 Aplet and native catalogs | HP39-enabled 1.1 Aplet template, checked against native ROM behavior | Aplet data/Reset and Program/Note catalogs; HP39 Stock with portal validation | Aplet packaging/template integration; no invented HOME namespace | Accepted on `6a7f9a28`: 15-invocation Reset and eight-invocation composite lifecycles, independent instances, SAVE and GC boundaries |
| Native Port 0/1/2 storage | Historical backup/library object semantics, distinct from FAT and HOME directories | Not implemented | No adapter claims | Each port's limitation was approved on 2026-10-04; implementation goals remain in the roadmap |
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
`system-services/historical-system.c`, with prime-count/checksum and visible
report checks. It passes Direct and Stock on all six models; its additional formal CI is accepted on `2988e1de`.

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
All 59 historical HPG function names are implemented; target-side and
representative historical-application acceptance remain open.

The adapter retains HPG's byte-indexed custom fonts, fixed character advance,
newline handling, and borrowed bitmap storage. This differs from the clean
Unicode font API. Default-font integration is in progress: prefer live native
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
Target-side adapter acceptance remains open; default-font pixel identity is
not an adapter guarantee.

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
`readint`/`readline` spelling remain adapter/UI work. Model-native Alpha/Shift
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
| Read an integer from an editable line | Bounded `hpclang_text_input` plus `hpclang_text_input_parse_int64`; all six models, Direct and Stock; no native-object context required | Historical blocking `readint` and its narrower `int` range remain an adapter concern | Host exact-boundary/error/re-edit tests and ARM System Services probes; CI accepted on `b92e2a7d` |
| Read a real number from an editable line | Same editor plus `hpclang_text_input_parse_double`; decimal/exponent syntax, explicit invalid/overflow/underflow errors | `readdouble` spelling, modal rendering and console event loop remain adapter/UI work | Host and ARM finite-subnormal, range and correction probes; CI accepted on `b92e2a7d` |

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
policy. Historical blocking `readint` and
`readdouble` adapters remain to be built over these clean services. The existing
file-backed stream implementation is not replaced.

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

HPGCC 1.1/2.0 `sys_sleep(duration)` is a timer-based seconds delay in the reviewed
implementation. Its ordinary behavior maps to the clean monotonic delay service;
the historical name is not evidence of a missing low-power CPU suspend service.

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
