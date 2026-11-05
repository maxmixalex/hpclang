# Cooperative C++ timer

Build with the installed SDK as described in `docs/getting-started.md`.
The application prints five elapsed one-second ticks, then returns to Stock.
It moves a repeating schedule into the loop and consumes all missed expirations
without restarting the timer or accumulating polling drift.

`cooperative_timer` uses no heap allocation, interrupt or background callback.
The loop must poll it; `delay` is a busy wait, not an operating-system sleep.
Start/poll failures return an error and preserve the old schedule (and poll
output). Check each result. Negative chrono durations are invalid. Destruction
discards the local schedule; there is no hardware lease to release. The C clock's
model/context limits still apply. Six-model Direct emulator execution passes
with an external observer checking stdout progress, elapsed time and return.
The timer wrapper also passes six-model Direct/Stock system-services integration;
this does not claim Stock execution of the standalone example or hardware testing.
