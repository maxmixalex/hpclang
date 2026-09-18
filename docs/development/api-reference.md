# API reference and HPGCC parity

This is the progress-oriented API reference for hpclang. It deliberately answers two different questions:

1. What can a native hpclang application call today?
2. How much of the historical HPGCC application surface has an hpclang equivalent yet?

The historical comparison is based on the complete HPGCC 1.0.3 beta, HPGCC 1.1 with 39g support, HPGCC 2.0 SP2, and HPGCC3 R004.revA distribution archives. Function counts below are de-duplicated public function names extracted from their installed/public headers. They are not a compatibility percentage.

## Status legend

| Mark | Meaning |
| --- | --- |
| ✅ | Native hpclang API exists and its calculator-dependent behavior has the required current validation. |
| 🧪 | Implemented, but the new behavior is awaiting a broader calculator gate or has narrower validation. |
| ⛔ | Blocker: implementation exists, but a required asset/contract is not yet frozen for release. |
| ◐ | A cleaner hpclang primitive covers part of the historical behavior; compatibility semantics are incomplete. |
| ⬜ | Planned or not implemented. |
| ↪ | Prefer a different hpclang or standard-library API; a historical wrapper would be migration/compatibility only. |
| ✕ | Intentionally not part of the canonical hpclang application API. |

Source-compatibility wrappers are not counted as implemented merely because the canonical core already has enough primitives to build them.

## What is public application API

`<hpclang/bootstrap.h>` is a launcher/platform contract. Normal applications should not use its MMU layout, private code-window constants, or launch-context structures. Likewise, `hpclang_boot_info().platform_cookie` is opaque platform state; applications must not infer the current HP39 private-window meaning from it.

The application-facing surface currently includes:

- `<hpclang/abi.h>`
- `<hpclang/target.h>`
- `<hpclang/device.h>`
- `<hpclang/time.h>` and `<hpclang/time.hpp>`
- `<hpclang/keyboard.h>`
- `<hpclang/display.h>` and `<hpclang/display.hpp>`
- `<hpclang/font.h>`
- `<hpclang/font-package.h>` for generated bitmap-font descriptors/build products

## Execution and device identity

| API | Status | Notes |
| --- | --- | --- |
| `hpclang_boot_info()` | ✅ | Process-lifetime boot record under the common HPC1 ABI. |
| `enum hpclang_model` | ✅ | Stable model IDs for HP39g+, HP39gs, HP40gs, HP48gII, HP49g+, HP50g. |
| `enum hpclang_family` | ✅ | `hp39`, `hp48`, `hp4950`; implementation grouping rather than an application ABI fork. |
| `hpclang_device_info()` | ✅ | Model, family and visible LCD geometry. |

## Time

| API | Status | Contract |
| --- | --- | --- |
| `hpclang_time_monotonic_us()` | ✅ | Monotonic microseconds since runtime entry. The stock backend observes existing timer/RTC state. |
| `hpclang_delay_us()` | ✅ | Explicit microsecond busy-wait; returns rather than deadlocking if the clock cannot be sampled. |
| `hpclang::monotonic_time()` | ✅ | C++23 chrono wrapper returning `std::chrono::microseconds`. |
| `hpclang::delay()` | ✅ | C++23 chrono duration wrapper. |

## Keyboard

| API | Status | Contract |
| --- | --- | --- |
| `hpclang_keyboard_read()` | ✅ | Instantaneous 8×7 matrix plus four dedicated inputs. |
| `hpclang_keyboard_any_is_down()` | ✅ | Query a raw snapshot. |
| `hpclang_keyboard_matrix_is_down()` | ✅ | Query a physical matrix coordinate. |
| `hpclang_keyboard_dedicated_is_down()` | ✅ | Raw ON + GPF4/5/6 dedicated inputs; semantic labels differ on HP39 versus HP48/49/50. |
| `hpclang_keyboard_events_reset()` | ✅ | Establish current stable baseline without synthetic DOWN events. |
| `hpclang_keyboard_events_set_timing()` | ✅ | Configure debounce and repeat timing. |
| `hpclang_keyboard_event_poll()` | ✅ | Polled DOWN, UP and REPEAT event with timestamp, held modifiers, raw scan code, and physical key identity. |
| `enum hpclang_key` / `event.key` | ✅ | Complete physical-key identities for HP39gs/40gs, HP48gII, HP49g+, and HP50g. All 51 positions pass Direct E2E on all five automated models. HP39g+ remains UNKNOWN pending its real fixture. |
| `HPCLANG_KEYBOARD_MOD_SHIFT` | ✅ | Portable physical “a Shift key is held” bit. HP48/49/50 retain left/right detail; HP39 has one Shift. |
| text / Alpha / Shift-plane translation | ⬜ | Higher layer, not part of the raw/debounce core. |

