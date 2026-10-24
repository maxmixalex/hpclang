# Declarative device manager

This ordinary C application exercises `View = f(Model)` on calculator screens:

- Two instances of the same status-card component use identical local keys.
- Connect/disconnect chooses a different button from model state.
- A controlled UTF-8 filter edits the model and filters stable device IDs.
- The deterministic simulated scan fails once, succeeds on retry and supports cancellation.
- Insert and reverse preserve surviving device-control identities.
- A clipped waveform Canvas updates through a model revision.

The scan is a demonstration provider, not physical device discovery. ASCII labels
keep the packaged font small and work on all six models. The waveform uses
grayscale surface primitives; a keyboard button replaces the sketch's pointer
gesture. All content lives in a scrolling viewport.

## Build

Use an installed hpclang SDK and its toolchain, as described in the SDK getting
started guide. Set `ATFE` to Arm Toolchain for Embedded and put its `bin`
directory on `PATH`, then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=MinSizeRel \
  -DCMAKE_PREFIX_PATH=/path/to/sdk \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/sdk/share/hpclang/cmake/toolchains/hpclang-armv4t.cmake
cmake --build build
```

Configure with `-DHPCLANG_DEMO_INVERTED=ON` for the alternate palette and
`-DHPCLANG_DEMO_NATIVE_FONT=ON` to prefer the calculator's default ROM font.
When ROM glyphs are unavailable in the launch context, the explicit packaged
font remains the fallback. Both choices use the same application model,
declarations, layout engine and keyboard actions. The custom waveform keeps
its own palette. These options do not claim to reproduce the Stock interface.

The build produces the HPC1 image and six-model launchers. HP39 Aplet packaging
uses the SDK's separate `hpclang_target_aplet` helper and its declared historical
tool/assets dependencies.

The example requires and caps its application read/write arena at 36 KiB through
`MIN_RW_BYTES` and `MAX_RW_BYTES`. This includes static data, heap and the reserved
8 KiB application stack. Launcher admission also accounts for application code
and startup workspace before entering the application. Inspect the generated
`declarative_ui-launchers/memory.json` for the linked image's pool requirements.
Stored objects and native allocator overhead are additional. The current LCD
DMA buffers use the application heap and are already inside its RW budget;
these settings do not reduce the Stock launcher's reservation.

## Operate

Up/down moves focus and scrolls it into view. Enter activates a button. In the
filter, native Alpha/Shift rules enter text, left/right moves the cursor, and
Enter accepts. Connect, then scan; choose Retry after the intentional first
failure. Toggle devices, insert another or reverse the order. Change wave
updates the plot. Exit returns to Stock; ON exits outside text editing.

The application owns a fixed 48-node/512-byte/one-task declaration configuration and
creates its renderer once. Its display session is explicitly acquired and
released. It retries display timeouts from the accepted image, reborrowing the
LCD back page each time. Startup/allocation failures return an error to the
launcher. The full example flow passes local Direct and Stock tests on all six
models, with normal return, alternate-theme and native/fallback-font checkpoints.
The ARM declaration pool uses 13,692 bytes; renderer and LCD pages are additional.
The shared application flow has passed Runtime CI. The new explicit launcher
memory requirement is a separate follow-up pending CI acceptance.
