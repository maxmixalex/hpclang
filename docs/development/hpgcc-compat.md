# HPGCC compatibility inventory

This page records the M7.0 source-compatibility inventory. It is intentionally
separate from the hpclang core API design: historical names are inputs to the
compatibility layer, not automatically names for the new core API.

## Sources and method

The HPGCC 1.1 and 2.0 comparison is based on the official historical source
trees previously inventoried from the SourceForge tags
`frozen_1.1_MMU_trunk` and `frozen_2.0_VC_release`, cross-checked against
the complete HPGCC 2.0 SP2 archive listing preserved by hpcalc.org.

The 2.0 SP2 archive includes source and installed public headers such as
`hpgraphics.h`, `hpscreen.h`, `hpkeyb49.h`, `hptime.h`, `hpsys.h`,
`kos.h`, `saturn.h`, `satdir.h`, `fsystem.h`, `ggl.h`, and the
`win*.h` widget headers.

HPGCC3 is inventoried separately from its published Doxygen documentation. It
is a design reference, not an ABI compatibility target.

Primary references:

- SourceForge HP-GCC releases:
  <https://sourceforge.net/projects/hpgcc/files/hpgcc/>
- hpcalc HPGCC 2.0 SP2 archive:
  <https://www.hpcalc.org/details/6384>
- HPGCC3 filesystem documentation:
  <https://hpgcc3.org/documentation/html/fsystem_8h.html>
- HPGCC3 GGL documentation:
  <https://hpgcc3.org/documentation/html/ggl_8h.html>
- HPGCC3 Saturn documentation:
  <https://hpgcc3.org/documentation/html/group__grconst.html>

## Stability between HPGCC 1.1 and 2.0

A filename-level API implementation comparison gives the following picture.
Private helpers beginning with an underscore are excluded.

| Area | HPGCC 1.1 | HPGCC 2.0 | Stable public implementation names |
| --- | ---: | ---: | ---: |
| HPG/GGL graphics | 106 | 105 | about 105 |
| Keyboard | 18 | 18 | 18 |
| Legacy screen | 15 | 15 | 15 |
| System/time/libc support | 50 | 50 | 49 |
| File I/O | 17 | 11 | 11 |
| Saturn/object bridge | 71 | 73 | 70 |
| KOS/stock OS helpers | 37 | 28 | 28 |
| win widget/event library | 74 | 74 | 74 |

This matters for hpclang: the user-facing portions of HPGCC 1.1 and 2.0 are
mostly stable even though the calculator launch/runtime implementation changed
substantially between those releases.

The compatibility layer can therefore target a meaningful common HPGCC source
surface instead of maintaining one wrapper set per historical release.

## Keyboard

The following 18 implementation names are present in both 1.1 and 2.0:

- `keyb_getkey`
- `keyb_getkeyM`
- `keyb_getmatrix`
- `keyb_getnextkey`
- `keyb_isAlpha`
- `keyb_isAnyKeyPressed`
- `keyb_isDown`
- `keyb_isKeyPrM`
- `keyb_isKeyPressed`
- `keyb_isLS`
- `keyb_isLeft`
- `keyb_isON`
- `keyb_isRS`
- `keyb_isRight`
- `keyb_isShiftKeyPrM`
- `keyb_isUp`
- `keyb_read_keymap`
- `keyb_waitKeyPressed`

### Compatibility decision

**Adapter**, with selected functions likely becoming effectively Direct.

The hpclang core should distinguish:

1. raw physical key state / matrix scanning;
2. translated key events;
3. blocking wait/repeat policy.

Historical HPGCC mixes these layers. Compatibility wrappers can reproduce the
common old calls, but the core API should not.

Likely first compatibility targets after the core keyboard service exists:

- `keyb_isKeyPressed`
- `keyb_isAnyKeyPressed`
- `keyb_getmatrix`
- `keyb_getkey`
- `keyb_waitKeyPressed`

## Graphics: HPG and GGL

HPG and GGL are the largest historical application-facing subsystem. The
common 1.1/2.0 implementation set includes operations such as:

- initialization and mode selection:
  `hpg_init`, `hpg_set_mode`, `hpg_set_mode_mono`,
  `hpg_set_mode_gray4`, `hpg_set_mode_gray16`;
- geometry:
  `hpg_draw_pixel`, `hpg_draw_line`, `hpg_draw_rect`,
  `hpg_fill_rect`, `hpg_draw_circle`, `hpg_fill_circle`,
  `hpg_draw_polygon`, `hpg_fill_polygon`;
- text/fonts:
  `hpg_draw_text`, `hpg_draw_letter`, `hpg_set_font`,
  `hpg_get_font`, `hpg_get_bigfont`, `hpg_get_minifont`;
