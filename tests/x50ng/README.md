# x50ng headless ARM execution backend

CI checks out a pinned external x50ng revision, replaces only its UI entry point
with the test frontend in this directory, boots official 49g+/50g ARM firmware,
reads the emulated LCD controller directly and writes a 131x80 screenshot.

No GTK, SDL, Xvfb, mouse automation or screen-coordinate automation is used at
runtime. x50ng remains an external emulator/test dependency; none of its GPL
implementation is copied into hpclang's production runtime.

The automated gate now boots the official stock firmware to HOME, enters the
hpclang stage0/stage1/common bootstrap as guest ARM code, runs the same
canonical HPC1 runtime image used on real HP39gs hardware, observes
machine-readable `0x50415353`, and captures the live `RUNTIME PASS` LCD.

The remaining work is the synthetic outer-launcher return boundary and then
replacing the synthetic ARMSYS preparation with the real stock 49g+/50g package
and launcher.
