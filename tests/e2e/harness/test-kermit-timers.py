#!/usr/bin/env python3
"""Exercise the bundled sender's actual timer code with deterministic timestamps."""

from pathlib import Path
import subprocess
import sys
import tempfile


def verify(source: Path) -> None:
    text = source.read_text()
    start = text.index("/*  R T T I N I T")
    end = text.index("#endif /* CK_TIMERS */", start)
    timer_code = text[start:end]
    prelude = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define VOID void
#define CK_ANSIC
#define CKFLOAT double
#define GFTIMER
#define RTT_SCALE 1000
static long receiver_interval;
#define debug(a, b, c, d) do { \
    if (strcmp(b, "RTT RECV") == 0) receiver_interval = (d); \
} while (0)
int timint = 1, rttflg = 1, spackets = 10, rpackets = 10;
int mintime = 1, maxtime = 30, rcvtimo;
long rttsamples, rttvariance, rttdelay, pktintvl, rttstddev;
long rrttbl[64], srttbl[64];
int sseqtbl[64];
struct { int pk_rtr; } s_pkt[64];
static double now;
double gftimer(void) { return now; }
long gtimer(void) { return (long)now; }
"""
    checks = r"""
static void sample(int n, double previous, double sent, double received) {
    now = previous; rrttbl[n - 1] = rttclock();
    now = sent; srttbl[n] = rttclock();
    now = received; rrttbl[n] = rttclock();
}
int main(void) {
    int n;
    for (n = 0; n < 64; ++n) sseqtbl[n] = n;
    rttinit();
    /* A fast exchange crossing a second boundary is still a fast exchange. */
    sample(5, .960, .990, 1.020);
    assert(rrttbl[5] - srttbl[5] >= 29 && rrttbl[5] - srttbl[5] <= 31);
    assert(getrtt(0, 5) == 0 && rcvtimo == 1);
    /* Retransmission makes the ACK timestamp ambiguous: preserve backoff. */
    sample(6, 1.020, 6.900, 7.000);
    s_pkt[6].pk_rtr = 2;
    rcvtimo = 8;
    assert(getrtt(0, 6) == -1 && rcvtimo == 8);
    /* The next clean exchange restores the normal interval immediately. */
    sample(7, 7.000, 7.015, 7.050);
    assert(getrtt(0, 7) == 0 && rcvtimo == 1);
    /* A genuinely slow, unambiguous exchange still increases the interval. */
    sample(8, 7.050, 7.100, 9.100);
    assert(getrtt(0, 8) == 0 && rcvtimo >= 3 && rcvtimo <= 6);
    /* Keep the receiver's existing one-second margin in the new units. */
    sample(9, 9.100, 9.150, 9.200);
    assert(getrtt(0, 9) == 0 && rcvtimo == 1);
    now = 9.200; srttbl[9] = rttclock();
    now = 9.300; rrttbl[10] = rttclock();
    assert(getrtt(1, 10) == 0 && receiver_interval == 1100);
    timint = 0;
    assert(getrtt(0, 8) == 0 && rcvtimo == 0);
    puts("PASS: Kermit subsecond sampling, retransmission ambiguity, recovery, slow peer");
    return 0;
}
"""
    with tempfile.TemporaryDirectory(prefix="hpclang-kermit-timers-") as directory:
        root = Path(directory)
        program = root / "timers.c"
        program.write_text(prelude + timer_code + checks)
        executable = root / "timers"
        subprocess.run(
            ["gcc", "-std=c99", "-Werror", str(program), "-o", str(executable)], check=True
        )
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    verify(Path(sys.argv[1]))
