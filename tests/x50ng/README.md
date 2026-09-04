# x50ng headless ARM execution backend

This test backend uses x50ng's S3C2410/ARM emulator, not Emu48's Saturn-only
execution core.

CI checks out a pinned external x50ng revision, replaces only its UI entry point
with the test frontend in this directory, boots official 49g+/50g ARM firmware,
reads the emulated LCD controller directly and writes a 131x80 screenshot.

No GTK, SDL, Xvfb, mouse automation or screen-coordinate automation is used at
runtime. x50ng remains an external emulator/test dependency; none of its GPL
implementation is copied into hpclang's production runtime.

The initial gate proves unattended ARM/S3C2410 execution and screenshot capture.
The next gate will load the hpclang common stage1/bootstrap and the same HPC1
runtime image used on the real HP39gs, then stop on machine-readable PASS and
capture that LCD frame.
