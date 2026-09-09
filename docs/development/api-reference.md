# API reference and HPGCC parity

This is the progress-oriented API reference for hpclang. It deliberately answers two different questions:

1. What can a native hpclang application call today?
2. How much of the historical HPGCC application surface has an hpclang equivalent yet?

The historical comparison is based on the complete HPGCC 1.0.3 beta, HPGCC 1.1 with 39g support, HPGCC 2.0 SP2, and HPGCC3 R004.revA distribution archives. Function counts below are de-duplicated public function names extracted from their installed/public headers. They are not a compatibility percentage: one filesystem or Saturn function is not equivalent in engineering cost to one drawing primitive.

## Status legend

| Mark | Meaning |
| --- | --- |
| ✅ | Native hpclang API exists and calculator-dependent behavior has passed the five-model Direct matrix. |
| 🧪 | Implemented, but the new behavior is awaiting the next calculator matrix or has narrower validation. |
| ⛔ | Blocker: implementation exists, but a required asset/contract is not yet frozen for release. |
| ◐ | A cleaner hpclang primitive covers part of the historical behavior; compatibility semantics are incomplete. |
| ⬜ | Planned or not implemented. |
| ↪ | Prefer a different hpclang or standard-library API; a historical wrapper would be migration/compatibility only. |
| ✕ | Intentionally not part of the canonical hpclang application API. |

Source-compatibility wrappers are not counted as implemented merely because the canonical core already has enough primitives to build them.

## What is public application API

`<hpclang/bootstrap.h>` is a launcher/platform contract. Normal applications should not use its MMU layout or launch-context structures.

The application-facing surface currently lives in:

- `<hpclang/abi.h>`
- `<hpclang/target.h>`
- `<hpclang/device.h>`
- `<hpclang/time.h>` and `<hpclang/time.hpp>`
- `<hpclang/keyboard.h>`
- `<hpclang/display.h>` and `<hpclang/display.hpp>`

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
| `enum hpclang_key` / `event.key` | ✅ | Complete physical-key identities for HP39gs/40gs, HP48gII, HP49g+, and HP50g. Common keys keep common enum values; layout-specific keys use model/family prefixes. HP49g+ CAT/EQW and HP50g EVAL/quote are represented separately because those two primary labels differ at the same physical positions. All 51 physical keys pass Direct E2E on all five automated models. HP39g+ remains UNKNOWN pending its real fixture. |
| `HPCLANG_KEYBOARD_MOD_SHIFT` | ✅ | Portable physical “a Shift key is held” bit. HP48/49/50 retain left/right detail; HP39 has one Shift. Dedicated modifier state is exercised as part of the five-model 51-key Direct matrix. |
| text / Alpha / Shift-plane translation | ⬜ | Higher layer, not part of the raw/debounce core. |

## Display and surfaces

The canonical coordinate system has a top-left origin. Rectangle APIs use `x, y, width, height`; this intentionally differs from historical HPG inclusive `x1, y1, x2, y2` rectangle calls.

| API | Status | Contract |
| --- | --- | --- |
| `hpclang_display_surface()` | ✅ | Borrow the physical display as an opaque surface. |
| `hpclang_surface_width()` | ✅ | Visible surface width. |
| `hpclang_surface_height()` | ✅ | Visible surface height. |
| `hpclang_surface_levels()` | ✅ | Current physical surfaces report two levels. |
| `hpclang_surface_clear()` | ✅ | Clear with explicit 0–255 intensity. |
| `hpclang_surface_get_pixel()` | ✅ | Read one pixel; out-of-bounds reads white. Five-model Direct and full-stock matrices PASS. |
| `hpclang_surface_set_pixel()` | ✅ | Clipped pixel write. |
| `hpclang_surface_draw_line()` | ✅ | Geometrically clipped line; extreme off-screen spans clip before rasterization. |
| `hpclang_surface_draw_rect()` | ✅ | Outline rectangle with extent semantics and geometric clipping. Five-model Direct and full-stock matrices PASS. |
| `hpclang_surface_draw_circle()` / `hpclang_surface_fill_circle()` | ✅ | Integer-radius circles clipped to visible rows/columns before rasterization; Core, five-model Direct, and full-stock matrices PASS. |
| `hpclang_surface_draw_polygon()` / `hpclang_surface_fill_polygon()` | ✅ | Explicit `hpclang_point` arrays; closed outline and even-odd fill with boundary inclusion. Core, five-model Direct, and full-stock matrices PASS. |
| `hpclang_surface_blit()` | ✅ | Representation-independent clipped copy with same-surface overlap support. Five-model Direct and full-stock matrices PASS. |
| `hpclang_surface_fill_rect()` | ✅ | Clipped filled rectangle. |
| `hpclang_surface_create()` / `hpclang_surface_create_mono()` / `hpclang_surface_destroy()` | ✅ | Opaque heap-owned 2/4/16-level software surfaces; mono convenience retained. Packing stays private and cross-level blit uses the common intensity contract. Core, five-model Direct, and full-stock matrices PASS. |
| `hpclang::surface_ref` | ✅ | Zero-owning C++ wrapper; owned-surface/blit calls are exercised from the five-model C++23 runtime probe. |

