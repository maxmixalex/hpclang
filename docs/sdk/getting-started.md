# Build an application with the installed SDK

The initial M8 SDK is a relocatable source package. It includes public headers,
runtime sources, generated fonts, CMake integration and launcher packaging tools. The
runtime library is compiled once in each application build with the selected
toolchain. Calculator packaging uses explicit external ARMToolbox dependencies;
the SDK does not redistribute those historical libraries or firmware.

## Requirements

- CMake 3.20 or later and a supported build tool, such as Make or Ninja.
- Arm Toolchain for Embedded (ATfE); the verified version is 22.1.0.
- Python 3 for the HPC1 packer; no third-party Python modules are required.

Use the ATfE compiler and its matching libraries. A generic desktop Clang alone
does not supply the required embedded C/C++ runtime. Add the ATfE `bin` directory
to `PATH` before configuring an application. A virtual environment can provide
Python without changing the system installation.

## Install

From an hpclang checkout:

```sh
git lfs pull
cmake -S . -B build/sdk-install
cmake --install build/sdk-install --prefix /path/to/sdk
```

Installation itself needs no compiler. The package is under
`/path/to/sdk/share/hpclang` and can be moved together with its prefix.
The font source bundle and its upstream notices accompany the generated assets;
installation rejects an unresolved Git LFS pointer. Application builds do not
parse that archive. Font redistribution remains subject to the selected face's
terms; see the installed `docs/licensing.md` and `runtime/fonts/fonts.zip`.

## Build the C and C++ examples

Copy `share/hpclang/examples/hello` from the installation to a separate
application directory. Configure using the installed toolchain and package:

```sh
cmake -S hello -B build/hello \
  -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/sdk/share/hpclang/cmake/toolchains/hpclang-armv4t.cmake \
  -DCMAKE_PREFIX_PATH=/path/to/sdk
cmake --build build/hello
```

This produces `hello_c.elf`, `hello_c.hpc`, `hello_cpp.elf` and `hello_cpp.hpc`,
plus `hello_c-launchers/` and `hello_cpp-launchers/` with all six model launchers.
The HPC1 payload is model-independent; it is not a calculator transfer file.
Each launcher directory contains raw `.bin` images and wrapped `.hp` objects.
Launcher packaging compresses the embedded application when this reduces its
size, then the bootstrap expands it directly into the application's code pages.
This reduces the memory occupied by the installed object while the app runs;
the public `.hpc` file stays HPC1. `MinSizeRel` uses Clang's `-Oz` for C/C++.
Compression does not remove the need for enough free calculator RAM for code,
data, the application heap and stack.

For a memory-constrained calculator, configure a separate build directory with
`-DHPCLANG_BUILD_MODEL=hp39gplus` (or `hp39gs`, `hp40gs`, `hp48gii`, `hp49gplus`,
`hp50g`). This enables whole-program optimization with a fixed device model,
allowing the linker to remove unreachable services while keeping the same source
and public APIs. The default is `universal`. Model-specific builds package only
the selected model's launcher; requesting another launcher is a configuration
error. Runtime startup rejects a mismatched calculator before constructors or
application code run. Use separate build directories for different models.

HP48gII/HP49g+/HP50g use the wrapped object with the corresponding installed
ARMToolbox library. HP39g+/HP39gs/HP40gs need an additional Aplet envelope;
the `.hp` file is an intermediate input, not a directly installable Aplet.
`hpclang_target_aplet` creates that envelope when supplied with the prepared
historical host tool and assets described below. The small examples
draw a greeting and return immediately; they are build examples, not interactive
applications or calculator execution acceptance tests.

The installed `examples/hpgcc-timer` directory demonstrates the supported
historical `hpsys.h` timing calls with an ordinary external CMake application.
It measures a short delay, draws the duration and returns. Its README explains
the units and the boundary between historical timer calls and hpclang rendering;
it does not imply complete HPGCC compatibility.

