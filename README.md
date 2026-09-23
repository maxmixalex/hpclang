# hpclang

hpclang is an LLVM-native SDK for the ARM-based HP graphing calculators. It uses one ARMv4T/AAPCS32 application ABI and one common runtime across six calculator profiles.

The implementation is organized into three platform families:

- `hp39`: HP 39g+, HP 39gs, HP 40gs;
- `hp48`: HP 48gII;
- `hp4950`: HP 49g+, HP 50g.

Applications are packaged as model-independent HPC1 images. Calculator-specific code handles the native package format, memory setup, stock-system launcher, and return path.

## Validation

HP 39gs, HP 40gs, HP 48gII, HP 49g+, and HP 50g pass Direct runtime and original-system E2E. HP 39gs also passes the canonical runtime on real hardware.

HP 39g+ support builds, but its E2E is blocked until a real machine is available to provide a trustworthy stock flash dump.

## Development documentation

The current documentation is an internal Development site built with Zensical:

- source: [`docs/`](docs/);
- configuration: [`zensical.toml`](zensical.toml);
- published site: <https://zyf722.github.io/hpclang/>;
- API progress/parity: [`docs/development/api-reference.md`](docs/development/api-reference.md), including HPGCC 1.0.3 / 1.1 / 2.0 SP2 / HPGCC3 comparison.

Historical HPGCC source is used as implementation evidence only. A normal hpclang build does not depend on GCC, historical HPGCC runtime objects, or a replacement calculator ROM.


## Development checks

Run `tools/lint.sh` for the repository lint gate: Ruff checks the Python 3.12 host tools and ShellCheck checks tracked shell scripts. C host/unit builds continue to use `-Wall -Wextra -Werror`.

Run `tools/format.sh` to format tracked C/C++ and Python sources with clang-format and Ruff. `tools/format.sh --check <files...>` checks selected files without rewriting them. Formatting is configured centrally, but the repository does not yet force a one-time full-tree reformat of historical sources.