## Display and surfaces

The canonical coordinate system has a top-left origin. Rectangle APIs use `x, y, width, height`; this intentionally differs from historical HPG inclusive `x1, y1, x2, y2` rectangle calls.

| API | Status | Contract |
| --- | --- | --- |
| `hpclang_display_surface()` | ✅ | Borrow the physical display as an opaque surface. |
| `hpclang_surface_width()` / `hpclang_surface_height()` | ✅ | Visible geometry. |
| `hpclang_surface_levels()` | ✅ | Current physical surfaces report two levels. |
| `hpclang_surface_clear()` | ✅ | Clear with explicit 0–255 intensity. |
| `hpclang_surface_get_pixel()` / `hpclang_surface_set_pixel()` | ✅ | Clipped pixel read/write. |
| `hpclang_surface_draw_line()` | ✅ | Geometrically clipped line. |
| `hpclang_surface_draw_rect()` / `hpclang_surface_fill_rect()` | ✅ | Extent-based clipped rectangles. |
| `hpclang_surface_draw_circle()` / `hpclang_surface_fill_circle()` | ✅ | Integer-radius clipped circles. |
| `hpclang_surface_draw_polygon()` / `hpclang_surface_fill_polygon()` | ✅ | Explicit point arrays; closed outline and even-odd fill. |
| `hpclang_surface_blit()` | ✅ | Representation-independent clipped copy with same-surface overlap support. |
| `hpclang_surface_create()` / `hpclang_surface_create_mono()` / `hpclang_surface_destroy()` | ✅ | Opaque heap-owned 2/4/16-level software surfaces. Packing remains private. |
| `hpclang::surface_ref` | ✅ | Zero-owning C++ wrapper. |

## Fonts and text

hpclang deliberately separates **calculator-native fonts** from **generated custom fonts**.

### Calculator-native fonts

The public core does **not** define a semantic font-role taxonomy. These accessors expose concrete calculator-native/ROM fonts by their historical names, plus one convenience default selection. Function names such as `mini`, `big`, and `editline` therefore identify a stock font source; they do not mean “small text”, “large text”, or “editor role” for application UI policy.

| API | Status | Contract |
| --- | --- | --- |
| `hpclang_font_default()` | ✅ | Borrow the calculator's normal default native selection. On HP39gs/40gs this is the stock 5×8 main raster; on HP48gII/49g+/50g it is the stock bigfont. This convenience alias is not a semantic role. |
| `hpclang_font_mini()` | ✅ | Borrow the stock minifont. HP39/40 uses its proportional five-row small font; HP48/49/50 uses the historical six-row minifont. |
| `hpclang_font_big()` | ✅/🧪 | Expose the historical HP48/49/50 bigfont only: eight raster rows with nominal advance 6. HP39-family calculators do not have this HPGCC bigfont and must return `NULL` rather than aliasing the 5×8 main font. The contract is frozen here before the implementation change. |
| `hpclang_font_editline()` | 🧪 | Expose the dedicated HP39gs/40gs fixed 6×10 EditLine ROM font. It is unavailable on HP48gII/49g+/50g, and remains unavailable on HP39g+ until a trustworthy full-flash fixture establishes its layout. The contract is frozen here before the implementation change. |
| `hpclang_font_height()` | ✅ | Line height without exposing bitmap storage; `NULL` reports 0. |
| `hpclang_font_advance()` | ✅ | Nominal/fixed advance; `NULL` reports 0. |
| `hpclang_font_codepoint_advance()` | ✅ | Per-codepoint advance where the native/generated font is proportional. |

No HP glyph bitmap is linked into the application. HP48gII/49g+/50g minifont/bigfont objects are resolved through the live Saturn logical mapping supplied by the launcher. HPGCC3 documents bigfont as an 8-row raster with normal advance 6; its raw record is eight bytes per character, so “8×8” describes storage/raster capacity rather than a fixed 8-pixel text advance.