- surfaces/images:
  `hpg_alloc_mono_image`, `hpg_alloc_gray4_image`,
  `hpg_alloc_gray16_image`, `hpg_free_image`, `hpg_blit`;
- clipping/state:
  `hpg_clip`, `hpg_clip_set`, `hpg_clip_reset`,
  `hpg_set_color`, `hpg_set_pattern`, `hpg_flip`;
- low-level GGL:
  `ggl_initscr`, `ggl_bitblt`, `ggl_hline`, `ggl_vline`,
  `ggl_rect`, `ggl_scrollup`, `ggl_scrolldn`, and related operations.

### Compatibility decision

**Adapter** for HPG. **Migration/adapter-later** for raw GGL.

The new core should use an explicit surface/display model with queryable
geometry and pixel capabilities. HPG compatibility wrappers can map common
draw operations onto that model.

GGL exposes assumptions and packed-pixel details that are valuable for fast
legacy programs but should not define the cross-platform core. HPGCC3's own
GGL documentation still exposes fixed LCD constants for its target environment,
which reinforces that it is a target-specific graphics layer rather than a
portable API contract.

## Legacy screen API

The 15 implementation names are unchanged between 1.1 and 2.0:

- `LCDOff`, `LCDOn`
- `clearScreen`
- `drawBlockOR`, `drawBlockOR4bit`
- `drawBlockXOR`, `drawBlockXOR4bitC`
- `fillTiles`
- `getBlock4bit`
- `getLCDPointer`
- `getMF`
- `invertScreen`
- `print`
- `setDisplayPlane`
- `testBlock4bitC`

### Compatibility decision

Mixed **Direct/Adapter/Migration**.

Simple semantic operations such as `clearScreen` and `invertScreen` can be
compatibility wrappers.

Raw LCD pointer, memory-format, and display-plane functions must not define the
core display ABI. They depend on physical framebuffer representation and may
be Migration-only or exposed through explicitly unsafe/low-level platform
interfaces where genuinely useful.

## Time and system services

The historical sys layer combines several different concerns:

- delays and timers:
  `sys_waitTicks`, `sys_waitRTCTicks`, `sys_RTC_seconds`,
  `sys_delta_seconds`, `sys_setupTimers`, `sys_restoreTimers`,
  `sys_setRTCTickPeriod`, `sys_updateTimer`;
- LCD controller state:
  `sys_lcdsave`, `sys_lcdrestore`, `sys_lcdsetmode`,
  `sys_LCDSynch`, `sys_lcdgetheight`;
- power/clock:
  `sys_sleep`, `sys_slowOn`, `sys_slowOff`;
- privileged/memory operations:
  `sys_intOn`, `sys_intOff`, `sys_map`, `sys_phys_malloc`;
- libc-like functions historically supplied by HPGCC itself:
  `malloc`, `calloc`, `realloc`, `memcpy`, `qsort`, and others.

### Compatibility decision

Split by responsibility.

- Time/delay: **Adapter** to hpclang monotonic time and deadline APIs.
- LCD mode/controller: **Migration**, except safe high-level behavior exposed
  by the display API.
- Interrupt/MMU/physical allocation: **Unsupported as normal application API**
  or explicitly internal/unsafe.
- Standard libc functions: **Direct through picolibc**, not hpclang wrappers.

Do not reintroduce a parallel `hpclang_malloc`, `hpclang_memcpy`, or
`hpclang_qsort` API.

## File I/O and standard C I/O

Both 1.1 and 2.0 provide the familiar implementation names:

- `fopen`
- `fclose`
- `fread`
- `fwrite`
- `fseek`
- `feof`
- `fgetc`
- `fgets`
- `fputc`
- `fputs`
- `fdelete`

The extra 1.1 files are duplicated lower KOS wrappers rather than a separate
user-facing stdio design.

### Compatibility decision

The standard C names are **Direct**, once hpclang provides the underlying
storage hooks required by picolibc.

`fdelete` is an **Adapter/Migration** candidate to standard `remove()`.

The core storage service should expose calculator-specific volume/object-store
facts needed to implement standard I/O rather than replace standard I/O.

HPGCC3's `fsystem.h` is useful design evidence for volumes, directories,
file attributes, path handling, and explicit errors. Its `FS_FILE` ABI and
`FS*` names are not hpclang compatibility targets.

## Saturn / RPL object bridge

The common 1.1/2.0 surface is broad. Representative groups include:

- raw object/memory:
  `sat_peek`, `sat_poke`, `sat_skipob`, `sat_check_type`;
- stack operations:
  `sat_stack`, `sat_stack_drop`, `sat_stack_swap`,
  `sat_stack_nip`, `sat_stack_list`;
