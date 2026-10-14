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
Release such resources before calling it. Global-object destruction follows
the runtime's ordinary exit handling.

## Migrate HPGCC source

Compatibility is provided by installed headers **and runtime implementations**.
For supported calls, keep the historical include spelling and function calls;
build the source with the hpclang toolchain and CMake package described above.
Do not add an old HPGCC include directory ahead of the SDK headers or link its
precompiled runtime libraries. This is source compatibility, not compatibility
with the old library binaries, startup code or internal structure layouts.

The SDK currently installs `hpgraphics.h`, `hpkeyb49.h`, `hpstdio.h`, `hpsys.h`,
`hptime.h`, `saturn.h`, `satdir.h` and `fsystem.h`. Each exposes its implemented
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
legacy timing and calendar calls. Its rendering uses the clean hpclang API;
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

For HP48gII, install ARMToolbox48 library 275 and transfer its model's `.hp`
object. HP49g+/HP50g use their corresponding ARMToolbox launcher support.
The generated application does not include or replace the calculator OS.

## Choose services at runtime

The installed `examples/notebook` combines the declarative C UI, calculator
clock and native object persistence. It uses Aplet-owned data on HP39 Stock
and a dedicated HOME object on classic Stock, with explicit Save/Reload
actions and rejection of foreign data. Its README describes the storage and
encoding contract; device persistence acceptance is still pending.

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