## Fonts and text

The canonical layer is deliberately narrower than historical HPG:

| API | Status | Contract |
| --- | --- | --- |
| `hpclang_font_mini()` | ⛔ | API/metric stable (height 6, advance 4); current glyph table is provisional pending final font selection. |
| `hpclang_font_big()` | ⛔ | API/metric stable (height 8, advance 6); current glyph table is provisional pending final font selection. |
| `hpclang_font_height()` / `hpclang_font_advance()` | ✅ | Fixed metrics without exposing bitmap storage. |
| `hpclang_surface_draw_char()` | ✅ | Foreground-only clipped glyph draw on an explicit surface and intensity. |
| `hpclang_surface_draw_text()` | ✅ | Fixed advance; newline resets to the original x and moves by one font height. |

Bytes outside printable Basic Latin render as `?`. That is an intentional
canonical v1 scope, not a claim that these built-ins reproduce HPG's historical
256-entry calculator character set. HPG-specific glyph repertoire and custom
font descriptors belong to the later compatibility layer unless an independent
core use case requires them.

## Storage model

hpclang distinguishes two storage semantics:

| Storage class | Platform status | Canonical direction |
| --- | --- | --- |
| byte-stream files | HP49g+/HP50g Stock KOS SD path proven by E2E read probe | private POSIX-style backend feeding picolibc `fopen/fread/fwrite/fseek`; no parallel hpclang stdio |
| calculator object store | HP39gs/40gs stock format proven to contain directory, Zaplet/Aplet, Note, Program, Variable, List, Matrix, Library and related object types | typed/named object API; do not flatten into `FILE *` |
| RPL/Saturn objects | HP48/49/50 historical platform capability | M7.3 typed object/stack bridge |

The HP39 object-store root is `HP39DIR.000`; `HP39DIR.CUR` tracks the
current directory. These names describe the stock object-store protocol, not a
claim that HP39 exposes a general POSIX filesystem.

## Validation by service

| Service | Host | ARMv4T link | Direct runtime | Full stock launch |
| --- | --- | --- | --- | --- |
| HPC1/runtime | ✅ | ✅ | 5/5 PASS | 5/5 PASS |
| device info | — | ✅ | 5/5 PASS | covered by common payload |
| monotonic time/delay | ✅ math | ✅ | 5/5 PASS | covered by common payload |
| raw keyboard | — | ✅ | 5/5 transition PASS | not separately asserted |
| keyboard events + key identity | ✅ | ✅ | 5/5 full 51-key DOWN/UP + scan + identity + modifier matrix PASS | not separately asserted |
| display clear/set/line/fill | ✅ | ✅ | 5/5 PASS | visible payload |
| owned 2/4/16-level surface + blit | ✅ | ✅ | 5/5 PASS | 5/5 PASS |
| font/text | ✅ | ✅ | 5/5 PASS | 5/5 PASS |
| display get-pixel/draw-rect | ✅ | ✅ | 5/5 PASS | 5/5 PASS |
| C++23 + libc++ | ✅ | ✅ | 5/5 real-service PASS | same HPC1 payload builds |

HP39g+ remains outside emulator E2E until a trustworthy real-machine full-flash fixture exists. This is a fixture blocker, not a separate API architecture.

## Historical public-surface size

The following is a mechanical unique-public-function-name view of the major calculator-facing headers.