`examples/hpgcc-window` uses `winevent.h` and `wintimer.h` to repaint a moving
rectangle through HPG, then quit after twenty 50 ms timer callbacks. Check
`hpclang_win_error()` after the loop: historical void calls latch failures.
Callbacks borrow their application data through synchronous dispatch. The
installed-SDK gate also builds this source as C++23 and packages all three HP39
Aplet variants. `winkeys.h` also supplies typed-key/modifier events, while
`win.h` includes the persistent control adapters. This small example exercises
events, keys and timers; see the [migration guide](migration.md) for control
ownership and adapting historical applications to 64/80-line screens.

`examples/hpgcc-console` combines C `printf`/`puts`, C++ `std::printf` and HPG
graphics, edits a name in a 16-byte buffer, reads a count and a value, prints
their product, then waits for a key before returning.
Including `hpconio.h` enables
lazy calculator-byte output without replacing standard C/C++ function names.
The same header can appear in multiple translation units. Explicit
`hpclang_stdio_bind` callbacks take priority over the historical default.
The bounded editor takes an explicit capacity and supports backspace and ON
cancellation. `hpstdio.h` provides `readint/readdouble` and the unsigned,
hexadecimal and 64-bit integer variants. These use a 31-byte edited line and
report conversion/input errors through `errno` and stream flags. They accept
numeric prefixes; use the clean text-input parsers for whole-line validation.
Historical `gets/_gets` call aliases accept fixed `char` arrays in C and C++;
use `hpclang_hpgcc_gets(buffer, capacity)` for pointers or dynamic buffers.
Legacy decimal formatting is explicitly selected per application translation
unit with `HPCLANG_HPGCC_LEGACY_FORMAT=1` before the compatibility include.
Omitted/zero preserves standard formatting. See the [migration guide](migration.md)
for the distinct legacy grammar, shared settings and bounded output entry points.
The formatting addition is locally validated; remote acceptance remains pending.

An application CMake file uses:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_application LANGUAGES C CXX ASM)
find_package(hpclang CONFIG REQUIRED)
hpclang_add_application(my_application main.cpp)
target_compile_features(my_application PRIVATE cxx_std_23)
hpclang_target_launchers(my_application MODELS hp49gplus hp50g)
```

`hpclang_add_application` supplies the startup, runtime library, linker script
and HPC1 packaging step. `hpclang_target_font` selects packaged font subsets;
the installed examples demonstrate `STATIC_AUTO` with `HPCLANG_UI_TEXT`.
The C++ profile has no exceptions or RTTI. Declare external dependencies in the
application rather than referring back to an hpclang checkout.

`hpclang_target_launchers` is optional. Omitting `MODELS` builds all six models;
an explicit list limits the output. It uses the selected compiler's matching
LLVM linker and binary tools and shares its bootstrap builder with repository
E2E. Changing the payload or bootstrap sources, or removing an output file,
causes CMake to regenerate the launchers.

For a reproducible application memory ceiling, use
`hpclang_target_launchers(my_application MAX_RW_BYTES 36864)` (36 KiB).
The command-line builder exposes the same option as `--max-rw-bytes 36864`.
The ceiling must be a positive multiple of 4096, at most 1 MiB, and must fit
the boot page, page-rounded static data and the reserved 8 KiB stack. A launcher
with fewer available pages still exposes only those available pages. This caps
the application's mapped read/write arena and moves its stack top accordingly;
it does not reduce the legacy launcher's Stock reservation or guarantee enough
heap for a particular application. Omitting the option keeps all available pages.

Use `MIN_RW_BYTES` when the application needs a known minimum rather than just
a ceiling. For example,
`hpclang_target_launchers(my_application MIN_RW_BYTES 36864 MAX_RW_BYTES 36864)`
requires a complete 36 KiB arena or rejects the launch. The command-line
equivalent is `--min-rw-bytes 36864 --max-rw-bytes 36864`. Both sizes must be
page-aligned and contain the boot page, static data and reserved stack; the
minimum cannot exceed the maximum. An impossible total page-pool requirement
is rejected during packaging. With a minimum configured, stage0 checks the
complete code/RW/workspace requirement before copying startup code or changing
the mappings or stack. Stage1 rechecks the RW requirement before app entry.
The option establishes an admission condition; it does not ask Stock to reserve
less memory or prove that the chosen size covers every application allocation.

Each launcher directory also contains `memory.json`, a versioned build report.
It separates the page-rounded application code, minimum RW layout (boot page,
static data and reserved stack), linked startup stage, startup stack and MMU
workspace. Per-model totals distinguish Direct from Stock: HP39 Stock retains
two additional native workspace pages. With `MAX_RW_BYTES`, the report also
calculates the pool needed to expose that complete ceiling; without it, those
fields are null. `required_rw_bytes` records an optional explicit minimum,
which is included in the per-model minimum pool totals; `minimum_rw_pages`
still describes the ABI-only layout. Deleting the report causes CMake to
regenerate it.

The report's `serialized` section records actual output sizes: the standalone
HPC1 file, the embedded payload (possibly compressed), and each model's raw
launcher, wrapped transfer file and native object in nibbles. The object extent
is checked against the wrapper's encoded size field; its eight-byte transfer
header is excluded. Wrapper-dependent values are null for `--raw-only` builds.
These are nested representations, not additive allocations: the wrapped file
already contains the launcher and embedded payload, and launching it does not
also require storing the standalone HPC1 file. HP39 Aplet envelopes, directory
metadata, native allocation overhead and transfer/install temporary copies are
not included in these serialized sizes.

These totals describe required pool geometry, not measured free memory or a
change to Stock's allocation policy. The minimum RW layout does not promise
application heap capacity. Native allocator overhead, stored launcher/payload
objects and transfer staging are excluded and listed in the report. The current
display backend allocates LCD DMA buffers from the application heap: their
backing storage and alignment slack consume the same RW arena, not an additional
native allocation. An ABI-only RW minimum does not budget those heap requests;
choose an application minimum that includes them. A successful build does not prove that a particular
calculator state has enough memory to launch.

## Own calculator objects in C++

`<hpclang/object.hpp>` provides the C++17 `hpclang::owned_object` for independent
calculator-object snapshots. It has no allocation of its own, exceptions or
RTTI. The underlying C operation can allocate and reports errors through its
return value and `errno`.

```cpp
#include <hpclang/object.hpp>
#include <cstdint>

