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
- `<hpclang/datetime.h>`, `<hpclang/timer.h>`, `<hpclang/sound.h>` and `<hpclang/power.h>`
- `<hpclang/input.h>` for bounded UTF-8 line editing
- `<hpclang/console.h>` for an owned Unicode console viewport
- `<hpclang/stdio.h>` for explicit standard-stream callback binding
- `<hpclang/keyboard.h>`
- `<hpclang/display.h>` and `<hpclang/display.hpp>`
- `<hpclang/font.h>`
- `<hpclang/font-package.h>` for generated bitmap-font descriptors/build products
- `<hpclang/storage.h>` for HP49g+/HP50g byte-stream metadata/media queries
- `<hpclang/object.h>` for owned calculator-object snapshots and stack exchange
- `<hpclang/object-store.h>` for HP48gII/HP49g+/HP50g named RPL stores
- `<hpclang/variable.h>` for HP39 native schema-defined variable values

Bounded HPGCC source-compatibility headers also now exist as `<satdir.h>` and `<saturn.h>`. They are **adapters above the canonical object/stack API**, not alternate core object models. In particular they do not expose raw Saturn memory access, real Saturn-address descriptors, or borrowed Stock-memory lifetimes.

## Execution and device identity

| API | Status | Notes |
| --- | --- | --- |
| `hpclang_boot_info()` | ✅ | Process-lifetime boot record under the common HPC1 ABI. |
| `enum hpclang_model` | ✅ | Stable model IDs for HP39g+, HP39gs, HP40gs, HP48gII, HP49g+, HP50g. |
| `enum hpclang_family` | ✅ | `hp39`, `hp48`, `hp4950`; implementation grouping rather than an application ABI fork. |
| `hpclang_device_info()` | ✅ | Model, family and visible LCD geometry. |
| `hpclang_device_capability()` | 🧪 | Service support by model/launch context; distinguishes missing RPL context from unsupported hardware. Does not reserve resources or guarantee current media/hardware readiness. |

### Object-service discovery

Use `hpclang_device_capability()` rather than decoding the private launch cookie.
`RPL_STACK` and `RPL_OBJECT_STORE` describe read access; their new
`RPL_STACK_MUTATION` and `RPL_OBJECT_STORE_MUTATION` counterparts also require
the launcher's validated memory bounds. Existing capability IDs retain their
values. HP39 queries validate the portal prefix and entry before reporting support.

| Capability | HP39g+ / HP39gs / HP40gs | HP48gII / HP49g+ / HP50g |
| --- | --- | --- |
| `RPL_STACK`, `RPL_OBJECT_STORE` | Unsupported | Supported with a live Saturn context; otherwise context unavailable |
| `RPL_STACK_MUTATION`, `RPL_OBJECT_STORE_MUTATION` | Unsupported | Supported with a live Saturn context and valid mutation bounds; otherwise context unavailable |
| `APLET_DATA`, `APLET_VARIABLES`, `TEXT_CATALOG`, `APLET_CATALOG` | Supported with a live Saturn context and valid Aplet portal; otherwise context unavailable | Unsupported |

Normal Stock launchers provide these family-specific contexts. Direct launches
do not provide calculator-object access. Queries read launcher-owned RAM without
calling Stock, accessing hardware, allocating memory or changing `errno` for a
known capability. They do not validate every live object or promise free space:
the requested operation still checks its arguments, mapping, resources and data.
Unknown IDs return `-1` with `EINVAL`. Native Port 0/1/2 services are not covered
by the HOME object-store capabilities.

Host regression tests cover all six models, absent contexts, invalid portal
prefixes and invalid mutation bounds. System-service E2E checks discovery under
Direct and Stock; object-mutation E2E checks support before exercising writes.
The expanded discovery checks await CI acceptance.

## Time and system services

| API | Status | Contract |
| --- | --- | --- |
| `hpclang_time_monotonic_us()` | ✅ | Monotonic microseconds since runtime entry. The stock backend observes existing timer/RTC state. |
| `hpclang_time_read_us()` | 🧪 | Checked monotonic read; reports an unavailable clock instead of returning the last sample. |
| `hpclang_datetime_read()` | 🧪 | Calculator civil clock, RTC years 2000–2099; read-only and bounded retry. |
| `hpclang_datetime_set()` | 🧪 | Set civil RTC years 2000–2099; preserves monotonic deadlines and RTC alarm configuration. |
| `hpclang_datetime_to_seconds/from_seconds()` | 🧪 | Gregorian years 1–9999, seconds relative to civil 2000-01-01; no UTC/timezone assumption. |
| `hpclang_datetime_weekday()` | 🧪 | Computed weekday, Sunday=0; does not trust the RTC weekday register. |
| `hpclang_timer_start/poll/cancel()` | 🧪 | Caller-owned one-shot/periodic events; cooperative polling, missed-expiration counts, cancellation and overflow checks. |
| `hpclang_sound_play()` | 🧪 | Blocking PWM tone with frequency/duration, cooperative cancellation and restoration of borrowed Timer2/GPIO/IRQ state. |
| `hpclang_delay_us()` | ✅ | Explicit microsecond busy-wait; returns rather than deadlocking if the clock cannot be sampled. |
| `hpclang::monotonic_time()` | ✅ | C++23 chrono wrapper returning `std::chrono::microseconds`. |
| `hpclang::delay()` | ✅ | C++23 chrono duration wrapper. |
| `hpclang_power_usb_present()` | 🧪 | USB voltage-detect input; does not infer battery percentage, enumeration or actual power-source selection. |
| `hpclang_display_contrast_read()` | 🧪 | Stock contrast ordinal 0–15, queried without changing the setting. |

These M7.4 services are formally accepted at functional head `06ab4deb`, including
the six-model Direct/Stock gate and cross-workflow regressions.
Battery percentage/voltage is not inferred from an uncalibrated ADC reading.

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
| `enum hpclang_key` / `event.key` | ✅ | Complete physical-key identities for HP39g+/39gs/40gs, HP48gII, HP49g+, and HP50g. Model 1 passes the same strict 51-key Direct transition gate against its verified D1.55 full-NOR fixture rather than a sibling inference. |
| `HPCLANG_KEYBOARD_MOD_SHIFT` | ✅ | Portable physical “a Shift key is held” bit. HP48/49/50 retain left/right detail; HP39 has one Shift. |
| text / Alpha / Shift-plane translation | ⬜ | Higher layer, not part of the raw/debounce core. |

### Bounded application input

`hpclang_text_input_init/edit/key()` provide allocation-free UTF-8 line editing in
an explicitly sized caller buffer, Unicode-scalar cursor movement/deletion,
capacity errors, accept and cancel. The key helper covers common numeric/editing
keys; applications or future HPG adapters supply Alpha/Shift-plane text through
explicit Unicode insertion. It does not introduce a global terminal or change
stdin/stdout behavior. The application chooses rendering, event polling and
whether to retain a cancelled working line.

