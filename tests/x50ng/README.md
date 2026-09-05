# x50ng headless ARM execution backend

CI checks out a pinned external x50ng revision and replaces only its UI entry
point with a headless test frontend from this directory. The tests read the
emulated LCD controller directly and drive the calculator key matrix directly.

No GTK/SDL window automation, Xvfb, mouse coordinates or host-screen scraping is
used at runtime. x50ng remains an external emulator/test dependency; none of
its GPL implementation is copied into hpclang's production runtime.

The public E2E workflows are organized by semantics:

- `E2E / ROM boot`: official-ROM boot sanity checks;
- `E2E / Canonical`: the common HPC1 execution/return gate;
- `E2E / Stock`: the real calculator shell/launcher path.

`E2E / Canonical` currently runs one matrix over HP39gs, HP40gs and HP50g.
All three execute the same canonical HPC1 runtime suite, with only the target
model/firmware adapter differing.

The HP50g stock gate mounts a host directory as calculator SD storage, installs
ARMToolbox through the stock File Manager, and drives the stock
LIB -> ARMToolbox -> PrRUN path. Its input sequence is stored separately under
`tests/e2e/sequences/`.

For HP39gs/HP40gs, the same stock-shell tier will use deterministic post-transfer
fixtures because x50ng does not currently expose a host Conn3x transport. The
stock Aplet UI and launcher remain part of the E2E; the file-transfer protocol
does not need to be a runtime regression gate.

See `tests/e2e/README.md` for the test-layer contract and sequence format.
