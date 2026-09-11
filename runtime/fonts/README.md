# Font source package

`fonts.zip` is the canonical custom-font source package for hpclang. It is tracked with Git LFS and must remain byte-for-byte identical to the reviewed source archive.

The archive is **not** consumed by normal application builds. Python code under `tools/` is repository-maintenance tooling only: maintainers use it when auditing the source package or regenerating checked-in font assets. SDK users compiling C/C++ applications must not need Python, Pillow, fontTools, or anything from `tools/`.

The pipeline is:

1. `runtime/fonts/fonts.zip` — canonical source archive, stored with Git LFS.
2. `tools/hpclang-font.py` — verifies the archive manifest and parses supported BDF/newRPL-BMP/TTF/FON source faces.
3. `tools/regenerate-font-assets.py` — batch-generates section-GC-friendly `.S` files plus the CMake catalog under `runtime/fonts/generated/`. Large Unicode faces are split into bounded assembly chunks while keeping one logical font descriptor.
4. normal C/C++ build — consumes only those generated assets through CMake/clang/lld; no source-font parser runs here.

`STATIC` selection means the final HPC1 retains only explicitly selected glyph mappings. `FULL` retains the complete mapped repertoire of the selected face. These are linker/package semantics, not a reason to invoke Python during an application build. The batch catalog is deliberately independent of UI font roles: `mini`, `text`, and `edit-large` are semantic choices layered on top of the general arbitrary-metrics font ABI.

Repository-maintenance verification/regeneration is equivalent to:

```bash
python3 tools/hpclang-font.py --package runtime/fonts/fonts.zip verify
python3 tools/hpclang-font.py --package runtime/fonts/fonts.zip audit
python3 tools/regenerate-font-assets.py --package runtime/fonts/fonts.zip
```

The `Maintenance / Font assets` workflow performs the same steps with Git LFS materialized and commits changed generated assets on `m7-fonts-gallery`.

Current reviewed source archive:

- size: `35,137,072` bytes
- SHA-256: `6b0cf8eca97d52d9a410b4ebb2f77585b9d895d5dab46fb106ec8992791c4f86`

Do not replace this file with a re-packed ZIP, extracted directory, normalized-only database, or Git LFS pointer whose object has not actually been uploaded.
