# Migrate an HPGCC application

Start from the installed `examples/hpgcc-timer` for a C application or
`examples/object-cpp` for C++ ownership patterns. Follow
[Getting started](getting-started.md) to configure the relocated SDK and build
the launcher. Keep your application's source and assets; replace its historical
startup objects, linker script and precompiled HPGCC libraries with the SDK build.
Do not put historical include directories before the installed headers.

## Choose the boundary for each old call

| Existing code | Migration action |
| --- | --- |
| `#include <hpgcc49.h>` | Replace the umbrella with the individual installed compatibility headers your source uses. The umbrella is not installed. |
| `hpgraphics.h`, `hpkeyb49.h`, `hpstdio.h`, `hpconio.h`, `hpstring.h`, `hpsys.h`, `hptime.h`, `saturn.h`, `satdir.h`, `fsystem.h` | Keep calls declared by the installed header; each header implements a subset. An undeclared call needs review, not a copied old declaration. |
| Application definition of `int errno` | Remove the definition and include `<errno.h>`. Check each operation's documented result before reading `errno`. |
| KOS filesystem handles or raw SD/controller calls | Move file operations to standard C streams and the services in `hpclang/storage.h`. Preserve operation failures and removable-media handling. |
| Raw Saturn addresses, `sat_peek`-style traversal or retained pointers into native objects | Use independent snapshots from `hpclang/object.h` and logical handles from `hpclang/object-store.h`; use `hpclang/port.h` for native port objects. |
| Direct LCD addresses, packed framebuffer writes, MMU changes or interrupt handlers | Express drawing through `hpclang/display.h` surfaces and fonts, and timing through `hpclang/time.h` / `timer.h`. A raw hardware algorithm requires a separate design review. |
| `win*` widgets | Port the view and event logic to the [declarative UI](declarative-ui.md); a compatible historical widget family is not provided. |
| `kos_beep` and completion polling | `kos.h` provides the sound subset with old-request replacement and waiting flags; locally verified on all six Direct models, Stock/CI acceptance pending. A true completion query can still mean a nonwaiting tone is playing. See the [mode contract](../development/hpgcc-compat.md). |

Compile with the normal SDK target options and resolve diagnostics one family
at a time. Do not suppress undeclared functions or cast away mismatched pointer
types to get a link: historical structure layouts and raw address lifetimes are
not an ABI contract. The headers are the authoritative list of supported calls.

## Preserve numbers and text deliberately

`double` exchange is approximate. For exact classic calculator REAL values, use
`hpclang_decimal_real` with `hpclang_object_create_real_decimal` and
`hpclang_object_real_decimal`; this preserves the classic format's twelve-digit
coefficient and exponent. It does not implement arbitrary-precision arithmetic.
For an integer outside `int64_t`, use the decimal-text integer creation/copy
functions in `hpclang/object.h`. Keep this exact representation through editing
and storage if rounding is unacceptable. Test negative, zero, boundary and
out-of-range values instead of checking only small positive examples.

Standard C formatting follows the SDK's C library. Historical numeric-format
settings are not implicitly supplied by including `hpconio.h`. Set the format
explicitly and verify rounding and displayed output for the application.

Legacy HPG text and the default compatibility console use calculator bytes.
The clean `hpclang_surface_draw_text` and `hpclang_surface_draw_utf8` functions
consume UTF-8. Object strings and directory names remain explicit raw bytes;
copying their bytes into a UTF-8 view does not transcode them. Choose the encoding
at the boundary and retain byte lengths, including any embedded zero bytes.

For clean text rendering, select a font with the SDK's `hpclang_target_font`
helper. Include dynamic characters explicitly: source-string extraction cannot
predict text loaded from a file or constructed at runtime. The timer example
packages printable ASCII for its formatted result. Check both missing-glyph
behavior and the intended calculator's display size. Legacy default fonts can
use native or packaged fallback faces, so pixel-identical glyphs are not promised.

## Make ownership visible

`ints`, `uints` and `sys_bcd2str` return borrowed formatting buffers. Copy a result
before another call that reuses its buffer; do not free it. A native object
snapshot, by contrast, is independently owned and must be released with
`hpclang_object_release`. Closing a directory handle does not release separately
owned snapshots or metadata iterators.

In C++, use `owned_object` from `hpclang/object.hpp` and the directory/iterator
owners from `hpclang/object-store.hpp`. Move to transfer ownership; borrow with
`get()`. Use a temporary owner and swap after a successful replacement, or
`owned_object::receive` for compatible C producer calls, so a failed operation
does not discard the old value. These wrappers work in the SDK's no-exceptions,
no-RTTI profile.

For byte-stream files and directories, `hpclang/storage.hpp` provides C++17
`owned_stream` and `owned_stream_directory`. Adopt an independently opened
`FILE*` or `hpclang_stream_directory*`, use `get()` with the existing C I/O
functions, and move or `release()` to transfer close responsibility. Do not
adopt standard streams or a borrowed handle. The wrappers add no allocation
and preserve the C backend's model, media and encoding restrictions.

Call and check `close()` before reporting a write as saved: it returns the
underlying close result and empties the owner even on failure. Empty `close()`
is an idempotent success. Destruction, `reset()` and move assignment provide
best-effort cleanup and preserve `errno`, so they deliberately cannot report a
close error. Explicitly close a writable stream before replacing its owner.
To keep an existing handle on open failure, first adopt the new result in a
temporary owner and replace only after checking it.

Returning normally destroys automatic C++ owners. Calling `exit()` does not
unwind those scopes; release local resources explicitly before that path.
`_Exit()` skips language callbacks. SDK-owned hardware restoration does not
substitute for saving application data or running application cleanup.

The legacy `gets` / `_gets` aliases accept fixed character arrays with a known
capacity. For a pointer or variable-length buffer, call
`hpclang_hpgcc_gets(buffer, capacity)` explicitly; capacity includes the final
NUL. Check cancellation/error and partial text. Do not replace the bounded
editor with an unbounded read to preserve a historical spelling.

## Validate the actual launch context

Use `hpclang_device_capability` from `hpclang/device.h` to distinguish supported,
unsupported and context-unavailable services. A supported capability still
does not promise available media, free memory or a successful next operation.
Check the service call as well. HP39 Aplet services, classic RPL stores and SD
streams have different model/context requirements; do not infer availability
from the compatibility header's presence.

`sys_playTone2(hz, ms)` uses milliseconds; `sys_playTone(hz, cs)` uses hundredths
of a second. Both block and restore borrowed sound hardware. `sys_slowOn` and
`sys_slowOff` select economy and original host clock profiles; they are not
nested scopes. Report failures rather than retrying through old raw registers.

Validate a complete scenario on each intended model: input and cancellation,
displayed text, storage across relaunch, allocation/service failure and clean
return to the calculator OS. Inspect the launcher's `memory.json` alongside
runtime observations; serialized file size and reserved RAM measure different
things. A passing build or host test alone does not establish calculator behavior.

Port 2 automatic startup recovery and space reclamation remain M8 work. Existing
append/replace/delete and explicit recovery paths do not establish those future
guarantees. Keep full-media and interrupted-write behavior in the application's
acceptance plan when it relies on native Flash persistence.
