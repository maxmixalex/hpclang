# x50ng headless ARM execution backend

CI checks out a pinned external x50ng revision and replaces only its UI entry
point with a headless test frontend from this directory. The tests read the
emulated LCD controller directly and drive the calculator key matrix directly.

No GTK/SDL window automation, Xvfb, mouse coordinates or host-screen scraping is
used at runtime. x50ng remains an external emulator/test dependency; none of
its GPL implementation is copied into hpclang's production runtime.

The public E2E workflows are organized by semantics:

- `E2E / Official ROM boot`: boot the unmodified HP ROM;
- `E2E / Direct runtime`: run hpclang directly, bypassing calculator menus;
- `E2E / Full HP50g launch`: install and launch hpclang through the normal
  HP50g UI and ARMToolbox.

`E2E / Direct runtime` currently runs one matrix over HP39gs, HP40gs and
HP50g. All three execute the same HPC1 runtime suite, with only the
target/model firmware adapter differing.

The HP50g stock gate mounts a host directory as calculator SD storage, installs
ARMToolbox through the stock File Manager, and drives the stock
LIB -> ARMToolbox -> PrRUN path. Its input sequence is stored separately under
`tests/e2e/sequences/`.

For HP39gs/HP40gs, the same stock-shell tier will use deterministic post-transfer
fixtures because x50ng does not currently expose a host Conn3x transport. The
stock Aplet UI and launcher remain part of the E2E; the file-transfer protocol
does not need to be a runtime regression gate.

See `tests/e2e/README.md` for the test-layer contract and sequence format.
