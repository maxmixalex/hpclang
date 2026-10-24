# Window event and timer example

This C client uses `winevent.h`, `wintimer.h` and `winkeys.h` to animate a rectangle for
twenty timer callbacks, then returns to Stock. The interval is 50 milliseconds;
repaint event 7 draws through the existing HPG backend. It demonstrates the
event/timer/key adapter. Typed digits 0–9 change the rectangle's vertical
position; ON exits early. The character mapping uses the historical
`hpkeyb49.h` numeric namespace on every model, including HP39; it does not
reinterpret model-specific key legends. Modifier indicators and historical
widgets are not drawn.

Build as an ordinary hpclang SDK example with `find_package(hpclang)` and the
ARMv4T toolchain. The target produces an HPC1 payload and all six launchers.
The installed-SDK gate builds this source as C and C++23 after relocating the
package, packages all three HP39 Aplet variants, and compares release outputs
from two clean builds. Native execution and independent pixel acceptance are
still pending.

Historical void functions report errors through `hpclang_win_error()`. The
example checks it after the loop and explicitly releases HPG's display lease.
Callbacks and application data must outlive active dispatch snapshots. The
bounded adapter currently supports 59 application handlers plus five internal
idle/key handlers, 256 simultaneous snapshot entries and 32 timers. Exhaustion
latches `ENOSPC`; `hpclang_win_reset()` clears registrations only when inactive.
