# Fixture provenance

Automated E2E depends on external stock firmware, bootloader, or full-NOR material. These inputs are part of the validation evidence and must be distinguished from hpclang build products.

## HP 39gs and HP 40gs

HP 39gs and HP 40gs Direct and stock-launch tests use the official 2.22 firmware archives preserved by hpcalc.org:

- HP 39gs: `hp39gs222.zip`, member `HP39GS_2.22/hp39gs_v2.22.bin`;
- HP 40gs: `hp40gs222.zip`, member `HP40GS_2.22/hp40gs_v2.22.bin`.

Pinned x50ng still supplies the generic bootloader mechanism used for these update images. These gates validate the original HP39gs/40gs stock OS and launcher behavior; they are not evidence that the emulator bootloader is a native dump from those models.

## HP 39g+

The HP39g+ fixture is a complete 1 MiB NOR image acquired from real hardware in
September 2026. The read-only one-launch acquisition path streamed the physical
flash in bounded blocks and the host reconstructed an exact 1,048,576-byte image.

Verified raw-image identity:

- size: `0x100000` bytes;
- SHA-256: `95920417d4dee311e6113f11720de11018da166a8a777d9255fbff2dd6433f67`;
- firmware-identifying string present in the image: `Version HP39-D1.55,*0`;
- ARM vector/boot area is nonblank and the image ends in a normal erased-`0xff`
  flash region.

The repository stores the fixture as
`tests/e2e/fixtures/hp39gplus/flash.bin.xz`. The target profile records the
expected decompressed size and SHA-256. `tools/prepare-nor-fixture.py` refuses
to install the image into x50ng state unless both checks pass.

Because this is a full NOR image, HP39g+ Direct and Stock E2E boot the fixture
directly as x50ng persistent `flash` state. They do **not** combine an HP39g+
OS image with the HP50g bootloader and do not reuse the HP39gs/40gs update
archives.

The temporary ROM-dumper/boot-dump research sources were retired once the full
fixture was obtained. Their useful result is now the fixture provenance above
and repository history, not a maintained extraction subsystem.

The full fixture establishes HP39g+ firmware identity and makes native runtime
E2E possible. It does **not** by itself establish that HP39g+ uses the same
physical ROM-font object/page layout as HP39gs/40gs. That mapping remains a
separate evidence task, so the gs/40gs-only native EditLine/font offsets are not
silently promoted to HP39g+.

## HP 48gII

The validated HP48gII target is the second-generation USB model.

Independent hardware documentation and newRPL's HP48gII target agree on the emulator-relevant differences:

- physical RAM ends at `0x08040000`: 256 KiB;
- LCD height is 64 rather than 80;
- firmware preamble is `KINPOHP48GIIMAGE`;
- the target has distinct LCD initialization values.

Historical HPGCC 1.1 contains a dedicated `ARMToolbox/masd_source48` path. Its Saturn-to-ARM conversion and memory-pool setup differ from the normal HP49/50 path, but it exposes the same ARMSYS fields used by the current hpclang stage0/stage1 boundary.

### Full-NOR provenance

The provenance chain has three distinct layers:

1. ZephRay's 2015 cnCalc hardware-modification write-up demonstrates the original method: replace an HP39gs NOR with SST39VF160, JTAG a ROM extracted from a real HP48gII at flash address zero, then boot as HP48gII: <https://www.cncalc.org/thread-11475-1-1.html>
2. wjchen's 2020 cnCalc post publishes a specifically identified factory 2.08 build-92 HP48gII JTAG dump and records its full-NOR properties: <https://www.cncalc.org/thread-24739-1-1.html>
3. lyc8503's 2021 write-up provides the currently live downloadable full-NOR fixture used by CI: <https://blog.lyc8503.net/en/post/hp39gs-modify/>

wjchen documented `48gII_208_92_NOR_CNA0000000.raw` as a complete 2 MiB JTAG dump of a factory 2.08 build-92 HP48gII, including the bootloader, with the serial cleared to `CNA0000000`. The original cnCalc attachment has expired.

The currently used mirrored `48gii.bin` is an independent dump lineage, not a byte-for-byte mirror of that serial-cleared image. It was independently inspected in CI:

- exact size: `0x200000` bytes;
- SHA-256: `4899102743d3420dc517ae1b489fe2405229be616dbdb1b9f00cc1d0aeae1937`;
- ARM vector table at offset zero;
- `KINPOHP48GIIMAGE` in the boot area;
- `Kinposhcopyright` at firmware boundary `0x4000`;
- bootloader serial `CNA6070206`, proving it is not wjchen's serial-cleared dump.

For an independent vendor-origin cross-check, hpcalc.org archives Hewlett-Packard's 2.15 emulator ROM as a 2 MiB `rom.49g` and describes it as an image from the HP48gII/49g+/50g family: <https://www.hpcalc.org/details/6744>.

hpcalc.org also preserves Hewlett-Packard HP49g+/50g ARM bootloaders but does not provide a standalone HP48gII ARM bootloader archive. The full-NOR mirror therefore remains the current source for the HP48gII boot area, and CI pins its SHA-256 rather than trusting mutable web content.

`tools/prepare-hp48gii-nor.py` installs the verified raw image directly as x50ng persistent flash state. Pinned x50ng defaults to `HDW_REINIT_NONE`; passing `--flash` or `--flash-full` would overwrite the already prepared HP48gII NOR.

### Validation results

GitHub Actions run `34132800652` booted the verified full NOR on the HP48gII model and completed the canonical runtime contract:

- machine-readable result `0x50415353`;
- `saw_pass=true`;
- `returned=true`;
- 131×64 PASS and post-return LCD captures.

The calculator-side path was closed by run `34147202219`:

```text
stock HP48gII UI
 -> Serial Kermit
 -> L275_48.LIB + generated HPCLANG.HP
 -> Port 0 library install
 -> warmstart
 -> ARMTo
 -> PrRUN
 -> HPC1 PASS
```

## HP 49g+ and HP 50g

HP 49g+ and HP 50g use the shared official 2.15 OS fixture while retaining distinct native ARM bootloaders:

- HP 49g+: `boot-49g+.bin`;
- HP 50g: `boot-50g.bin`.

Those bootloader filenames are model metadata and are intentionally not collapsed into a single shared file.
