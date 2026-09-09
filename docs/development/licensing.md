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

## Built-in bitmap fonts

**Blocker:** the final mini/big glyph source is not frozen yet.

The current implementation mechanically extracts printable Basic Latin glyphs
from the X11 misc-fixed 4x6 and 5x7 ISO10646-1 BDF fonts. Those BDF headers
identify the fonts as public domain ("Share and enjoy."), so the current tables
are safe as an implementation candidate.

Historical HPG also embeds two complete 256-entry bitmap fonts with the same
6/4 and 8/6 metrics. Their source files are part of GPL-licensed HPGCC and carry
HPGCC's linking/use exception, but that does not justify silently relicensing
copied glyph tables as MIT source. If hpclang ultimately uses those exact
tables, their provenance and applicable license must be preserved explicitly
(or the data must live in an optional compatibility component under compatible
terms). Until that decision is made, HPG glyphs remain reference material and
the built-in font asset stays blocked.

## Implementation discipline

For new runtime/platform code:

1. record the externally visible contract;
2. create a fixture or execution test;
3. write a fresh implementation;
4. compare generated structures and behavior;
5. keep historical source outside the supported implementation path.

A normal checkout should contain only code the project is entitled to redistribute and build.

Removing an old research directory does not mean its conclusions should disappear. Durable compatibility or provenance facts belong in the Development documentation before the original generated evidence is pruned.
