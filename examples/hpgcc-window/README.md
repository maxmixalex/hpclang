# Window event and timer example

This C client uses `winevent.h` and `wintimer.h` to animate a rectangle for
twenty timer callbacks, then returns to Stock. The interval is 50 milliseconds;
repaint event 7 draws through the existing HPG backend. It demonstrates the
event/timer adapter, not historical widgets or typed keyboard input.

Build as an ordinary hpclang SDK example with `find_package(hpclang)` and the
ARMv4T toolchain. The target produces an HPC1 payload and all six launchers.
Native execution and independent pixel acceptance are still pending.

Historical void functions report errors through `hpclang_win_error()`. The
example checks it after the loop and explicitly releases HPG's display lease.
Callbacks and application data must outlive active dispatch snapshots. The
bounded adapter currently supports 61 application handlers plus three internal
idle handlers, 256 simultaneous snapshot entries and 32 timers. Exhaustion
latches `ENOSPC`; `hpclang_win_reset()` clears registrations only when inactive.
