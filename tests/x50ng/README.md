# x50ng headless ARM execution backend

CI checks out a pinned external x50ng revision and replaces only its UI entry
point with a headless test frontend from this directory. Tests read the
emulated LCD controller and drive the calculator key matrix directly.

No GTK/SDL window automation, Xvfb, mouse coordinates or host-screen scraping
is used at runtime. x50ng remains an external emulator/test dependency; none of
its GPL implementation is copied into hpclang's production runtime.

Current roles:

- `E2E / Direct runtime`: common HPC1 execution and stock-state restoration
  on HP39gs, HP40gs, HP48gII, HP49g+ and HP50g;
- `E2E / HP39/40 stock launch`: real stock serial-disk transfer followed by
  APLET/ARMToolbox39 launch;
- `E2E / HP50g stock launch`: stock File Manager/ARMToolbox/PrRUN path;
- HP48gII: verified 2 MiB full-NOR boot on a 256 KiB / 131x64 medium-apple
  personality; ARMToolbox48 stock launch remains.

HP39gs/HP40gs use the little-apple x50ng hardware path. HP49g+/HP50g share the
hp4950 path. HP48gII uses the same S3C2410 base with its genuine 256 KiB RAM,
64-line LCD and native 48gII bootloader/full-NOR state.

See `tests/e2e/README.md` for the test-layer contract.
