# Font source package

`fonts.zip` is the canonical custom-font source package for hpclang. It is tracked with Git LFS and must remain byte-for-byte identical to the reviewed source archive.

The archive is **not** consumed by normal application builds. Python code under `tools/` is repository-maintenance tooling only: maintainers use it when auditing the source package or regenerating checked-in font assets. SDK users compiling C/C++ applications must not need Python, Pillow, fontTools, or anything from `tools/`.

The intended pipeline is:

1. `sdk/fonts/fonts.zip` — canonical source archive, stored with Git LFS.
2. repository-maintenance importer — validates provenance/mappings and regenerates checked-in linkable font assets when the source archive changes.
3. normal C/C++ build — consumes only those generated assets through CMake/clang/lld; no source-font parser runs here.

`STATIC` selection means the final HPC1 retains only explicitly selected glyph mappings. `FULL` retains the complete mapped repertoire of the selected face. These are linker/package semantics, not a reason to invoke Python during an application build.

Current reviewed source archive:

- size: `35,229,317` bytes
- SHA-256: `4777a9eced2517cd40b14773cc061b5c29bacb1d7124c54ef03e04b267545652`

Do not replace this file with a re-packed ZIP, extracted directory, normalized-only database, or Git LFS pointer whose object has not actually been uploaded.
