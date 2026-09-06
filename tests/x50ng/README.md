# x50ng headless ARM execution backend

CI checks out a pinned external x50ng revision and replaces only its UI entry
point with a headless test frontend from this directory. Tests read the
emulated LCD controller and drive the calculator key matrix directly.

No GTK/SDL window automation, Xvfb, mouse coordinates or host-screen scraping
is used at runtime. x50ng remains an external emulator/test dependency; none of
its GPL implementation is copied into hpclang's production runtime.

Current roles:

- `E2E / Direct runtime`: common HPC1 execution and stock-state restoration;
- `E2E / HP50g stock launch`: stock HP50g File Manager/ARMToolbox/PrRUN path;
- HP39gs/HP40gs stock launch: generated installed fixture followed by stock
  APLET/ARMToolbox39 UI;
- HP48gII: true medium-apple hardware preparation for the shared 2.15 ROM.

HP39gs/HP40gs use the little-apple x50ng hardware path. HP49g+/HP50g share the
hp4950 path. HP48gII uses the same S3C2410 base with its real 256 KiB RAM and
64-line LCD differences.

See `tests/e2e/README.md` for the test-layer contract.
