# Historical console output

This installed-package example combines C and C++17 output with HPG graphics,
then waits for a key before returning to the calculator. Build it with the
installed hpclang CMake package and ARMv4T toolchain, as described in the SDK
getting-started guide. No generated font declaration is needed: the historical
adapter selects its supported native/fallback minifont.

Including `hpconio.h` in either language enables lazy standard-stream output;
including it in multiple translation units is supported. `printf`, `puts` and
`std::printf` keep their standard signatures. Text uses calculator bytes, not
UTF-8. The visible screen holds complete 4×6 cells; it does not expose the old
partial cell overlapping the status column.

`hpkeyb49.h` supplies the separate historical key wait. Interactive line editing,
historical number-format policy and unsafe unbounded `gets` are not implemented
by this output example. An application using a clean UI can explicitly bind
its own standard streams; that binding takes precedence over this default.
