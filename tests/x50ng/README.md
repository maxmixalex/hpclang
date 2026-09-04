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

The normalized ARM gate is complete through PASS and stock-OS resume.

A second stock-launcher gate is intentionally separate. It mounts a calculator
SD directory containing ARMToolbox SETUP.BIN and a normal Launcher-V3 .hp
object, then drives only the calculator key matrix. It must eventually prove
the stock System-RPL/ARMToolbox memory packer creates the ARMSYS state consumed
by the same hpclang stage0.

The Little-Apple bring-up uses the same hardware-level emulator core only as a
starting point. It boots official HP39gs/HP40gs update images and records the
first hardware/firmware boundary that differs; no Saturn-only emulator is part
of this path.