HP39gs/40gs fixed native rasters are physical stock-ROM assets. The launcher maps only physical NOR pages `0x0007e000` and `0x0007f000` as uncached 4 KiB pages in the bootstrap-private prefix of hpclang's own code coarse table. Runtime accepts that mapping only when the opaque `platform_cookie` matches the expected private-window base. The established 5×8 main and proportional mini tables already live in this window.

Independent HP39 UI reverse engineering also establishes a third, dedicated EditLine table: fixed 6×10 cells, 20 Saturn nibbles per character, with the legacy lookup `0x07D3EA + 20 * characterCode`. The 2.22 HP39gs/40gs candidate locations obtained from the independently verified main/mini relocation deltas remain implementation evidence until the new ROM hash/pixel oracle confirms them. No glyph payload from that table is redistributed by hpclang.

The HP39 two-page window replaced an experimental long-lived high-L1 NOR section alias. Runtime diagnostics proved the alias descriptor words could themselves lie inside the active LCD framebuffer's physical interval and be overwritten by ordinary drawing. The fix is therefore an MMU-ownership correction below the font API, not an application ABI change.

HP39 mini is five bitmap rows with a per-glyph advance (nominal 4); ASCII lowercase follows the stock small-font case-folding behavior. HP39 main is the stock 5×8 raster with nominal advance 6. HP39 EditLine is a separate fixed 6×10 raster with advance 6. HP48/49/50 mini/big retain the historical 6/4 and 8/6 line-height/advance conventions.

HP39g+ is intentionally not inferred from its siblings. It retains the non-physical fallback path for already established fonts, and the EditLine accessor remains unavailable until a trustworthy real-hardware full-flash fixture establishes its own native layout.

### Generated custom fonts

`hpclang_target_font()` is a CMake build API over the checked-in generated catalog. It emits a `struct hpclang_font` descriptor backed by only the glyphs selected for that target.

The three repertoire modes are implemented and Core-CI-gated:

| Mode | Contract |
| --- | --- |
| `STATIC_AUTO` | Collect code points only from explicit `HPCLANG_UI_TEXT("...")` markers in the target's C/C++ sources. Ordinary literals/comments/metadata do not grow the font. |
| `STATIC_EXPLICIT` | Retain caller-supplied `CODEPOINTS` plus the configured fallback. No source-text discovery. |
| `FULL` | Retain the selected generated face's complete mapped repertoire. |

Generated convenience aliases are currently `@mini = newrpl_6a`, `@normal = newrpl_8c`, and `@large = newrpl_10a`; arbitrary generated face IDs and arbitrary metrics remain valid. These aliases do **not** redefine the native `hpclang_font_mini()/big()` functions.

`HPCLANG_UI_TEXT(text)` is an identity macro at runtime. It exists solely as explicit build metadata for `STATIC_AUTO`.

### Text drawing

| API | Status | Contract |
| --- | --- | --- |
| `hpclang_surface_draw_codepoint()` | ✅ | Draw one Unicode code point with native fallback or generated mapping. |
| `hpclang_surface_draw_char()` | ✅ | Compatibility byte entry point; byte value maps directly to U+0000..U+00FF. |
| `hpclang_surface_draw_utf8()` | ✅ | Decode UTF-8; invalid sequences become U+FFFD then the selected font's fallback if needed. |
| `hpclang_surface_draw_text()` | ✅ | Canonical UTF-8 text API; same semantics as `draw_utf8()`. |

There is deliberately no public core semantic-role API such as `text`, `caption`, or `edit-large`. The historical names `mini`, `big`, and `editline` are concrete ROM-font accessors, not theme roles. Applications select native or generated fonts directly; a future UI toolkit/theme layer may define its own role vocabulary above this API.

## Storage model

hpclang distinguishes byte streams from typed calculator objects.

| Storage class | Platform status | Canonical direction |
| --- | --- | --- |
| byte-stream files | ✅ HP49g+/HP50g | private KOS-backed `open/close/read/write/lseek/unlink` feeds picolibc `fopen/fread/fwrite/fseek/remove`; HP39/40 and HP48 return clean `ENOSYS` for unsupported stream operations |
| calculator object store | HP39gs/40gs format/protocol known; API not implemented | typed/named object API; do not flatten into `FILE *` |
| RPL/Saturn objects | HP48/49/50 historical platform capability; API not implemented | M7.3 typed object/stack bridge |

The HP39 object-store root is `HP39DIR.000`; `HP39DIR.CUR` tracks the current directory. These names describe the stock object-store protocol, not a general POSIX filesystem.

