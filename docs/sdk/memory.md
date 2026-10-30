# Memory budgets and package sizes

Set the application's minimum RW arena from its live working set, then check
that each target can supply the complete launch pool. Keep these quantities
separate when estimating resource requirements:

| Quantity | What it includes | Where to inspect it |
| --- | --- | --- |
| Application RW | Boot page, static data, heap and reserved stack; heap includes renderer and LCD buffers | `memory.json` application fields and application measurements |
| Required launch pool | Application code/RW, startup stage/stack, MMU workspace and model-specific native workspace | `memory.json` model fields |
| Supplied launch pool | Pages made available by the current calculator state and native launcher | Runtime observations |
| Serialized application | Wrapped launcher with its embedded application; HP39 Aplet adds its envelope and support files | `memory.json` serialized fields and Aplet `footprint.json` |
| Whole-system peak | Simultaneously live native allocations, stored objects and temporary transfer/installation data | Measurements of that installation and execution flow |

## Configure RW admission

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

## Read your build report

After building the example target `my_application`, inspect
`build/my_application-launchers/memory.json`. For example:

```python
import json
from pathlib import Path

report = json.loads(Path("build/my_application-launchers/memory.json").read_text())
print("Required application RW:", report["application"]["required_rw_bytes"])
print("Application RW ceiling:", report["application"]["rw_ceiling_bytes"])
for model, layout in report["models"].items():
    print(model, "minimum Stock pool bytes:",
          layout["minimum_stock_pool_pages"] * report["page_bytes"])
```

`required_rw_bytes` is null when no explicit minimum is configured. Compare
against the report from the exact linked application you deploy: adding code,
fonts, controls or static storage changes its pool requirement. A model-specific
build can reduce code size; use the model selector described in
[Getting started](getting-started.md).

For HP39 Aplets, the adjacent `footprint.json` accounts for all five generated
transfer files. The full bundle includes the library even when it is already
installed on your calculator. The wrapped launcher is embedded in the Aplet;
adding both file sizes would count it twice. See
[Aplet packaging](getting-started.md#package-an-hp39-aplet)
for the packaging command and output files.

## Choose and validate a working-set budget

1. Include application state, UI node/text/task storage, renderer buffers, LCD
   buffers and their allocator/alignment overhead in the working set. Reserve
   room for simultaneously live old/new resources during replacement. The
   [UI guide](declarative-ui.md) gives component sizes and lifetime rules.
2. Use equal `MIN_RW_BYTES` and `MAX_RW_BYTES` while validating a fixed arena.
   Exercise maximum content, editing, scrolling, custom drawing, failed
   allocations and reconstruction on each supported model. Check all fallible
   API results and preserve the last usable state when a replacement fails.
3. Measure stack and heap use with the same settings and build. A touched-stack
   watermark is an observation, not a bound on untouched stack reservations.
   A measured heap extent already includes its live renderer/LCD allocations;
   do not add component requests to it again.
4. Validate installation, repeated launch and normal return separately from
   application rendering. Record the calculator model, OS state, toolchain and
   linked image with the measurements. Emulator timings describe that emulator
   run; physical-device response time requires measurement on the device.

When a launch is rejected before entry, inspect the generated pool requirement
and available calculator memory. When an allocation fails after entry, inspect
simultaneously live resources within the RW arena. Increasing an RW ceiling
can help only when the supplied pool has enough pages to expose it; increasing
it also raises the total pool needed to reach that ceiling.