hpclang::owned_object value;
if (value.receive([](auto **out) {
        return hpclang_object_create_integer_i64(42, out);
    }) != 0) {
    return 1;
}
std::int64_t answer = 0;
if (hpclang_object_integer_i64(value.get(), &answer) != 0)
    return 2;
```

The owner automatically releases its snapshot on normal scope exit. Move it to
transfer ownership; copying is disabled. `get()` borrows the snapshot, `reset()`
releases it, and `release()` transfers responsibility back to a C caller.
An explicit raw-pointer constructor adopts an already-owned C result. Never
adopt a borrowed pointer or put one snapshot in two owners.

`receive()` passes an initially null output pointer to a nonthrowing producer.
On failure, the old snapshot and the producer's `errno` are preserved. On
success, it replaces the old snapshot. The producer must return a new owned
snapshot, not the owner's existing pointer. This works with constructors,
object-store reads, Port reads and composite extraction without duplicating
their C APIs. The producer may read the old snapshot while building the new one.
Native-service availability and numeric/encoding rules remain those of the C
API; owning a snapshot does not grant access to a native RPL stack on HP39.

See `examples/object-cpp` for a complete C++17 application. Automatic local
cleanup applies to normal returns; `exit()` does not unwind local C++ objects.

For byte streams, `<hpclang/storage.hpp>` supplies move-only `owned_stream`
and `owned_stream_directory` owners. The installed `examples/storage-cpp`
is a standalone C++17 application that opens `README.TXT` on an HP49g+/HP50g
FAT SD card and checks reading and both close results. Cleanup on an early
return preserves `errno`; call `close()` explicitly when its result matters.
The owners retain the C backend's model and media requirements.

### Own directory handles and iterators

`<hpclang/object-store.hpp>` adds `owned_object_store` and
`owned_object_store_iterator`. They are `std::unique_ptr` aliases with the
matching C close operations, allocate no storage of their own, and preserve
`errno` when closing. The C services still determine model/context availability.

```cpp
#include <hpclang/object-store.hpp>