- push/pop conversions:
  `sat_push_real`, `sat_pop_real`, `sat_push_string`,
  `sat_pop_str`, `sat_push_zint_llong`, `sat_pop_zint_llong`;
- object conversion:
  `sat_real2double`, `sat_double2real`, `sat_decode_real`;
- directories/variables:
  `sat_dir_open`, `sat_dir_close`, `sat_dir_getobject`,
  `sat_dir_rclobject`, `sat_dir_chdir`, `sat_dir_getcwd`;
- object construction:
  `sat_createtemp`, `sat_push_bint`, `sat_push_char`,
  `sat_push_comp`.

2.0 adds a few helpers such as `sat_memmove`,
`sat_shrinklastob`, and `sat_stack_pop_stdargs`; 1.1 contains
`sat_moveup` instead.

### Compatibility decision

**Adapter**, deferred until the object-lifetime model is designed.

This subsystem is too important to model as raw pointers. The hpclang core
should use explicit Saturn address/object types or opaque handles, with clear
copy/borrow/ownership semantics.

Compatibility wrappers can later map the stable and widely used `sat_*`
operations onto that core.

HPGCC3's Saturn API is particularly useful as a design reference because it
introduces an explicit `SAT_OBJECT` type and allocation/object descriptors
rather than treating every calculator object as an ordinary C pointer.

## KOS and direct stock-OS calls

The common KOS implementation includes disk enumeration, RTC access, LCD
contrast, serial number, reset/beep, and direct stock-OS file/directory
operations.

### Compatibility decision

Mostly **Migration/Internal**.

The new core should expose portable capabilities such as time, storage,
display state, and device information. It should not make historical KOS
entry numbers or raw stock-OS call conventions part of the hpclang public ABI.

A low-level stock-OS escape hatch can be considered later if real software
requires operations that cannot be represented cleanly.

## win widget/event library

The 74 implementation names are stable between 1.1 and 2.0. The library
contains:

- widgets and widget geometry/state;
- boxes/layout;
- text;
- images;
- progress controls;
- checkboxes;
- choice controls;
- soft menus;
- event registration/posting;
- focus traversal;
- timer events;
- repaint/event loop management.

### Compatibility decision

**Migration initially; compatibility layer later if justified by real ports.**

Reimplementing the historical widget library before the display, keyboard, and
time primitives exist would invert the dependency order.

M7 should first establish the lower service layer. A later UI toolkit may
either provide win compatibility or offer a smaller modern API with adapters
for common historical widgets.

## HPGCC3 design input

HPGCC3 is not a compatibility baseline, but three design choices are useful:

1. **Filesystem:** explicit file objects, volumes, directories, attributes,
   path rules, and error codes.
2. **Graphics:** surfaces and explicit drawing targets are preferable to
   assuming every draw call targets one global LCD pointer.
3. **Saturn objects:** an explicit object descriptor type is safer than
   exposing Saturn addresses as ordinary application pointers.

These ideas can inform hpclang while avoiding HPGCC3's ROMLIB/custom-ROM ABI.

## Initial compatibility matrix

| Historical area | Core hpclang direction | Compatibility class | M7 priority |
| --- | --- | --- | --- |
| standard libc/string/math | picolibc/libm | Direct | already/core |
| keyboard `keyb_*` | raw state + event service | Adapter | **M7.1** |
| HPG graphics `hpg_*` | display/surface service | Adapter | **M7.1** |
| legacy screen helpers | display service | mixed | M7.1/later |
| time `sys_*Ticks*` | monotonic time/deadlines | Adapter | **M7.1** |
| stdio file functions | picolibc backed by storage service | Direct | M7.2 |
| HPGCC3 `FS*` | design reference only | Migration | M7.2 |
| Saturn/RPL `sat_*` | typed object/stack bridge | Adapter | M7.3 |
| KOS/syscall internals | platform/private service | Migration/Unsupported | later |
| `win*` widgets | future UI toolkit | Migration, adapter later | M7.4 |
| raw MMU/interrupt APIs | internal only | Unsupported normal API | none |

## M7.1 design consequence

The first vertical slice remains:

```text
capabilities
+ monotonic time
+ keyboard
+ display/surface
```

but this inventory narrows the design target:

- keyboard must be able to support the five most common `keyb_*` wrappers
  without forcing their semantics into the core;
- display must support HPG-style pixel/line/rect/text compatibility without
  fixing the core to one framebuffer format;
- time must be monotonic and unit-explicit while allowing historical
  tick-based wrappers to be implemented;
- capability queries must expose real geometry/model differences required by
  those services.

No M7.1 public function names should be frozen until those semantics are
specified in the SDK design document.