The default `stdin` returns EOF and `stdout`/`stderr` discard output until an
application explicitly binds them. The console/standard-stream extension below
supplies that routing; text-plane mapping and a ready-made keyboard line-input
loop remain separate follow-ups. File-backed `FILE *` support is unaffected.

`hpclang_surface_draw_text_n()` renders bounded message/error text without requiring
a terminator, exiting, waiting for a key or clearing the caller's surface.
Both services have host tests and accepted six-model Direct/Stock ARM E2E
coverage in [M7.4](validation.md#m74-formal-acceptance).

The numeric-input extension adds `hpclang_text_input_parse_int64()` and
`hpclang_text_input_parse_double()` (accepted on `b92e2a7d`). They
validate the entire current line and write an output only on success. This lets
an application reject an invalid Enter/accept attempt and continue editing the
same buffer. Cancellation remains the application's decision.

| Conversion | Accepted syntax | Failure |
| --- | --- | --- |
| Signed 64-bit integer | Optional leading sign, then decimal digits | `EINVAL` for incomplete/invalid text; `ERANGE` outside `INT64_MIN..INT64_MAX` |
| C `double` | Optional sign, decimal mantissa with optional `.`, at least one mantissa digit, optional `e/E` and signed decimal exponent | `EINVAL` for incomplete/invalid text; `ERANGE` for overflow or a nonzero mantissa rounded to zero |

Whitespace, grouping separators, expressions, hexadecimal, NaN and infinity are
rejected. Integer conversion is exact; floating conversion uses the SDK C-locale
`strtod` rounding, including finite subnormals and signed zero. Neither conversion
changes the text/cursor, consumes keyboard events, chooses an Alpha/Shift policy,
nor accepts the editor automatically. Successful conversion preserves `errno`;
failure preserves the output value. A valid binary floating-point result does
not imply exact decimal calculator-object precision.

For example, after the editor reports `HPCLANG_INPUT_ACCEPTED`:

```c
int64_t value;
if (hpclang_text_input_parse_int64(&input, &value) == 0) {
    /* Commit value to the application. */
} else {
    /* Show EINVAL/ERANGE and keep editing input. */
}
```

### Explicit Alpha letter lookup

`hpclang_text_alpha_lookup(model, key, lowercase, &codepoint)` maps a physical
key to its printed Latin letter on all six models. `lowercase` is explicitly 0
or 1. Return 1 means a letter/space was found; 0 means no mapping and leaves the
output untouched. Invalid arguments return -1/`EINVAL`; unknown models return
-1/`ENOSYS`. Successful calls preserve errno.

This pure text-layer helper does not consume events or decide whether Alpha is
held, latched or locked. The caller selects the plane. HP39 Alpha-Plus maps to
space; Enter has no Alpha letter. HP48/49/50 use their dedicated space key.
Modifier keys, editing commands,
digits without Alpha letters, punctuation planes and non-Latin symbols are not
mapped by this helper. Call it only for the chosen text plane and DOWN/REPEAT
events, before falling back to editing controls. It is implemented pending CI;
physical-input acceptance remains open.

### Native text-mode state

The caller-owned `hpclang_text_mode` selects a text plane from the complete
physical event stream. It is implemented pending CI. Initialize a fresh mode
for each input session; feed modifier releases as well as key presses. It does
not poll/reset the keyboard, insert characters, acquire a display or change
Stock's own state.

| Operation | Contract |
| --- | --- |
| `hpclang_text_mode_init(&mode, model)` | Start unlocked, with no pending shift. Unknown model: `ENOSYS`. |
| `hpclang_text_mode_event(&mode, &event, &plane)` | 1: route this DOWN/REPEAT through the selected plane; 0: consumed modifier or release; -1: invalid argument. Output changes only on 1. |
| `hpclang_text_mode_set_lock(&mode, lock)` | Explicit application/menu selection: unlocked, uppercase lock or lowercase lock. Consumes pending/held modifiers used to select the action. |
| `hpclang_text_mode_get(&mode, &status)` | Read the effective plane and persistent lock for the application's display. |

Successful calls preserve errno; invalid arguments return `EINVAL` without
changing state/output. The mode fields are private despite caller-owned storage.
Events remain unchanged, including their **physical** modifier bits. Simultaneous
presses are handled even when the key event precedes the modifier's own queued
DOWN event. Modifier REPEAT events do not cycle locks.

The default transitions follow the native model:

| Input | HP39g+/39gs/40gs | HP48gII/49g+/50g |
| --- | --- | --- |
| Alpha once | Next key uses Alpha; another Alpha cancels | Next key uses Alpha; another Alpha locks uppercase |
| Hold Alpha | Alpha remains effective while held | Alpha remains effective while held |
| Lowercase once | Shift before or after Alpha | Alpha, then left Shift |
| Shift while Alpha is locked | Temporarily invert letter case | Left Shift temporarily inverts case; right Shift selects symbols |
| Alpha while locked | Suspend lock for one ordinary key, as in Notes | Unlock |
| Left Shift, Alpha | Single lowercase entry with the HP39 Shift key | Toggle case only when already locked; otherwise a shifted command, clearing a pending single Alpha |
| Right Shift, Alpha | No separate right Shift key | Shifted command; preserves an existing lock |

HP39 locking is an application/menu action, like Notes' A..Z menu, rather than
a double-Alpha gesture. Use `set_lock()` for that action; selecting lowercase
lock while Shift is held does not immediately invert it again. A one-shot Shift
is consumed by the first key even when Alpha stays held: native HP39
Shift-then-held-Alpha with two presses of 2 produces `yY`.

The result plane is plain, left/right shifted, uppercase/lowercase Alpha or
Alpha symbols. For the two Latin planes, call `hpclang_text_alpha_lookup()` and
insert the resulting scalar with `hpclang_text_input_edit()`. The application
still routes editing actions, symbols and commands. A symbol plane must not be
passed to Latin lookup as though it were lowercase. This state layer does not
yet supply a complete symbol/command mapper or ready-made keyboard editor.

One-shot state is consumed by the key even if translation or insertion fails;
retrying an edit must not feed the same physical event twice. Repeat timing
remains the physical event service's configuration. Enter/ON are routed to the
application, which owns accept/cancel and the next session's initialization.
System Services composes this layer with the bounded editor and console.
In addition to scripted events, its physical GPIO sequences produce `AaZZ2`
and `ABcD2D2` through the real scanner and debounce queue. They cover single,
held and locked Alpha, temporary lowercase, ordinary input and leaving a lock;
captured text is compared with independently rendered literal references.
Full symbol lookup and a ready-made keyboard editor remain separate work.

### Bounded line sessions

`hpclang_text_input_run(input, callbacks)` supplies a synchronous event/edit/
validation/render loop over an initialized editor. This extension is implemented
and awaiting CI acceptance. It retains prefilled text and allocates
nothing. The caller supplies the event source and owns the display/context.

| Callback | Contract |
| --- | --- |
| `next` (required) | Return 1 with an event, 0 when idle, or -1 with errno to abort; may poll or wait |
| `edit` (optional) | Replace the basic `hpclang_text_input_key()` policy, e.g. with model-specific Alpha translation; same result/error contract |
| `accept` (optional) | Validate the proposed line: 0 accepts, 1 with errno rejects for correction, -1 with errno aborts |
| `update` (optional) | Initial notification and each event, including accept/cancel and recoverable errors; render/present as desired; 0 succeeds, -1 aborts |

Buffer exhaustion (`ENOSPC`) and validation rejection return to editing, with an
error notification for the UI. Other errors abort. Idle polls do not redraw.
Accept/cancel preserve the line and entry errno; failures retain the current
working line. Descriptors are copied and callback context is borrowed until the
call returns. Callbacks must not reenter the session or free its state; `update`
and `accept` must not mutate the editor. A failed callback without errno becomes
`EIO`; a result outside its contract becomes `EINVAL`.

For physical input, the event-source adapter can simply call the existing poller:

```c
static int next_key(void *context, struct hpclang_keyboard_event *event)
{
    (void)context;
    return hpclang_keyboard_event_poll(event);
}
```

Applications should provide `update` to display the working line, cursor and
errors, and optionally validate numeric input with the checked parsers. The
session does not reset keyboard events, select repeat timing, acquire a screen,
bind stdin, or invent Alpha/Shift mappings. The default editing policy retains
the previously documented basic keys. Full model-specific Alpha mapping and a
physical-key-driven rendering acceptance test remain open.

### Console and standard streams

The numeric-input, console and stream-binding extensions passed CI on `b92e2a7d`.

| API | Contract |
| --- | --- |
| `hpclang_console_create/destroy()` | Own a fixed-cell Unicode grid and drawing buffers; borrow the selected font; no display session or hardware acquisition |
| `hpclang_console_write/flush/clear()` | Incremental UTF-8, wrapping, scrolling, LF/CR/BS/TAB/FF; flush replaces an unfinished sequence; clear resets text and decoder |
| `hpclang_console_cell()` | Inspect a visible cell as a Unicode scalar; scrolled-away text is not retained |
| `hpclang_console_render()` | Opaque black-on-white viewport, each glyph clipped to its cell; clip to destination; caller chooses when to draw and present |
| `hpclang_stdio_bind/unbind()` | One explicit binding for the C standard streams; callbacks are copied and context is borrowed; file-backed streams and raw file descriptors are unaffected |

Each Unicode scalar occupies one cell using the font's nominal advance and line
height. This is not ANSI escape parsing, double-width/grapheme layout or an
unbounded transcript. Invalid UTF-8 becomes U+FFFD. A filled final column uses
delayed wrapping, so the last screenful remains visible until more output arrives.
`\b` moves left without erasing, tabs insert spaces to four-cell stops, and
unrecognized C0/C1 controls are ignored.

The output adapter below updates the text model; drawing/presentation remains
explicit. Dynamic console text needs a suitable font repertoire rather than
assuming `STATIC_AUTO` can discover future input.

```c
struct hpclang_console *console = hpclang_console_create(20, 8, hpclang_font_mini());
if (console != NULL) {
    struct hpclang_stdio_callbacks io = {
        .write = hpclang_console_stdio_write,
        .flush = hpclang_console_stdio_flush,
        .context = console,
    };
    if (hpclang_stdio_bind(&io) == 0) {
        printf("Value: %d\n", 12);
        fflush(stdout);
        hpclang_console_render(console, hpclang_display_surface(), 0, 0);
        hpclang_stdio_unbind();
    }
    hpclang_console_destroy(console);
}
```

An optional read callback supplies bytes to `getchar/fgets/scanf`. It may obtain
an edited line using the bounded editor and the application's event/render loop.
It returns `HPCLANG_STDIO_EOF` for normal end-of-input, or
`HPCLANG_STDIO_ERROR` with `errno` for a failure/cancellation (`ECANCELED`).
These map to distinct `feof`/`ferror` states. No implicit Alpha-lock or Stock
modal dialog is selected. Callbacks may use direct console/drawing operations,
but must not recursively call standard I/O or change its binding; recursion is
rejected with `EBUSY`.

Unbind before releasing the callback context or console. Unbind flushes both
output channels and detaches even if a flush fails, returning the first error.
Bind/unbind reset EOF, errors and pushback so old input cannot leak to a new
source. stdout and stderr have separate C stream status; the console adapters
deliberately combine their bytes into one ordered UTF-8 output stream.

## Display and surfaces

The canonical coordinate system has a top-left origin. Rectangle APIs use `x, y, width, height`; this intentionally differs from historical HPG inclusive `x1, y1, x2, y2` rectangle calls.

| API | Status | Contract |
| --- | --- | --- |
| `hpclang_display_surface()` | ✅ | Borrow Stock's mono display, or the back page during an acquired display session. Reacquire after presenting. |
| `hpclang_surface_width()` / `hpclang_surface_height()` | ✅ | Visible geometry. |
| `hpclang_surface_levels()` | ✅ | Reports 2, 4 or 16; Stock's default borrowed surface reports 2. |
| `hpclang_display_begin(levels)` | 🧪 | Acquire two owned DMA pages and a temporary 2/4/16-level LCD mode. Failure preserves Stock state. |
| `hpclang_display_present(timeout_us)` | 🧪 | Publish the cache, wait for a frame-counter rollover, then exchange front/back pages with LCD DMA stopped. |
| `hpclang_display_wait_frame(timeout_us)` | 🧪 | Bounded frame synchronization without changing the display. Timeout range: 1–1,000,000 µs. |
| `hpclang_display_indicator_set(segment, intensity)` | 🧪 | Set one of six off-screen scan segments in the back page. Present publishes it; printed icon meanings vary by model. |
| `hpclang_display_end()` | 🧪 | Restore original LCD configuration before releasing pages; idempotent and automatic on normal runtime shutdown. |
| `hpclang_surface_clear()` | ✅ | Clear with explicit 0–255 intensity. |
| `hpclang_surface_get_pixel()` / `hpclang_surface_set_pixel()` | ✅ | Clipped pixel read/write. |
| `hpclang_surface_draw_line()` | ✅ | Geometrically clipped line. |
| `hpclang_surface_draw_rect()` / `hpclang_surface_fill_rect()` | ✅ | Extent-based clipped rectangles. |
| `hpclang_surface_draw_circle()` / `hpclang_surface_fill_circle()` | ✅ | Integer-radius clipped circles. |
| `hpclang_surface_draw_polygon()` / `hpclang_surface_fill_polygon()` | ✅ | Explicit point arrays; closed outline and even-odd fill. |
| `hpclang_surface_blit()` | ✅ | Representation-independent clipped copy with same-surface overlap support. |
| `hpclang_surface_create()` / `hpclang_surface_create_mono()` / `hpclang_surface_destroy()` | ✅ | Opaque heap-owned 2/4/16-level software surfaces. Packing remains private. |
| `hpclang::surface_ref` | ✅ | Zero-owning C++ wrapper. |

### Physical display ownership

`HPCLANG_CAPABILITY_DISPLAY_MODES` and `HPCLANG_CAPABILITY_DISPLAY_INDICATORS`
report service availability; they do not promise enough contiguous free memory.
No framebuffer is reserved at startup. Each allocation is checked page by page
for physical continuity, alignment, address overflow and LCD bank constraints.
An allocation or mapping failure frees temporary allocations and leaves Stock's
LCD unchanged. Calls are single-threaded and must not run from interrupt handlers.

```c
if (hpclang_display_begin(4) == 0) {
    struct hpclang_surface *back = hpclang_display_surface();
    hpclang_surface_clear(back, 85);
    hpclang_display_indicator_set(5, 255);
    if (hpclang_display_present(200000) == 0) {
        back = hpclang_display_surface(); /* previous front page */
        /* Draw the next frame here. */
    }
    hpclang_display_end();
}
```

The borrowed page must not be destroyed or retained across `end`. After each
successful `present`, request the new back page; it contains the previous front
image. Neither page aliases Stock's saved framebuffer. Indicator writes change
display segments only, not native Alpha/Shift/battery state. Classic HPG numbers
0–5 mean remote, left shift, right shift, alpha, battery, wait; HP39 applications
must interpret their own panel's printed symbols.

The runtime cleans framebuffer cache lines before DMA reads, restores only
writable LCD configuration, and never forces an originally disabled LCD on.
`end` runs after normal `main`/destructor completion, and before `_exit` halts.
It does not recover from arbitrary CPU faults or power loss. Frame waiting reports
disabled output, timeout or a failed/stalled clock instead of spinning forever.
Flipping stops and restarts DMA to discard FIFO prefetch. Emulator scanout tests
prove pixel packing and state restoration; real-panel flicker, brightness and
electrical timing still require hardware measurement.

## Fonts and text

hpclang deliberately separates **calculator-native fonts** from **generated custom fonts**.

### Calculator-native fonts

The public core does **not** define a semantic font-role taxonomy. These accessors expose concrete calculator-native/ROM fonts by their historical names, plus one convenience default selection. Function names such as `mini`, `big`, and `editline` therefore identify a stock font source; they do not mean “small text”, “large text”, or “editor role” for application UI policy.

| API | Status | Contract |
| --- | --- | --- |
| `hpclang_font_default()` | ✅ | Borrow the calculator's normal default native selection. On HP39g+/39gs/40gs this is the stock 5×8 main raster; on HP48gII/49g+/50g it is the stock bigfont. This convenience alias is not a semantic role. |
| `hpclang_font_mini()` | ✅ | Borrow the stock minifont. HP39/40 uses its proportional five-row small font; HP48/49/50 uses the historical six-row minifont. |
| `hpclang_font_big()` | ✅ | Expose the historical HP48/49/50 bigfont only: eight raster rows with nominal advance 6. HP39-family calculators return `NULL` rather than aliasing the 5×8 main font. |
| `hpclang_font_editline()` | ✅ | Expose the dedicated HP39g+/39gs/40gs fixed 6×10 EditLine ROM font using each model’s own ROM. It is unavailable on HP48gII/49g+/50g. The HP39g+ extension awaits formal CI acceptance. |
| `hpclang_font_height()` | ✅ | Line height without exposing bitmap storage; `NULL` reports 0. |
| `hpclang_font_advance()` | ✅ | Nominal/fixed advance; `NULL` reports 0. |
| `hpclang_font_codepoint_advance()` | ✅ | Per-codepoint advance where the native/generated font is proportional. |

No HP glyph bitmap is linked into the application. HP48gII/49g+/50g minifont/bigfont objects are resolved through the live Saturn logical mapping supplied by the launcher. HPGCC3 documents bigfont as an 8-row raster with normal advance 6; its raw record is eight bytes per character, so “8×8” describes storage/raster capacity rather than a fixed 8-pixel text advance.

HP39gs/40gs fixed native rasters are physical stock-ROM assets. The launcher maps only physical NOR pages `0x0007e000` and `0x0007f000` as uncached 4 KiB pages in the bootstrap-private prefix of hpclang's own code coarse table. Runtime validates the private window through the tagged HP39 context carried by the opaque `platform_cookie`; it also accepts the legacy direct-base token. Applications must not decode either form. The established 5×8 main and proportional mini tables already live in this window.

Independent HP39 UI reverse engineering also establishes a third, dedicated EditLine table: fixed 6×10 cells, 20 Saturn nibbles per character, with the legacy lookup `0x07D3EA + 20 * characterCode`. The official 2.22 images place the same table at physical nibble `0x0FD882` on HP39gs and `0x0FDA35` on HP40gs. Both locations independently match the legacy table SHA-256 `f106efa599a5bb8a783f29cbc2dfa3afbace750975e783c223f53864a2945f53`, and Direct/Stock Gallery both pass a ROM-derived 6×10 pixel oracle. No glyph payload from that table is redistributed by hpclang.

The HP39 two-page window replaced an experimental long-lived high-L1 NOR section alias. Runtime diagnostics proved the alias descriptor words could themselves lie inside the active LCD framebuffer's physical interval and be overwritten by ordinary drawing. The fix is therefore an MMU-ownership correction below the font API, not an application ABI change.

HP39 mini is five bitmap rows with a per-glyph advance (nominal 4); ASCII lowercase follows the stock small-font case-folding behavior. HP39 main is the stock 5×8 raster with nominal advance 6. HP39 EditLine is a separate fixed 6×10 raster with advance 6. HP48/49/50 mini/big retain the historical 6/4 and 8/6 line-height/advance conventions.

HP39g+ D1.55 uses its own physical NOR pages `0x0006e000` and `0x0006f000` in the same private window. Its main, mini and EditLine tables begin at physical nibble addresses `0x0DC096`, `0x0DE0DA` and `0x0DCE2F`. The complete tables match the reviewed gs atlas; known ASCII shapes and the independent EditLine fingerprint guard their identity before pixel comparison. The old logical-address fallback is removed. See [validation](validation.md#hp39g-native-font-follow-up) for the current acceptance state.

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
| byte-stream files | ✅ HP49g+/HP50g | process-wide owned FatFs namespace feeds standard `open/close/read/write/lseek/unlink` and picolibc `fopen/fread/fwrite/fseek/remove`; HP39/40 and HP48 return clean `ENOSYS` |
| calculator object store | ✅ HP48/49/50 named RPL stores and HP39 native reads; M7.3f public writes accepted | owned typed/named snapshots, logical-directory put/remove, and native-variable get/set; HP39 native catalogs/contexts are not flattened into a fake HOME tree or `FILE *` |
| RPL/Saturn objects | ✅ HP48/49/50 owned snapshots + bounded stack mutation; ✅ HP39 private Stock portal/native readers | M7.3 typed object/stack bridge; bounded HPGCC compatibility adapters sit above it |

The byte-stream metadata layer is also public and typed rather than exposing KOS structures:

| API | Status | Contract |
| --- | --- | --- |
| `hpclang_stream_path_query()` | ✅ | Versioned FAT size/type/portable attributes and validity-gated modification, creation and stored access dates. `/` is the SD volume root; explicit paths never mutate Stock global cwd. |
| `HPCLANG_STREAM_PATH_LAST_MODIFIED_VALID`, `HPCLANG_STREAM_PATH_CREATED_VALID`, `HPCLANG_STREAM_PATH_LAST_ACCESSED_VALID` | ✅ | Each date is usable only with its corresponding validity bit. No timezone or epoch is invented. |
| `hpclang_stream_path_set_attributes(path, attributes, mask)` | ✅ Accepted on `6a7f9a28` | Updates only masked read-only/hidden/system/archive bits and synchronizes before success. Unknown bits or attributes outside the mask fail with `EINVAL`; other families return `ENOSYS`. Existing descriptors retain their access rights; new write opens respect read-only. |
| `hpclang_stream_storage_query()` | ✅; owned-media refresh accepted on `56e4a9dd` | Media-present/read-only flags plus free bytes and total capacity from FatFs. GPIO sampling continues during ownership without KOS re-entry. Observed removal or block-transfer failure invalidates old objects; new path operations can remount. Old descriptors/directories fail with `EBADF`; live write protection rejects writes with `EROFS`. Removal/reinsertion entirely between observations and physical hot-plug validation remain open. |
| `hpclang_stream_storage_free_bytes()` | ✅ | Convenience free-space query over the direct FAT volume. |
| `rename()` / `mkdir()` / `rmdir()` | ✅ | Implemented directly in the owned FatFs namespace; the broken/absent Stock KOS namespace primitives are not used. |

Path-info version 3 and directory-entry version 2 append `created` and
`last_accessed`; rebuild applications against the matching SDK headers/runtime.
Creation and modification are exported at two-second resolution. Access is the
stored FAT date with zero time fields: reads do not update it, and FatFs clears
it on writes. Missing or malformed dates are zero-filled with their validity
bit clear. These are queries, not timestamp setters or POSIX atime tracking.
Raw KOS handles/module IDs and implementation-specific FAT internals remain private.

The `fsystem.h` adapter implements `FSChAttr`, `FSGetCreatTime` and
`FSGetAccessDate`; `FSCreate` accepts read-only/hidden/system/archive flags.
Date getters fill local civil `struct tm` fields, or zero the output when
unavailable. `FSChAttr` replaces mutable flags while requiring directory/volume
type bits to remain unchanged. Entries retain their path independently of the
parent iterator. This metadata extension is formally accepted on `6a7f9a28`.

`HP39DIR.000` / `HP39DIR.CUR` describe the external HP39/40G typed transfer/disk protocol, not the calculator's internal object-store root. HP39 internal objects live behind native Stock catalogs/contexts. The private HP39 runtime uses the real Stock model rather than one generic HOME namespace: Aplets are native DORRP snapshots below `'apletdir`, Program/Note bodies use the current `PGMCXT` / `NOTESCXT` directories, and Aplet Variables use the current `ApletVars#` schema plus native recall/type/store descriptors. Descriptor-driven Variable recall now quotes the genuine extended variable ID and invokes Stock `@`; no generic HOME/current mapping or public HP39 catalog hierarchy is claimed.

## Aplet-owned data (M7.3h, in progress)

`<hpclang/aplet.h>` adds `hpclang_aplet_data_get/set/clear` for the current
HP39 Stock Aplet under the supported HPGCC39 template. Reads return owned
snapshots, writes copy their input, and clear restores the empty-list value.
The template saves on Aplet exit. Writes are limited to 7000 native nibbles;
other models, Direct execution and unsupported callers return `ENOSYS`.
HP39g+/39gs/40gs have local GC, instance-isolation and exit/relaunch evidence;
The packaged Aplet exposes Stock's selected-Aplet Reset confirmation. Local
HP39g+/HP39gs/HP40gs evidence covers cancellation, data reset, relaunch and isolation
from another saved instance. Formal M7.3h acceptance is complete on `6a7f9a28`.
See the [object-service contract](object-service.md#m73h-hp39-aplet-owned-data-in-progress).

`hpclang_aplet_list/clone/remove` manage the native HP39 Stock Aplet catalog.
List returns an owned list of display-name strings; names contain 1–254 raw
calculator bytes. Clone saves a named copy of an existing Aplet and rejects
existing destinations (`EEXIST`). Remove protects the active Aplet (`EBUSY`)
and native default Aplets (`EPERM`). Missing names return `ENOENT`; native
allocation failure returns `ENOSPC`. Large copies may exhaust native memory
while the ARM application is resident. All three HP39 models pass local catalog
checks; CI acceptance is complete on `6a7f9a28`. This does not construct arbitrary Aplet types.

`hpclang_device_capability()` separately reports `APLET_DATA`, `APLET_VARIABLES`,
`TEXT_CATALOG` and `APLET_CATALOG`. HP39 requires a live Stock portal; other
models report unsupported. Discovery does not reserve native memory or replace
per-call validation of the active Aplet template.

## Validation by service

| Service | Host | ARMv4T link | Direct runtime | Stock evidence |
| --- | --- | --- | --- | --- |
| HPC1/runtime | ✅ | ✅ | 5/5 PASS | Runtime #135 (`35298949365`) is fully green on the final M7.1 head, including settled HP48 return |
| device info | — | ✅ | 5/5 PASS | covered by common payload |
| monotonic time/delay | ✅ math | ✅ | 5/5 PASS | covered by common payload |
| keyboard events + identity | ✅ | ✅ | 5/5 full 51-key PASS | Direct and Stock both require the exact 52-frame progression; Runtime #135 is green |
| display/surfaces | ✅ | ✅ | 5/5 PASS | all stock paths have historical/current PASS evidence |
| native font/text | ✅ | ✅ | Gallery #112 (`35298949457`) covers all Direct families; HP39 mini/main/EditLine pixels are validated through the physical-ROM path | Stock hp39gs/hp40gs/hp48gII/hp50g all green with settled return |
| generated font selection | ✅ configure | ✅ | Gallery #112 exhaustively traverses all **245 generated faces** on all five automated targets | HP39gs/40gs/HP48gII/HP50g Stock generated controls green |
| C++23 + libc++ | ✅ | ✅ | 5/5 real-service PASS | same canonical HPC1 payload builds |
| hp4950 stream storage | — | ✅ | non-stream families reject unsupported stream/metadata/media access cleanly | ✅ Storage #53 (`35428824540`): unified FatFs stdio/metadata/capacity/namespace/directory semantics plus bounded HPGCC filesystem adapters; Direct hp39/hp48/hp4950 and Stock hp4950 all green |
| calculator objects / M7.3a-e | ✅ owned-object + adapter units | ✅ focused Core gates through #404 | ✅ Runtime #338 all Direct families | ✅ Runtime #338 all Stock families + clean return; bounded `satdir`/`saturn` parity baseline closed; M7.3f acceptance recorded below |

Formal `E2E / Font Gallery` run **35298949457 / #112** is fully green across Build, universal x50ng, all three Direct Gallery families, and Stock hp39gs/hp40gs/hp48gii/hp50g. HP39gs/40gs traverse **248 entries (245 generated + 3 concrete ROM fonts) × 2 pages = 496 captures**; HP48gII/49g+/50g remain **247 × 2 = 494** because they expose minifont + bigfont/default only. The HP39g+ follow-up adds the third native face, making its full Gallery contract 248 entries / 496 captures, and adds Stock USB Gallery to the matrix. This extension is formally accepted on `8f79c156`; the earlier #112 gate does not cover it.

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
| structured `fsystem.h` | — | — | 41 | 43 | bounded source adapter implemented over clean storage core; final Storage #53 green |
| KOS `kos.h` | 15 | 32 | 33 | 33 | raw KOS kept private |
| Saturn `saturn.h` | — | 50 | 58 | replaced | ✅ bounded application subset implemented and closed for M7.3e; raw-address/native-BCD functions remain Migration/Internal |
| Saturn directory `satdir.h` | — | 29 | 29 | replaced | ✅ high-level cwd/get/recall/search/REAL subset implemented; address-returning tree helpers remain Migration/Internal |
| conio | 27 | 27 | 26 | 49 in new `conio.h` | not implemented as a subsystem |
| raw `syscall.h` | 9 | 9 | 9 | 9 | intentionally not canonical |

HPGCC 1.1/2.0 also contain a substantial win widget/event family with 74 stable implementation names between those releases. HPGCC3 moves to a different GUI family. hpclang has not started a widget subsystem.

## HPG graphics: detailed progress

HPG is unusually stable: all 55 HPG 1.0.3 declarations survive exactly into 1.1, which adds four functions, and all 59 1.1 declarations remain signature-stable in 2.0 and HPGCC3.

| HPG group | Historical calls | hpclang status |
| --- | --- | --- |
| lifecycle | `hpg_init`, `hpg_cleanup` | ↪ Core surface access needs no matching global lifecycle; adapter can supply legacy setup semantics. |
| display mode | mono/gray4/gray16, `hpg_flip` | 🧪 Clean owned display sessions and bounded page presentation; historical spelling remains an adapter. |
| geometry | width/height, pixel, line, rect, circle, polygon | ✅ Canonical primitives implemented; legacy context/coordinate wrappers absent. |
| indicators | `hpg_set_indicator` | 🧪 Six scan-order status segments through the owned display service; printed icons are model-specific. |
| clipping state | `hpg_clip_*` | ◐ Intersecting mutable clips demonstrated over public primitives; installed HPG adapter remains M8. |
| fonts | `hpg_get_minifont`, `hpg_get_bigfont`, metrics, allocation/free | ◐ Native and generated hpclang font systems are implemented; HPG's exact bundled-font/custom-descriptor source semantics still need an adapter. |
| text | `hpg_draw_letter[_on]`, `hpg_draw_text[_on]` | ✅ Canonical explicit-surface UTF-8/codepoint drawing exists; mutable HPG context wrapper absent. |
| mutable style / PAINT/XOR / patterns | HPG context state | ◐ Executable prototype covers PAINT/XOR and transparent anchored/relative ink patterns; mutable state stays in the adapter. |
| owned images | mono/gray images | ◐ Generic 2/4/16-level software surfaces cover the clean ownership model. |
| blit | `hpg_blit` | ◐ Canonical blit implemented; HPG argument/context adapter pending. |
| XPM | mono/gray loaders | ◐ Transparent one-character RGB XPM prototype over public pixels; broad historical importer remains M8. |

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
| RTC wall clock | `sys_RTC_seconds`, `sys_tm_RTC` | 🧪 Civil read/set/conversion and computed weekday; historical source adapters remain pending. |
| clock speed | `sys_slowOn/off`, `sys_clksave/restore` | ✕ canonical app API unless a real generic policy use case appears. |
| interrupt/timer hardware | `sys_int*`, `sys_setupTimers` | ✕/↪ backend ownership detail. |
| LCD hardware state | `sys_lcd*`, `setDisplayPlane` | ↪ Owned display sessions restore LCD configuration and manage physical grayscale. |
| virtual/physical mapping | `sys_map_*` | ✕ canonical application API. |
| physical allocation/cache | `sys_phys_malloc`, `sys_flush_cache` | ✕ raw application API; display sessions own and publish validated DMA buffers internally. |
| tone/beep | `sys_playTone*`, `kos_beep` | 🧪 Clean PWM playback and cancellation; source adapters absent. |

## Files, stdio and storage

HPGCC's historical SD stdio was not universally available: the 1.1 Programming Guide documents the normal file path for HP49g+ rather than HP48gII/39g+.

hpclang now provides standard C stream semantics on the platform that actually has a suitable stock byte-stream filesystem:

| Capability | Status |
| --- | --- |
| libc formatting/string/memory | ✅ |
| heap-backed libc/C++ containers | ✅ |
| HP49g+/HP50g file open/read/write/seek/close | ✅ picolibc over process-wide FatFs |
| create/truncate/append/remove/ENOENT | ✅ |
| size/type/portable path attributes | ✅ versioned typed FAT query |
| media present/read-only + free bytes | ✅; KOS only samples presence/protect before takeover |
| last-modified timestamp | ✅ validity-gated FAT civil datetime; no timezone/epoch claim |
| attribute mutation and creation/access dates | Accepted on `6a7f9a28`. Masked FAT attributes, creation datetime and stored access date; no automatic atime tracking |
| total capacity | ✅ direct block-device sector count |
| rename/mkdir/rmdir | ✅ direct FatFs namespace |
| owned directory iteration | ✅ independent iterators, UTF-8 LFN, preserved stored case |
| unsupported stream access on HP39/40 and HP48 | ✅ clean `ENOSYS` |
| HPGCC2/3 `FS*` adapter | ✅ bounded useful subset over clean FatFs core |
| HPGCC `hpstdio.h` adapter | ✅ standard libc + thin `fdelete()` |
| HPGCC `fsystem.h` useful subset | ✅ implemented and exercised in Storage #53 |
| public raw KOS file API | ✕; raw KOS stays Migration/Internal, while real KOS remains host coordination only |

HP39 typed Aplet/Note/Program/etc. storage is deliberately a different M7.3-style object service rather than synthetic POSIX files.

## Saturn/RPL and calculator objects

HPGCC 1.1 exposes about fifty `saturn.h` functions plus 29 `satdir.h` functions. HPGCC 2.0 extends that surface; HPGCC3 replaces it with a larger `sat3_*` environment. hpclang keeps historical raw address/MMU idioms out of the canonical API and exposes owned objects plus capability-level stack/store operations.

| API / service | Status | Contract |
| --- | --- | --- |
| `struct hpclang_object` | ✅ opaque owned snapshot | ARM-side immutable copy of a proven-bounded native object; no Saturn pointer escapes the runtime |
| `hpclang_catalog_list/get/put/remove()` | M7.3j local implementation | HP39 Stock Program/Note text catalogs; owned names and body snapshots, create/replace/upsert modes, native compilation/editor record maintenance. Names are 1–19 calculator bytes; Program `Editline` is protected. These are distinct from Aplet Variables and HOME directories. |
| Port 0 / Port 1 / Port 2 | Reviewed limitation, 2026-10-04 | No public port read/write services. Main RAM, extended RAM and Flash library/backup namespaces retain separate follow-up goals; HOME CRUD does not imply port support. |
| `hpclang_object_kind()` / `release()` | ✅ | Stable coarse semantic class and explicit ARM-side lifetime |
| `hpclang_object_create_character()` | ✅ | Construct an owned calculator CHARACTER object without exposing its native prologue |
| `hpclang_object_create_string()` | ✅ | Construct an owned classic calculator DOCSTR from explicit-length raw calculator bytes; embedded zero bytes are preserved and no UTF-8 transcoding occurs |
| `hpclang_object_create_integer_i64()` | ✅ | Construct an exact classic ZINT snapshot for any `int64_t` value, including `INT64_MIN` |
| `hpclang_object_create_real_f64()` | ✅ classic DOREAL | Construct the nearest bounded 12-digit classic REAL from a finite `double`; NaN/Inf are rejected |
| `hpclang_object_create_complex_f64()` / `hpclang_object_complex_f64()` | M7.3i local implementation | Classic complex values contain two 12-digit REAL components. Construction rejects nonfinite inputs; conversion updates both outputs only on success. |
| `hpclang_object_create_array()` / `hpclang_object_array_info()` | M7.3i local implementation | Nonempty rank-1 vectors and rank-2 numeric matrices with homogeneous classic REAL or COMPLEX elements; shape must match the element count. Linked arrays and symbolic matrices are unsupported by these APIs. |
| `hpclang_object_array_get()` / `hpclang_object_array_replace()` | M7.3i local implementation | Zero-based row-major indexing; extraction and replacement return independent owned snapshots. Native decimal digits are copied without conversion through `double`; replacement leaves the input unchanged. |
| `hpclang_object_create_list()` / `list_size()` / `list_get()` / `list_splice()` | M7.3i local implementation | Owned heterogeneous lists, including empty lists. Splice removes a range and inserts copied elements into a new list. Extraction survives releasing the list. Native data references are materialized before Stock can move or remove their targets. |
| `hpclang_object_create_matrix()` / `matrix_size()` / `matrix_get()` / `matrix_replace()` | M7.3i local implementation | Nonempty rectangular native symbolic matrices, with heterogeneous cells. Row/column indices are zero-based; replacement returns a new object. Native exchange requires a model implementing DOMATRIX. |
| `hpclang_object_create_identifier()` / `identifier_info()` / `identifier_copy()` | M7.3i local implementation | Global or local identifiers with explicit-length calculator-byte names, 1–255 bytes. Construction does not define or resolve a variable. |
| `hpclang_object_create_tagged()` / `tag_size()` / `tag_copy()` / `tag_value()` | M7.3i local implementation | A copied label of 0–255 calculator bytes and an owned value. A tagged list retains independent children after native GC. |
| `hpclang_object_create_binary()` / `binary_size()` / `binary_copy()` | M7.3i local implementation | Native binary integers retain their exact width. Width is a positive multiple of four bits; bytes are least-significant first and an unused high half-byte must be zero. |
| `hpclang_object_create_integer_decimal()` / `integer_decimal_size()` / `integer_decimal_copy()` | M7.3i local implementation | Exact arbitrary-length classic ZINT text exchange, bounded by the native object-size limit and available memory. Optional input sign; canonical output without leading zeros or a plus sign. Length excludes a terminator. |
| `hpclang_object_create_real_decimal()` / `real_decimal()` | M7.3i local implementation | Exact classic REAL coefficient/exponent exchange: `(-1)^negative × significand × 10^exponent`. At most 12 significant digits; no rounding. This preserves values outside `double` range, without claiming EREAL/LONGREAL conversion or decimal arithmetic. |
| `hpclang_object_integer_i64()` | ✅ | Exact classic BINT/ZINT conversion; `ERANGE` if arbitrary-precision ZINT does not fit `int64_t` |
| `hpclang_object_real_f64()` | ✅ classic DOREAL | Explicit approximate conversion; overflow or nonzero underflow is `ERANGE`, not silent HPGCC-style clamping; EREAL/LONGREAL remain `ENOTSUP` |
| CHAR / DOCSTR conversion | ✅ | Raw calculator 8-bit code/bytes; no implicit UTF-8 reinterpretation |
| `hpclang_stack_depth/peek` | ✅ HP48/HP4950 Stock | 1-based user-visible stack after launcher-bias normalization; peek returns an owned snapshot |
| `hpclang_stack_push/pop/drop/replace` | ✅ HP48/HP4950 Stock | Bounded by launcher-derived native-page fences; pop snapshots before drop; replace copies before pointer commit |
| `hpclang_stack_swap()` / `hpclang_stack_nip()` | M7.3i local implementation | Require two visible stack entries. Swap exchanges them; nip removes level 2 while preserving level 1. Native references are reordered without allocation; expected validation failures occur before writes. HP39 and Direct return `ENOSYS`. |
| `hpclang_stack_push_boolean()` / `peek_boolean()` / `pop_boolean()` | M7.3i local implementation | Exchange actual SysRPL TRUE/FALSE identities on the classic Stock stack. Push accepts only 0 or 1. Numeric values are not coerced; failed peek/pop leave the output unchanged. No native object allocation. HP39 and Direct return `ENOSYS`. |
| private Saturn view | ✅ runtime-only | Explicit 20-bit nibble address + launch-preserved `SAT_CPU->Read[]`; arbitrary public poke is not exposed |
| `hpclang_object_store_home/current/open_child/get` | ✅ HP48/HP4950 Stock | Logical store handles re-resolve paths; named lookup returns owned snapshots and never changes Stock cwd |
| `hpclang_object_store_iterator_*` | ✅ HP48/HP4950 Stock | Independent ARM-side metadata snapshots; raw calculator-byte names are preserved |
| `hpclang_object_store_put/remove` | M7.3f accepted | Copy owned values into logical directories. Put supports upsert, create-only and replace-only; remove deletes a non-directory entry. Stock cwd and old references are preserved. |
| `hpclang_object_store_mkdir/rmdir` | ✅ M7.3g accepted | Classic Stock child creation, empty/explicit recursive removal, cwd/ancestor protection and retained value references. See [directory lifecycle](object-service.md#m73g-directory-lifecycle). |
| `hpclang_variable_get/set` | M7.3f accepted | Exact HP39 category/name lookup in the active Aplet schema; native `@`, descriptor type check and `STO`; owned snapshots and size-changing writes up to 7000 nibbles. |
| HP39 Aplet/Program/Note native reads | ✅ private runtime | Native DORRP / `PGMCXT` / `NOTESCXT` structures; no fabricated classic-RPL hierarchy |
| HP39 Aplet Variable value recall | ✅ public `hpclang_variable_get/set` on HP39 Stock | `ApletVars#` schema + genuine extended ID are resolved through the Stock descriptor contract by quoting the ID and invoking Stock `@`; Runtime #320 Stock/hp39 validates `NoteText` on both HP39gs and HP40gs with HPC1 PASS and the full 52-frame progression |
| HPGCC `<satdir.h>` high-level subset | ✅ bounded compatibility adapter | `sat_dir_open/close/curdir/chdir`, exact get, ancestor recall, recursive HOME search, owned search-result cleanup, REAL kind/query and get/recall REAL are implemented above owned store handles. `SAT_OBJ_DSCR.addr` is an opaque adapter token, not a Saturn address. |
| HPGCC `<saturn.h>` safe subset | ✅ bounded compatibility adapter; M7.3i additions accepted on `6a7f9a28` | Lifecycle, depth/drop/dropn, String push/pick/pop, historical textual int/double pushes, exact ZINT push/pop, classic REAL push/pop, CHAR push, and failure-safe `stdargs` are implemented above the clean object/stack service. M7.3i adds `sat_stack_swap/nip` and `sat_pushtruefalse` (`sat_stack_push_bool` alias). Historical raw-address/native-BCD signatures remain Migration/Internal. |

**Core #296 (`35439583956`)** remains the focused mutation-core gate, and
Runtime #320 Stock/hp39 remains the descriptor-driven `NoteText` recall gate.
M7.3e then progresses through Core #396/#399/#400/#402/#404 for the bounded
source adapters, exact ZINT/REAL constructors, representative HPGCC stack flow,
failure-safe `stdargs`, and focused calculator witness builds.

The final M7.3e read/adapter closure is **E2E / Runtime #338
(`35812005032`) on `ba2c0398`**: Build, all Direct families, Stock
HP39gs/HP40gs, Stock HP48gII, Stock HP49g+/HP50g, and evidence publication are
green. HP48 uses the small `ZZE2E` DOCSTR for successful owned named-object
snapshot validation rather than forcing a copy of the large launcher. The
focused Stock adapter witness restores incoming stack depth after ZINT, REAL
and String exchange and only consumes `stdargs` when the incoming stack is
already empty.

The parser remains intentionally fail-closed. Unknown or recognized-but-
unfrozen prologues are rejected rather than treated as arbitrary memory ranges.
Raw HPGCC `sat_peek/poke/map_s2a/memmove`, low-level address-returning
`satdir` operations, borrowed stack descriptors, raw native-BCD/list builders,
and native-prologue contracts remain Migration/Internal. General named-object
mutation is different: although HPGCC 1.1/2.0 read/search/recall adapter parity
does not require it, hpclang implements it as the clean-core **M7.3f**
write/delete slice.

**M7.3a–j are accepted for their implemented scope and approved limitations.**
Native port services and broader parity follow-ups remain in [Roadmap](roadmap.md).

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

Classifying a historical signature as Migration/Unsupported does not waive its useful application behavior. Any omitted capability requires an individually documented, maintainer-approved limitation.

Binary compatibility with old HPGCC executables is not a goal. HPGCC3 custom-ROM/ROMLIB ABI is not an hpclang target.

## Near-term API order

Based on historical stability, current implementation, and remaining dependencies:

1. ✅ owned 2/4/16-level surfaces, geometry and blit;
2. ✅ six-model physical keyboard identity/event core;
3. ✅ calculator-native font borrowing plus generated custom-font package with `STATIC_AUTO`, `STATIC_EXPLICIT`, and `FULL`;
4. ✅ hp4950 standard-C stream storage through picolibc;
5. ✅ exhaustive Direct Font Gallery plus HP50g and HP48gII compact Stock live-native controls; font implementation/evidence closure is complete;
6. ✅ M7.2d filesystem source adapters over the now-owned HP49g+/HP50g FatFs core;
7. M7.3a–e read/stack/adapters are accepted by historical Runtime #338; M7.3f mutation is also accepted. Complete M7.3g–j directory, persistence, composite and namespace capabilities next;
8. only after the corresponding clean services are stable, begin broad HPG/keyboard/system compatibility headers and the renderer-agnostic declarative UI layer.

This preserves the dependency direction: historical HPGCC APIs are implemented on top of a coherent hpclang core, not used to define that core.

### Keyboard action boundary

The public keyboard service stops at physical key identity plus modifier state. Model/user-interface actions such as HP39 `Shift+VARS -> CHARS` are not folded back into `enum hpclang_key`. Shortcut-to-command mappings belong to a higher application or compatibility layer.

### Current bitmap-only font closure status (2026-09-14)

Canonical bundled assets are bitmap-source only. The generated catalog is **245 faces / 1,639,485 mapped glyphs / 3,472 assembly sources** with **245/245** source→generated parity. Font Assets **#24** is green and regeneration produces no drift; the general TTF importer has been removed.

Final bitmap curation adds Cubic 11, ChillBitmap 7px, and QuanPixel/May7 with source provenance retained. Ark Pixel labels/specimens now expose useful locale differences, and Gallery text covers width-sensitive Latin, punctuation/digits, accents/symbols, and supported CJK/Japanese/Korean repertoires.

`E2E / Font Gallery` **35298949457 / #112** is fully green. HP39gs/40gs Direct traverse **248 entries / 496 captures** including the dedicated EditLine ROM font; HP48gII/49g+/50g remain **247 / 494**. Stock hp39gs/hp40gs additionally validate mini/main/EditLine/generated-control captures and the independent ROM fingerprint/pixel oracle, while HP48gII and HP50g retain their settled live-Saturn native controls.

The font slice therefore has no remaining API or evidence blocker. Repository integration and bounded Stock-harness closeout remain outside the font-core closure.