hpclang::owned_object_store home{hpclang_object_store_home()};
if (!home)
    return 1;
hpclang::owned_object_store_iterator entries{
    hpclang_object_store_iterator_open(home.get())};
if (!entries)
    return 2;
home.reset(); // Entry metadata is an independent snapshot.
```

Use the C iterator/read/write functions with `get()`. Names remain explicit
calculator bytes; these wrappers do not reinterpret them as UTF-8. A child
directory handle also owns its logical path independently of its parent handle.
Move owners to transfer responsibility, or use `release()` to transfer it back
to C. Never adopt a borrowed pointer or call `reset(owner.get())`.

For failure-preserving replacement, construct a temporary owner from the new C
result, check it, then `swap()` it into the old owner. Calling `reset()` with a
failed null result would close the old handle. Local scopes clean up on normal
return; explicitly reset owners before `exit()` or `_Exit()` if they must close
before termination.
Global-object destruction follows the runtime's ordinary exit handling.

## Migrate HPGCC source

See the [migration guide](migration.md) for per-interface decisions, precision,
ownership, text encodings and an application acceptance checklist. This guide
is also installed beside this page in the SDK.

Compatibility is provided by installed headers **and runtime implementations**.
For supported calls, keep the historical include spelling and function calls;
build the source with the hpclang toolchain and CMake package described above.
Do not add an old HPGCC include directory ahead of the SDK headers or link its
precompiled runtime libraries. This is source compatibility, not compatibility
with the old library binaries, startup code or internal structure layouts.

The SDK currently installs `hpgraphics.h`, `hpkeyb49.h`, `hpstdio.h`, `hpconio.h`,
`hpstring.h`, `hpsys.h`, `hptime.h`, `saturn.h`, `satdir.h` and `fsystem.h`. Each exposes its implemented
subset; the presence of a header does not promise every function from every
HPGCC version. An umbrella `hpgcc49.h` is not provided. Replace that include
with the individual headers needed by the application, then resolve remaining
calls against the supported declarations rather than supplying old headers.

Start with a small application and check these migration boundaries:

- Remove application definitions of `int errno`; use `<errno.h>` and the
  SDK runtime's error state.
- Review direct hardware access, inline assembly and assumptions about raw
  calculator addresses separately. Recompilation does not make them portable
  across the six calculator models.
- Check service availability and operation errors in the intended launch
  context. A successful build does not make native RPL or SD services available
  on a model that lacks them.
- HPG default fonts preserve the legacy character cell and byte input, but
  their glyphs need not be pixel-identical. The adapter uses live native fonts
  where supported and packaged newRPL fallbacks elsewhere.
- Rebuild and package the application using the SDK launcher helpers, then
  validate its input, display, persistence and return to the calculator OS.

`examples/hpgcc-timer` is a complete installed-package starting point for
legacy timing, calendar, tone and packed-BCD calls. `sys_playTone2(hz, ms)` uses
milliseconds; `sys_playTone(hz, cs)` uses hundredths of a second. Both block,
restore borrowed sound hardware state and report failure through `errno`.
`sys_bcd2str` returns borrowed text overwritten by its next call.
`sys_slowOn/Off` select economy mode and restore the original host clock;
monotonic timing continues across both transitions. These are profile selections,
not nested scopes, and failures are reported through `errno`.
The example's rendering uses the clean hpclang API;
it demonstrates mixing compatible calls with new services rather than proving
that an arbitrary historical application compiles unchanged.

## Package an HP39 Aplet

HP39g+, HP39gs and HP40gs share the historical HPGCC39 Aplet format, with a
different model launcher inside each package. Supply an extracted HPGCC 1.1
kit with HP39 support. Prepare its host tool and resources using the installed
helper and a **host** Clang compiler:

```sh
python /path/to/sdk/share/hpclang/tools/prepare-hp39-aplet.py \
  /path/to/extracted-hpgcc39 /path/to/prepared-hpgcc39
clang -O2 /path/to/prepared-hpgcc39/src/hp2aplet.c \
  -o /path/to/prepared-hpgcc39/hp2aplet
