# HPGCC timer calls with the installed SDK

This new C11 migration example uses `hpsys.h`, `hptime.h` and the historical names
`sys_updateTimer`, `sys_sleep`, `sys_waitTicks`, `sys_RTC_seconds` and
`sys_delta_seconds`, plus `sys_tm_RTC` for the civil date/time. Copy this directory out of the installed SDK and build it
with the installed toolchain and CMake package as described in the getting-started
guide. The build produces HPC1 and all six model launchers; HP39 additionally
needs the Aplet envelope described in that guide.

The example requests 35 ms of delay, draws its measured duration and returns
to the calculator system. `sys_sleep` takes milliseconds; `timer.current`
accumulates nanoseconds. Civil seconds have only one-second resolution and
are a separate measurement. No exact observed duration is promised.

`hptime.h` forwards the SDK libc time types and supported RTC adapter. Calendar
fields use standard `struct tm` conventions; weekday and year-day are computed,
and `tm_isdst` is -1 because the calculator RTC has no timezone policy. Read errors
set `errno` and leave the supplied structure unchanged.

The timer calls demonstrate source compatibility. Rendering explicitly uses
hpclang's display API. This is not an unchanged historical application or a
claim that the entire HPGCC library is implemented. Use the individual supported
headers; an umbrella `hpgcc49.h` is not currently provided. Raw PWM state and
the old binary layout of `sys_timer_t` are not preserved.
