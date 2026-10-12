# Build an application with the installed SDK

The initial M8 SDK is a relocatable source package. It includes public headers,
runtime sources, generated fonts, CMake integration and the HPC1 packer. The
runtime library is compiled once in each application build with the selected
toolchain. Prebuilt runtime archives and complete Stock installation bundles
are not yet part of this package.

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

This produces `hello_c.elf`, `hello_c.hpc`, `hello_cpp.elf` and `hello_cpp.hpc`.
The HPC1 payload is model-independent; it is not a calculator transfer file.
Model-specific Stock wrapping, the HP39 Aplet installer and external packaging
dependencies remain part of the next SDK delivery slice. The small examples
draw a greeting and return immediately; they are build examples, not interactive
applications or calculator execution acceptance tests.

An application CMake file uses:

```cmake
cmake_minimum_required(VERSION 3.20)
project(my_application LANGUAGES C CXX ASM)
find_package(hpclang CONFIG REQUIRED)
hpclang_add_application(my_application main.cpp)
target_compile_features(my_application PRIVATE cxx_std_23)
```

`hpclang_add_application` supplies the startup, runtime library, linker script
and HPC1 packaging step. `hpclang_target_font` selects packaged font subsets;
the installed examples demonstrate `STATIC_AUTO` with `HPCLANG_UI_TEXT`.
The C++ profile has no exceptions or RTTI. Declare external dependencies in the
application rather than referring back to an hpclang checkout.

The Core CI gate installs to a temporary prefix, relocates it, copies out the
examples and builds both languages from the installed package, including fonts
and HPC1 output. It rejects compile commands referring to the source checkout
or the old install prefix. This establishes build/relocation acceptance;
calculator launch and return have separate E2E gates.
