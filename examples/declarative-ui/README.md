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

The build produces the HPC1 image and six-model launchers. HP39 Aplet packaging
uses the SDK's separate `hpclang_target_aplet` helper and its declared historical
tool/assets dependencies.

## Operate

Up/down moves focus and scrolls it into view. Enter activates a button. In the
filter, native Alpha/Shift rules enter text, left/right moves the cursor, and
Enter accepts. Connect, then scan; choose Retry after the intentional first
failure. Toggle devices, insert another or reverse the order. Change wave
updates the plot. Exit returns to Stock; ON exits outside text editing.

The application owns a fixed 64-node/1024-byte declaration configuration and
creates its renderer once. Its display session is explicitly acquired and
released. It retries display timeouts from the accepted image, reborrowing the
LCD back page each time. Startup/allocation failures return an error to the
launcher. Calculator memory use and six-model execution are still being
validated; host flow tests do not establish target acceptance.
