# Historical-code licensing strategy

hpclang is a new implementation. Historical HPGCC source is evidence and
compatibility reference; it is not automatically suitable for copying into this
repository.

## HPGCC 1.x / 2.x

The HPGCC 1.1 distribution states that the entire project, including its
libraries, is released under GPLv2-or-later, with additional exceptions for
certain non-profit uses.

Consequences for hpclang:

- GPL source can be studied, tested and used to establish historical behavior.
- Directly copying or modifying substantial HPGCC 1.x/2.x implementation code
  would make the corresponding hpclang work derivative and would carry GPL
  obligations.
- Until hpclang's own project license is deliberately chosen with that
  consequence in mind, the default implementation policy is independent
  reimplementation from documented formats, observable behavior and hardware
  tests.
- Small factual constants, file-format values, register offsets and hardware
  addresses are recorded as compatibility facts, with provenance.

This policy also makes it easier to separate truly necessary calculator
contracts from accidental 2004-era implementation choices.

## HPGCC3

The R004.revA distribution uses the HPGCC3 License Agreement rather than the
older GPL project license.

The agreement explicitly restricts redistribution of the Software in source or
binary form, apart from narrow exceptions. It permits users to use include and
example files in their own programs, but does not provide a general right to
vendor the hplib/ROMLIB implementation into another SDK.

Therefore hpclang must **not** copy HPGCC3 runtime/library source into this
repository.

HPGCC3 is used for:

- public executable/API behavior research;
- format and ABI compatibility research;
- identifying useful architectural improvements;
- independent reimplementation of compatible APIs;
- optional interoperability with an already-installed ROMLIB environment.

The hpclang core must remain buildable and useful without HPGCC3.

## HP Tools / System RPL toolchain

HP Tools has a separate historical provenance and must be audited separately
before any source is vendored.

Until that audit is complete, hpclang may:

- document the exact source-to-object transformations required by historical
  HP39/48/49 launchers;
- implement narrow independent encoders/packers for formats that are sufficiently
  understood;
- use external historical tools only in archaeology/comparison tests, never as
  a required production dependency.

## Clean-room implementation discipline

For new runtime/platform code:

1. record the externally visible contract first;
2. write tests/fixtures for the contract;
3. write a fresh implementation in hpclang;
4. compare generated structures and hardware behavior;
5. do not mechanically translate historical implementation source.

Examples already following this direction include:

- the ROPI/RWPI Launcher V3 MMU loader;
- the program-header based ELF-to-L3 packer;
- target capability definitions.

## Repository goal

A normal hpclang checkout should eventually contain only code that the project
has the right to redistribute and build, while historical HPGCC archives remain
external research inputs.