## Validation by service

| Service | Host | ARMv4T link | Direct runtime | Stock evidence |
| --- | --- | --- | --- | --- |
| HPC1/runtime | ✅ | ✅ | 5/5 PASS | Runtime #122 is fully green across all Stock families, including settled HP48 return |
| device info | — | ✅ | 5/5 PASS | covered by common payload |
| monotonic time/delay | ✅ math | ✅ | 5/5 PASS | covered by common payload |
| keyboard events + identity | ✅ | ✅ | 5/5 full 51-key PASS | Direct and Stock both require the exact 52-frame progression; Runtime #122 is green |
| display/surfaces | ✅ | ✅ | 5/5 PASS | all stock paths have historical/current PASS evidence |
| native font/text | ✅ | ✅ | production Gallery #99 covers all Direct families; HP39 native pixels are valid through the physical-ROM path | HP50g and HP48gII compact live-native controls green with settled return |
| generated font selection | ✅ configure | ✅ | Gallery #99 exhaustively traverses all **245 generated faces** on all five automated targets | HP50g and HP48gII generated controls green |
| C++23 + libc++ | ✅ | ✅ | 5/5 real-service PASS | same canonical HPC1 payload builds |
| hp4950 stream stdio | — | ✅ | non-stream families reject unsupported stream access cleanly | ✅ HP49g+/HP50g read/create/truncate/write/flush/seek/reopen/remove/ENOENT |

Formal `E2E / Font Gallery` run **35255868206 / #99** is fully green across Build, universal x50ng, all three Direct Gallery families, and Stock hp39gs/hp40gs/hp48gii/hp50g. It traverses **247 entries (245 generated + 2 native) × 2 pages = 494 captures per target**. HP39g+ remains outside emulator E2E until a trustworthy real-machine full-flash fixture exists.

## Historical public-surface size

| Historical surface | 1.0.3 | 1.1 39G | 2.0 SP2 | HPGCC3 | hpclang today |
| --- | ---: | ---: | ---: | ---: | --- |
| HPG `hpgraphics.h` | 55 | 59 | 59 | 59 | clean core geometry/surface/text exists; no HPG adapter yet |
| keyboard `hpkeyb49.h` | 12 | 17 | 17 | — | raw + event core implemented |
| HPGCC3 `keyb.h` | — | — | — | 8 functions plus queue/macros | event core implemented; queue policy not copied |
| legacy `hpscreen.h` | 12 | 9 | 9 | — | migrate to opaque surface |
| GGL `ggl.h` | — | 28 | 28 | 28 | not implemented |
| system `hpsys.h` | 25 | 40 | 41 | — | monotonic time/device subset |
| HPGCC3 `sys.h` | — | — | — | 61 | small generic subset only |
| `hptime.h` | — | 3 | 3 | — | monotonic/chrono API instead |
| `hpstdio.h` | 26 | 37 | 40 | — | libc formatting plus hp4950 standard `FILE *` storage complete |
| HPGCC3 `stdio.h` | — | — | — | 40 | hp4950 standard `FILE *` storage complete |
| structured `fsystem.h` | — | — | 41 | 43 | no `FS*` adapter yet |
| KOS `kos.h` | 15 | 32 | 33 | 33 | raw KOS kept private |
| Saturn `saturn.h` | — | 50 | 58 | replaced | not implemented |
| Saturn directory `satdir.h` | — | 29 | 29 | replaced | not implemented |
| conio | 27 | 27 | 26 | 49 in new `conio.h` | not implemented as a subsystem |
| raw `syscall.h` | 9 | 9 | 9 | 9 | intentionally not canonical |

HPGCC 1.1/2.0 also contain a substantial win widget/event family with 74 stable implementation names between those releases. HPGCC3 moves to a different GUI family. hpclang has not started a widget subsystem.

## HPG graphics: detailed progress

HPG is unusually stable: all 55 HPG 1.0.3 declarations survive exactly into 1.1, which adds four functions, and all 59 1.1 declarations remain signature-stable in 2.0 and HPGCC3.

