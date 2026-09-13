# Historical-code licensing

hpclang is a new implementation. Historical HPGCC source is evidence and compatibility reference, not code to copy automatically into the SDK.

## HPGCC 1.x / 2.x

The historical project is GPL-licensed. Studying it and recording observable contracts is compatible with hpclang's research process, but copying substantial implementation source would carry derivative-work obligations.

The default implementation policy is therefore independent reimplementation from documented formats, observable behavior, and hardware/emulator tests.

## HPGCC3

HPGCC3 uses a separate license with redistribution restrictions. hpclang must not vendor its runtime/library implementation.

It may be used to understand public behavior, identify architectural ideas, and design independently implemented compatible APIs.

## HP tools and binary assets

Historical HP/System-RPL tools, bootloaders, firmware, full-NOR dumps, and calculator binary assets have separate provenance and redistribution terms.

They are not vendored into the normal source tree unless their redistribution terms are explicitly suitable. Where CI needs an external firmware fixture, the documentation must record what the fixture is, where it came from, and how its identity is checked.

## Font assets

The old provisional Basic-Latin tables are no longer the long-term custom-font source. `runtime/fonts/fonts.zip` is the reviewed source bundle used to regenerate the checked-in font catalog. It contains upstream font families together with their source distributions/license material; redistribution decisions are made per included family rather than inferred from historical HPGCC code.

Repository-maintenance tooling parses the selected BDF, FON, and newRPL bitmap sources and emits independent hpclang assembly assets. TTF is deliberately not a canonical importer format: bitmap candidates that only ship an outline/container font must first provide a reviewable bitmap-source or deterministic asset-preparation path rather than silently entering the SDK through a generic TTF rasterizer. Normal application builds consume only the generated assets and do not execute source-font tooling.

The stock-ROM fonts exposed by `hpclang_font_mini()`, `hpclang_font_big()`, and `hpclang_font_default()` are a separate case: hpclang reads the calculator's own active ROM data at runtime and does not copy those ROM bitmap tables into the repository.

Historical HPG font tables remain compatibility evidence only. They must not be copied into hpclang merely to reproduce the old 256-entry HPG character namespace.

The custom-font build contract has three repertoire modes—`STATIC_AUTO`, `STATIC_EXPLICIT`, and `FULL`—but those modes do not change licensing. A generated application must still use a face whose upstream terms permit the intended redistribution/use. The source bundle and its accompanying license material are therefore the provenance reference; generated `.S` files are derived build assets, not a substitute for upstream license notices.

## Implementation discipline

For new runtime/platform code:

1. record the externally visible contract;
2. create a fixture or execution test;
3. write a fresh implementation;
4. compare generated structures and behavior;
5. keep historical source outside the supported implementation path.

A normal checkout should contain only code the project is entitled to redistribute and build.

Removing an old research directory does not mean its conclusions should disappear. Durable compatibility or provenance facts belong in the Development documentation before the original generated evidence is pruned.
