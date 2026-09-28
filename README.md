# hpclang

hpclang is an LLVM-native SDK for the ARM-based HP graphing calculators. It uses one ARMv4T/AAPCS32 application ABI and one common runtime across six calculator profiles.

The implementation is organized into three platform families:

- `hp39`: HP 39g+, HP 39gs, HP 40gs;
- `hp48`: HP 48gII;
- `hp4950`: HP 49g+, HP 50g.

Applications are packaged as model-independent HPC1 images. Calculator-specific code handles the native package format, memory setup, stock-system launcher, and return path.

## Validation

All six models have Direct runtime evidence. HP 39gs also passes the canonical runtime on real hardware.

HP 39g+ uses a verified real-machine D1.55 full-NOR fixture. All six models
pass canonical original-system USB launch and Direct runtime E2E. The separate
Storage and Font Gallery workflows also pass on the accepted integration head;
their Stock tests retain SD and UART/SD paths. See
[Validation](docs/development/validation.md) for the formal acceptance records.
USB transport implementation is complete;
repeated-run performance benchmarking is tracked independently in
[Stock USB transport](docs/development/stock-usb-transport.md).

A subsequent main Runtime run exposed an ALG/RPN setup assumption and a keyboard
readiness race; follow-up acceptance is tracked in [Validation](docs/development/validation.md).

## Development documentation

The current documentation is an internal Development site built with Zensical:

- source: [`docs/`](docs/);
- configuration: [`zensical.toml`](zensical.toml);
- published site: <https://zyf722.github.io/hpclang/>;
- API progress/parity: [`docs/development/api-reference.md`](docs/development/api-reference.md), including HPGCC 1.0.3 / 1.1 / 2.0 SP2 / HPGCC3 comparison.

Historical HPGCC source is used as implementation evidence only. A normal hpclang build does not depend on GCC, historical HPGCC runtime objects, or a replacement calculator ROM.


## Development checks

Run `tools/lint.sh` for the repository lint gate: Ruff checks the Python 3.12 host tools and ShellCheck checks tracked shell scripts. C host/unit builds continue to use `-Wall -Wextra -Werror`.

Run `tools/format.sh` to format tracked C/C++ and Python sources with clang-format and Ruff. `tools/format.sh --check <files...>` checks selected files without rewriting them. Quality checks all owned C/C++ and Python sources against the committed format baseline. Third-party and generated font sources are excluded. Use clang-format from the pinned ATfE toolchain and Ruff from `tools/requirements-quality.txt`.

Run `ATFE=/path/to/atfe tools/analyze.sh` for Clang static analysis using the
CMake compilation databases for host unit tests and ARM runtime/Gallery builds.
See [Validation](docs/development/validation.md) for scope and CI responsibilities.