| HPG group | Historical calls | hpclang status |
| --- | --- | --- |
| lifecycle | `hpg_init`, `hpg_cleanup` | ↪ Core surface access needs no matching global lifecycle; adapter can supply legacy setup semantics. |
| display mode | mono/gray4/gray16, `hpg_flip` | ⬜ Physical grayscale/page flip deferred pending contiguous LCD-DMA resource ownership. |
| geometry | width/height, pixel, line, rect, circle, polygon | ✅ Canonical primitives implemented; legacy context/coordinate wrappers absent. |
| indicators | `hpg_set_indicator` | ↪ Model-specific compatibility/capability concern, not part of the visible surface ABI. |
| clipping state | `hpg_clip_*` | ◐ Surface-bound clipping exists; mutable user clip state remains adapter work. |
| fonts | `hpg_get_minifont`, `hpg_get_bigfont`, metrics, allocation/free | ◐ Native and generated hpclang font systems are implemented; HPG's exact bundled-font/custom-descriptor source semantics still need an adapter. |
| text | `hpg_draw_letter[_on]`, `hpg_draw_text[_on]` | ✅ Canonical explicit-surface UTF-8/codepoint drawing exists; mutable HPG context wrapper absent. |
| mutable style / PAINT/XOR / patterns | HPG context state | ↪ Adapter state, not canonical surface state. |
| owned images | mono/gray images | ◐ Generic 2/4/16-level software surfaces cover the clean ownership model. |
| blit | `hpg_blit` | ◐ Canonical blit implemented; HPG argument/context adapter pending. |
| XPM | mono/gray loaders | ↪ Convenience importer/adapter work. |

Zero HPG source-compatibility wrappers are claimed merely because the clean primitives exist.

## Keyboard: detailed progress

HPGCC 1.0.3 has 12 public keyboard functions. HPGCC 1.1 adds matrix-oriented helpers and reaches 17; all 17 remain signature-identical in 2.0. HPGCC3 reorganizes keyboard handling around raw state plus an asynchronous message queue.

| Historical behavior | HPGCC calls | hpclang status |
| --- | --- | --- |
| raw physical matrix | `keyb_getmatrix`, `keyb_isKeyPressed` | ✅ Native raw snapshot and coordinate query. Historical logical matrix numbering is not reused. |
| any key | `keyb_isAnyKeyPressed` | ✅ |
| dedicated modifiers | `keyb_isAlpha`, `keyb_isLS`, `keyb_isRS`, `keyb_isON` | ✅ Physical held-state queries. |
| arrows | `keyb_isLeft/Right/Up/Down` | ✅ Arrow identities are part of the measured `hpclang_key` set. |
| blocking wait | `keyb_waitKeyPressed` | ◐ Can be built by polling event/time services; no canonical blocking helper yet. |
| logical key return | `keyb_getkey`, `keyb_getkeyM`, `keyb_getnextkey` | ◐ Physical DOWN/UP/REPEAT exists; historical key-plane/text translation does not. |
| HPGCC3 async queue | KEYDN/PRESS/KEYUP machinery | ◐ Core event semantics exist; global IRQ-owned queue policy is intentionally not copied. |

## Time, system and machine control

Historical `hpsys.h` mixes time, clock control, interrupts, LCD state, address mapping, physical allocation, tones and error helpers. hpclang intentionally splits those concerns.

| Historical area | Representative HPGCC calls | hpclang |
| --- | --- | --- |
| elapsed time/delay | `sys_updateTimer`, `sys_sleep`, `sys_waitTicks`, `sys_waitRTCTicks` | ✅ Monotonic microseconds + explicit delay; source adapter absent. |
| RTC wall clock | `sys_RTC_seconds`, `sys_tm_RTC` | ⬜ |
| clock speed | `sys_slowOn/off`, `sys_clksave/restore` | ✕ canonical app API unless a real generic policy use case appears. |
| interrupt/timer hardware | `sys_int*`, `sys_setupTimers` | ✕/↪ backend ownership detail. |
| LCD hardware state | `sys_lcd*`, `setDisplayPlane` | ↪ Surface/display API; physical grayscale ownership pending. |
| virtual/physical mapping | `sys_map_*` | ✕ canonical application API. |
| physical allocation/cache | `sys_phys_malloc`, `sys_flush_cache` | ✕ canonical API; specialized DMA resource support may justify a later low-level service. |
| tone/beep | `sys_playTone*`, `kos_beep` | ⬜ |

## Files, stdio and storage

HPGCC's historical SD stdio was not universally available: the 1.1 Programming Guide documents the normal file path for HP49g+ rather than HP48gII/39g+.

hpclang now provides standard C stream semantics on the platform that actually has a suitable stock byte-stream filesystem:

| Capability | Status |
| --- | --- |
| libc formatting/string/memory | ✅ |
| heap-backed libc/C++ containers | ✅ |
| HP49g+/HP50g file open/read/write/seek/close | ✅ through picolibc |
| create/truncate/append/remove/ENOENT | ✅ Stock E2E |
| unsupported stream access on HP39/40 and HP48 | ✅ clean `ENOSYS` |
| directory iteration | ⬜ |
| HPGCC2/3 `FS*` adapter | ⬜ |
| public raw KOS file API | ✕; backend remains private |

HP39 typed Aplet/Note/Program/etc. storage is deliberately a different M7.3-style object service rather than synthetic POSIX files.

## Saturn/RPL and calculator objects

HPGCC 1.1 exposes about fifty `saturn.h` functions plus 29 `satdir.h` functions. HPGCC 2.0 extends that surface; HPGCC3 replaces it with a larger `sat3_*` environment.

Current canonical hpclang status: **not implemented**. The core will not expose Saturn addresses as ordinary C pointers; it needs explicit address/object handles, copy/convert operations, ownership rules, and stack access where supported.

## KOS, raw syscalls and widgets

The raw nine-function `syscall.h` surface is declaration-stable across all four historical generations examined. It remains useful evidence but is not a good canonical application API because it exposes stock calling conventions directly.

Status: no canonical raw-syscall API. Required host services stay behind typed hpclang services; compatibility wrappers can be considered in M8.

HPGCC 1.1/2.0 win widgets and HPGCC3 GUI are also not implemented. Graphics/input/time/storage should stabilize first.

## Compatibility classes

| Class | Meaning |
| --- | --- |
| Direct | Historical source behavior can be implemented safely and cheaply on top of hpclang. |
| Adapter | Common source patterns can be retained, but state/types/coordinates need translation. |
| Migration | No historical wrapper in the canonical SDK; document the hpclang replacement. |
| Unsupported | Historical behavior conflicts with hpclang architecture or cannot be made reliable. |

Binary compatibility with old HPGCC executables is not a goal. HPGCC3 custom-ROM/ROMLIB ABI is not an hpclang target.

## Near-term API order

Based on historical stability, current implementation, and remaining dependencies:

1. ✅ owned 2/4/16-level surfaces, geometry and blit;
2. ✅ five-model physical keyboard identity/event core;
3. ✅ calculator-native font borrowing plus generated custom-font package with `STATIC_AUTO`, `STATIC_EXPLICIT`, and `FULL`;
4. ✅ hp4950 standard-C stream storage through picolibc;
5. ✅ exhaustive Direct Font Gallery plus HP50g and HP48gII compact Stock live-native controls; font implementation/evidence closure is complete;
6. design Saturn/RPL and HP39 typed-object handles;
7. only then begin broad HPG/keyboard/object compatibility headers.

This preserves the dependency direction: historical HPGCC APIs are implemented on top of a coherent hpclang core, not used to define that core.

### Keyboard action boundary

The public keyboard service stops at physical key identity plus modifier state. Model/user-interface actions such as HP39 `Shift+VARS -> CHARS` are not folded back into `enum hpclang_key`. Shortcut-to-command mappings belong to a higher application or compatibility layer.

### Current bitmap-only font closure status (2026-09-14)

Canonical bundled assets are bitmap-source only. The generated catalog is **245 faces / 1,639,485 mapped glyphs / 3,472 assembly sources** with **245/245** source-to-generated parity. Font Assets **#24** is green and regeneration produces no drift; the general TTF importer has been removed.

Final bitmap curation adds Cubic 11, ChillBitmap 7px, and QuanPixel/May7 with source provenance retained. Ark Pixel labels/specimens now expose useful locale differences, and Gallery text covers width-sensitive Latin, punctuation/digits, accents/symbols, and supported CJK/Japanese/Korean repertoires.

`E2E / Font Gallery` **35255868206 / #99** is fully green. Direct traverses **247 entries (245 generated + 2 native) × 2 pages = 494 captures per target** on HP39gs, HP40gs, HP48gII, HP49g+, and HP50g. HP50 Stock reaches settled PASS. HP48 Stock has HPC1 PASS/control-flow return, distinct nonblank live-native/generated evidence, and a settled Stock repaint; Gallery #102 independently passes the new strict HP48 return gate.

The font slice therefore has no remaining API or evidence blocker. Repository integration and bounded Stock-harness closeout remain outside the font-core closure.