| Historical surface | 1.0.3 | 1.1 39G | 2.0 SP2 | HPGCC3 | hpclang today |
| --- | ---: | ---: | ---: | ---: | --- |
| HPG `hpgraphics.h` | 55 | 59 | 59 | 59 | core surface primitives in progress; no HPG adapter yet |
| keyboard `hpkeyb49.h` | 12 | 17 | 17 | — | raw + event core implemented |
| HPGCC3 `keyb.h` | — | — | — | 8 functions plus queue/macros | event core implemented; queue policy not copied |
| legacy `hpscreen.h` | 12 | 9 | 9 | — | migrate to opaque surface |
| GGL `ggl.h` | — | 28 | 28 | 28 | not implemented |
| system `hpsys.h` | 25 | 40 | 41 | — | monotonic time/device subset |
| HPGCC3 `sys.h` | — | — | — | 61 | small generic subset only |
| `hptime.h` | — | 3 | 3 | — | monotonic/chrono API instead |
| `hpstdio.h` | 26 | 37 | 40 | — | formatting/libc works; filesystem stdio does not |
| HPGCC3 `stdio.h` | — | — | — | 40 | formatting/libc works; filesystem stdio does not |
| structured `fsystem.h` | — | — | 41 | 43 | not implemented |
| KOS `kos.h` | 15 | 32 | 33 | 33 | no canonical raw-KOS surface |
| Saturn `saturn.h` | — | 50 | 58 | replaced | not implemented |
| Saturn directory `satdir.h` | — | 29 | 29 | replaced | not implemented |
| conio | 27 | 27 | 26 | 49 in new `conio.h` | not implemented as a subsystem |
| raw `syscall.h` | 9 | 9 | 9 | 9 | intentionally not canonical |

HPGCC 1.1/2.0 also contain a substantial win widget/event family with 74 stable implementation names between those releases. HPGCC3 moves to a different GUI family. hpclang has not started a widget subsystem.

## HPG graphics: detailed progress

HPG is unusually stable. All 55 HPG 1.0.3 declarations survive exactly into 1.1, which adds only four functions; all 59 declarations are signature-stable from 1.1 to 2.0 and remain present in HPGCC3.

| HPG group | Historical calls | hpclang status |
| --- | --- | --- |
| lifecycle | `hpg_init`, `hpg_cleanup` | ↪ Core surface access needs no matching global lifecycle; an adapter can provide legacy setup semantics later. |
| display mode | `hpg_set_mode_mono`, `hpg_set_mode_gray4`, `hpg_set_mode_gray16`, `hpg_flip` | ⬜ Physical grayscale/double buffering/page flip are explicitly deferred pending a resource broker for contiguous LCD DMA memory. |
| geometry | `hpg_get_width`, `hpg_get_height` | ✅ `hpclang_surface_width/height`. |
| indicators | `hpg_set_indicator` | ↪ Historical HPG writes x=131, y=0..5 outside the visible 131-pixel surface and assigns HP49-style annunciator meanings. Keep this model-capability/adapter work rather than widening the canonical surface. |
| clipping state | `hpg_clip_reset`, `hpg_clip_set`, `hpg_clip` | ◐ Every primitive clips to surface bounds; mutable user clip rectangles are not implemented. |
| clear | `hpg_clear_on`, `hpg_clear` | ✅ `hpclang_surface_clear`; default-screen/global-state variants belong in an adapter. |
| pixel | `hpg_draw_pixel_on`, `hpg_draw_pixel`, `hpg_get_pixel` | ✅ Canonical read/write primitives are five-model validated; HPG wrappers remain absent. |
| line | `hpg_draw_line_on`, `hpg_draw_line` | ✅ `hpclang_surface_draw_line`. |
| rectangle | `hpg_draw_rect_on`, `hpg_draw_rect`, `hpg_fill_rect_on`, `hpg_fill_rect` | ✅ Canonical outline/fill primitives are five-model validated; HPG coordinate/state adapters remain absent. |
| circle | `hpg_draw_circle_on`, `hpg_draw_circle`, `hpg_fill_circle_on`, `hpg_fill_circle` | ✅ Canonical clipped outline/fill circle primitives are merged and five-model validated; HPG context wrappers remain later work. |
| polygon | `hpg_draw_polygon_on`, `hpg_draw_polygon`, `hpg_fill_polygon_on`, `hpg_fill_polygon` | ✅ Canonical point-array outline/fill primitives with even-odd fill are merged and five-model validated; HPG array/state adapters remain later work. |
| fonts | `hpg_get_minifont`, `hpg_get_bigfont`, font metrics, allocation/free | ⛔ Renderer and 6/4 + 8/6 metrics are stable, but final built-in glyph data is blocked pending font selection/provenance. HPG's two bundled 256-entry fonts are a compatibility candidate/reference, not automatically MIT-relicensed data. |
| text | `hpg_draw_letter[_on]`, `hpg_draw_text[_on]` | ✅ Canonical explicit-surface character/text drawing is merged and five-model Direct/Stock validated; HPG mutable-context wrappers remain later work. |
| mutable style | color, mode, pattern and font getters/setters | ↪ Canonical primitives take explicit intensity. An HPG adapter can own legacy mutable state. |
| PAINT/XOR composite mode | HPG context mode | ↪ Historical XOR is per-context mutable state. If a clean-core compositing primitive is later needed, it should be explicit per operation rather than hidden in the surface. |
| patterns | `hpg_alloc_pattern`, `hpg_free_pattern` | ↪ Historical 8-bit repeating masks belong naturally to the HPG adapter unless a non-HPG use case appears. |
| owned images | mono/gray image allocation/free | ◐ Generic 2/4/16-level software surfaces cover the clean-core ownership model; HPG constructors remain adapter work. |
| blit | `hpg_blit` | ◐ Canonical blit is implemented; HPG state/argument adapter remains later work. |
| XPM | mono/gray4/gray16 XPM loaders | ↪ Convenience importers should be built above owned surfaces/blit or in the HPG adapter, not baked into the framebuffer ABI. |