```

The host tool runs on the development computer, so do not give this compile
command the calculator's ARM target flags. Its original source and binary
assets remain external dependencies; preparing them does not change their
redistribution terms. The SDK helper adds the native Aplet Start/Reset views
and the companion Reset program.

After declaring the application and enabling its launcher, add:

```cmake
hpclang_target_launchers(my_application MODELS hp39gs)
hpclang_target_aplet(my_application
    MODEL hp39gs NAME "My application" AUTHOR "My name"
    TOOL "${HP39_APLET_TOOL}" ASSETS "${HP39_APLET_ASSETS}")
```

Configure the project with `-DHP39_APLET_TOOL=/path/to/prepared-hpgcc39/hp2aplet`
and `-DHP39_APLET_ASSETS=/path/to/prepared-hpgcc39/data`. `NAME` accepts 1–48
printable ASCII characters; `AUTHOR` defaults to `hpclang` and accepts 1–33.
The author bound prevents the historical splash renderer from drawing before
the screen's left edge.

The resulting `my_application-aplet/hp39gs/` contains `HPGCC000.000`,
`HPCRESET.000`, `LIB275L.000` and the Connectivity Kit directory files.
Transfer the Aplet and Reset program using Conn3x/HPGComm; install the library
once. Build separate packages with `MODEL hp39gplus` or `MODEL hp40gs` for
those calculators. Reset clears only the selected Aplet's custom data.

The same directory contains a host-side `footprint.json` report. Its `files`
map and `bundle_file_bytes` sum describe the five transfer files, including the
library and directory listings. `aplet_file_bytes` counts `HPGCC000.000`;
`embedded_launcher_bytes` counts the launcher inside it, without the eight-byte
`HPHP49-C` transfer header. `aplet_non_launcher_bytes` is the remaining Aplet
envelope and support data. The input launcher, embedded launcher and Aplet are
nested representations: do not add them together. The report is not listed for
transfer and is excluded from the bundle total. These file sizes do not measure
calculator resident allocations, transport overhead or peak installation RAM.

For HP48gII, install ARMToolbox48 library 275 and transfer its model's `.hp`
object. HP49g+/HP50g use their corresponding ARMToolbox launcher support.
The generated application does not include or replace the calculator OS.

## Choose services at runtime

The installed `examples/notebook` combines the declarative C UI, calculator
clock and native object persistence. It uses Aplet-owned data on HP39 Stock
and a dedicated HOME object on classic Stock, with explicit Save/Reload
actions and rejection of foreign data. Its README describes the storage and
encoding contract. All six original-ROM Stock emulator persistence cases
passed Runtime 39928737278; this is emulator evidence, not physical-device
validation.

Include `<hpclang/device.h>` and read `hpclang_device_info()` for model and
visible display dimensions. Use `hpclang_device_capability()` to decide whether
to offer an optional service in the current launch context:

```c
int state = hpclang_device_capability(HPCLANG_CAPABILITY_RPL_OBJECT_STORE);
if (state == HPCLANG_CAPABILITY_SUPPORTED) {
    /* Offer the calculator object-store action. */
}
```

Compare with `HPCLANG_CAPABILITY_SUPPORTED` explicitly. A nonzero value can
also mean `HPCLANG_CAPABILITY_CONTEXT_UNAVAILABLE`: the model can provide the
service, but this launch lacks its required native context. `UNSUPPORTED`
means that service is not provided for this model; `-1` reports an invalid
capability ID through `errno`.

Read and mutation capabilities are separate. Availability does not reserve
memory, insert an SD card, validate objects or acquire a display session.
Check each operation's result and follow its documented ownership/cleanup
contract. Model geometry does not expose framebuffer addresses or pixel packing.

## SDK validation

The Core CI gate installs to a temporary prefix, relocates it, copies out the
examples and builds both languages from the installed package, including fonts
and HPC1/launcher output. It rejects compile commands referring to the source checkout
or the old install prefix. This establishes build/relocation acceptance;
calculator launch and return have separate E2E gates.
Core's installed-SDK gate also builds the historical host tool with Clang
and verifies both external examples' installable Aplets for all three
HP39 models. The SDK and E2E share both launcher and Aplet packaging tools.