The current canonical primitives now cover the clean implementation basis for most stateless HPG geometry, text, software-image, and blit operations. Zero HPG source-compatibility wrappers are claimed yet; mutable context state, physical display ownership, and convenience loaders remain separate compatibility work.

## Keyboard: detailed progress

HPGCC 1.0.3 has 12 public keyboard functions. HPGCC 1.1 adds matrix-oriented helpers and reaches 17; all 17 remain signature-identical in 2.0. HPGCC3 reorganizes keyboard handling around raw state plus an asynchronous message queue.

| Historical behavior | HPGCC calls | hpclang status |
| --- | --- | --- |
| raw physical matrix | `keyb_getmatrix`, `keyb_isKeyPressed` | ✅ Native raw snapshot and coordinate query. Historical logical matrix numbering is not reused. |
| any key | `keyb_isAnyKeyPressed` | ✅ |
| dedicated modifiers | `keyb_isAlpha`, `keyb_isLS`, `keyb_isRS`, `keyb_isON` | ✅ Physical held-state queries. |
| arrows | `keyb_isLeft/Right/Up/Down` | ✅ Arrow identities are part of the five-model measured `hpclang_key` set. |
| blocking wait | `keyb_waitKeyPressed` | ◐ Can be built by polling event/time services; no canonical blocking helper yet. |
| logical key return | `keyb_getkey`, `keyb_getkeyM`, `keyb_getnextkey` | ◐ DOWN/UP/REPEAT plus measured physical key identity exist; historical key-plane/text translation does not. |
| matrix keycode helpers | `keyb_isKeyPrM`, `keyb_isShiftKeyPrM` | ↪ Adapter concern above the measured hpclang raw/semantic layers. |
| HPGCC3 async queue | KEYDN/PRESS/KEYUP message machinery | ◐ hpclang event semantics exist, but the stock backend is deliberately polled and does not copy HPGCC3 global queue/IRQ ownership. |

## Time, system and machine control

Historical `hpsys.h` mixes time, clock control, interrupt ownership, LCD state, address mapping, physical allocation, tones and error helpers. hpclang intentionally does not recreate that mixture as one header.

| Historical area | Representative HPGCC calls | hpclang |
| --- | --- | --- |
| elapsed time/delay | `sys_updateTimer`, `sys_sleep`, `sys_waitTicks`, `sys_waitRTCTicks` | ✅ Monotonic microseconds + explicit delay; source adapter absent. |
| RTC wall clock | `sys_RTC_seconds`, `sys_tm_RTC`, `asctime` | ⬜ |
| clock speed | `sys_slowOn/off`, `sys_clksave/restore` | ✕ canonical app API; only a generic policy API if a real use case requires it. |
| interrupt ownership | `sys_intOff/on` | ✕ routine application API; execution safety belongs below the service boundary. |
| timer hardware setup | `sys_setupTimers`, `sys_restoreTimers`, PWM helpers | ✕/↪ backend detail unless a genuine low-level capability needs exposure. |
| LCD hardware state | `sys_lcd*`, `setDisplayPlane` | ↪ surface/display API; grayscale and buffering still pending. |
| virtual/physical mapping | `sys_map_v2p`, `sys_map_p2v`, `sys_mem_classify` | ✕ canonical application API. |
| physical allocation/cache | `sys_phys_malloc`, `sys_flush_cache` | ✕ canonical application API; specialized DMA support may justify a later low-level API. |
| tone/beep | `sys_playTone*`, `kos_beep` | ⬜ audio service not started. |
| fatal helpers | `sys_error`, `sys_chkptr` | ↪ normal C/C++ error/assertion policy. |

## Files, stdio and storage

This is the largest practical SDK gap visible from the historical APIs.

- HPGCC 1.x exposes file declarations through `hpstdio.h` and KOS helpers, but the HPGCC 1.1 Programming Guide explicitly documents its SD-card `fopen/fread/fwrite/...` path as HP49g+-only and warns not to use it on 48gII or 39g+.
- HPGCC 2.0 adds the structured 41-function `fsystem.h`.
- HPGCC3 retains those 41 declarations and adds `FSGetFileFromHandle` and `FSGetHandle`.

hpclang already validates picolibc formatting, memory/string operations, heap allocation and `snprintf`. That does not mean calculator filesystem-backed `fopen/fread/fwrite` works.

| Capability | Status |
| --- | --- |
| libc formatting/string/memory | ✅ |
| heap-backed libc/C++ containers | ✅ |
| calculator file open/read/seek/close | ⬜ |
| directory iteration | ⬜ |
| writable files/storage | ⬜ |
| standard `FILE *` integration | ⬜ |
| HPGCC2/3 `FS*` adapter | ⬜ |
| KOS file API adapter | ⬜ |

The intended direction is a small calculator storage service underneath picolibc where standard C semantics fit, with compatibility adapters above it rather than a parallel stdio universe. Platform backends must reflect real storage/host capabilities instead of treating the historical HP49g+ SD implementation as a universal calculator filesystem.

## Saturn/RPL and calculator objects

This is the other large M7 gap.

HPGCC 1.1 exposes about fifty `saturn.h` functions plus 29 `satdir.h` functions. HPGCC 2.0 extends the Saturn surface while retaining the directory surface. HPGCC3 replaces that primary interface with a much larger `sat3_*` environment.

Current hpclang status: **not implemented**.

The canonical design will not expose Saturn addresses as ordinary C pointers. It needs explicit address/object handles, copy/convert operations, documented ownership, stack access where the host provides it, and compatibility adapters for useful historical calls.

## KOS, raw syscalls and widgets

The raw nine-function `syscall.h` surface is declaration-stable across all four historical generations examined. That makes it useful evidence, but not a good canonical application API because it exposes stock-environment calling conventions directly.

Status: no canonical raw-syscall API. Required host services should be wrapped behind typed hpclang services; compatibility wrappers can be considered in M8.

HPGCC 1.1/2.0 win widgets and HPGCC3 GUI are also not implemented. Graphics/input/time/storage should stabilize first; a higher-level widget layer should be driven by real applications rather than copied wholesale.

## Compatibility classes

| Class | Meaning |
| --- | --- |
| Direct | Historical source behavior can be implemented safely and cheaply on top of hpclang. |
| Adapter | Common source patterns can be retained, but state/types/coordinates need translation. |
| Migration | No historical wrapper in the canonical SDK; document the hpclang replacement. |
| Unsupported | Historical behavior conflicts with hpclang architecture or cannot be made reliable. |

Binary compatibility with old HPGCC executables is not a goal. HPGCC3 custom-ROM/ROMLIB ABI is not an hpclang target.

## Near-term API order

Based on historical stability, example usage and current implementation dependencies:

1. ✅ close owned mono surfaces and blit through host, C++23 and five-model runtime gates;
2. ✅ measure and expose model-neutral physical key identities across the three validated platform families;
3. ⛔ freeze final mini/big glyph data and provenance; renderer/metrics are validated but font selection remains a blocker;
4. establish calculator storage/file services and wire standard stdio where semantics fit;
5. design Saturn/RPL object and stack handles;
6. only then begin HPG/keyboard/stdio compatibility headers.

This preserves the dependency direction: historical HPGCC APIs are implemented on top of a coherent hpclang core, not used to define that core.


### Keyboard action boundary

The public keyboard service stops at physical key identity plus modifier state.
Model/user-interface actions such as HP39 `Shift+VARS -> CHARS` are not folded
back into `enum hpclang_key`. This follows the same separation used by modern
input stacks: key/scancode and modifier state remain in the keyboard layer,
while shortcut-to-command mappings belong to a higher application or
compatibility layer. HPGCC source compatibility may reproduce its historical
key-plane behavior in the compatibility adapter without changing the clean core
ABI.
