# Calculator object service design

M7.3 adds calculator-object, RPL and stack services without treating native
calculator objects as byte-stream files or exposing raw Saturn addresses as
ordinary ARM pointers.

The historical HPGCC 2.0 SP2 Saturn sources separate three concerns:

- stack/object conversion helpers such as `sat_stack_*`, push/pop and
  encode/decode helpers;
- named-directory/object helpers such as `sat_dir_*`;
- raw Saturn memory helpers such as `sat_peek`, `sat_poke`,
  `sat_map_s2a` and `sat_memmove`.

hpclang makes the first two application capabilities available through a clean
object service. Raw memory/address operations remain runtime-private or
migration-only compatibility concerns.

## Public ownership model

The canonical public value is an opaque `struct hpclang_object`.

An object handle is an **owned immutable snapshot**:

- functions that read from the calculator stack or an object store return a new
  owned snapshot;
- the snapshot remains valid if the Stock stack, current directory or source
  object later changes;
- `hpclang_object_release()` releases only the ARM-side snapshot and never
  mutates the calculator;
- push/store functions copy from the snapshot back into the calculator rather
  than transferring the caller's handle;
- no borrowed Stock-memory pointer crosses the public ABI in M7.3;
- object snapshots are process-local and model-local. They are not a portable
  on-disk serialization format.

This intentionally prefers copying over borrowed aliases. ARM920T calculators
have limited memory, so later zero-copy/borrow APIs may be added only with an
explicit scoped lifetime if a real application requires them.

## Opaque-object boundary rule

The clean service does not need semantic knowledge of every Stock object before
it can preserve it, but it **must** be able to prove the object's storage
boundary.

A snapshot stores enough native representation to round-trip an object on the
same supported family. `hpclang_object_kind()` exposes a stable coarse class
when recognized. A semantically unknown object may still become
`HPCLANG_OBJECT_UNKNOWN` when its native format has a proven fixed,
length-prefixed, or otherwise verified boundary.

A completely unknown prologue whose object extent cannot be established is not
copied heuristically. Snapshot creation fails with `EILSEQ` or `ENOTSUP`
rather than risking a read into adjacent calculator objects. This keeps opaque
round-trip support lossless without turning raw prologue values into public API.

## Object classes

The initial stable classification is deliberately semantic and small:

- unknown/opaque;
- integer;
- real;
- complex;
- string;
- character;
- identifier;
- list;
- array/matrix;
- symbolic;
- program;
- directory;
- graphic;
- library;
- Aplet;
- Note.

Family-specific native type/prologue values stay private. HPGCC source adapters
may translate historical constants above this clean classification where
necessary.

## Stack contract

Where a platform has a real calculator object stack:

- `stack_depth` observes the current depth without mutation;
- `stack_peek(level)` returns an owned snapshot and leaves the stack untouched;
- `stack_push(object)` copies an object onto the calculator stack;
- `stack_pop()` is transactional: first make the owned snapshot, then remove
  the Stock object. If snapshot creation fails, the stack is unchanged;
- `stack_replace(level, object)` must not leave a half-written or partially
  shifted stack on failure;
- `stack_drop(count)` either performs the requested mutation through a
  validated platform operation or reports an error.

Levels are 1-based to match ordinary calculator stack terminology; level 1 is
the top object.

HP39-family models do not gain a synthetic RPL stack merely for API symmetry.
Unsupported stack calls return `ENOSYS`.

## Named object-store contract

Named objects use opaque store and iterator handles. The clean core never
changes the Stock calculator's global current-directory state as an implicit
side effect.

A store handle identifies an explicit namespace. The currently frozen mapping is:

- HP48gII/49g+/50g: HOME/current RPL directory or an explicitly opened child
  directory.
- HP39gs/40gs: no generic HOME/current mapping is frozen. Stock exposes a
  hierarchy of native catalogs/contexts rather than one HOME-like directory:
  the Aplet directory is the top-level catalog; Program and Note catalogs are
  reached through the current `PGMCXT` / `NOTESCXT` bindings; ordinary
  variables are context-local (for example Stock explicitly executes
  `DoInCalcCxt -> StoVar`). Generic named-store entry points therefore remain
  `ENOSYS` rather than inventing a synthetic hierarchy.

`HP39DIR.000`, `HP39DIR.CUR`, `HP39AscA` and `HP39BinX` belong to the
external typed transfer/disk protocol used by HP39/40G import/export. Their
private parsers are transfer-format support and must not define the internal
calculator object-store model.

Lookup returns owned snapshots. Enumeration owns independent iterator state.
Names are exposed as text only when the underlying Stock object model has a text
name; implementation-specific record addresses remain private.

The **contract** for a future named-store put operation is to copy from an owned
snapshot rather than transfer the caller's handle. That contract must not be
confused with current implementation status: M7.3 currently implements
HP48gII/49g+/50g named stores as read-only, and HP39 has no generic named-store
put/remove API. HP39 variable descriptors do contain native store-handler words,
but no general Stock invocation path for those handlers has been validated.
This is separate from RPL stack mutation: `stack_push`, `stack_pop`,
`stack_drop` and `stack_replace` are already implemented for the bounded
HP48gII/49g+/50g Stock stack path.

## Error and mutation rules

Public functions use the normal hpclang convention: success values plus
`errno` on failure.

The object service distinguishes at least:

- `ENOSYS`: capability does not exist on this family;
- `ENOENT`: named object is absent;
- `EINVAL`: malformed name, level or incompatible operation;
- `EILSEQ`: malformed native object representation;
- `ENOMEM`: no ARM-side memory for an owned snapshot;
- `ENOSPC`: calculator object store/temporary-object memory cannot accept a
  copy;
- `EROFS` / `EACCES`: protected namespace/object;
- `EBUSY`: Stock ownership/state makes a safe mutation unavailable.

A failed public mutation must not intentionally leave a partially updated stack
or namespace.

## Internal Saturn-address boundary

Runtime code may use an explicit private Saturn-nibble address type and the
launch-preserved `SAT_CPU->Read[]`/write mapping. Such values are not C
pointers and are never returned as canonical hpclang object handles.

The existing native-font backend already proves the read-map path on live Stock
HP48gII/49g+/50g launches. M7.3 factors the common nibble-map mechanics through
private helpers rather than duplicating `sat_map_s2a`-style arithmetic in each
object implementation.

Raw `peek/poke/map_s2a/memmove` compatibility, if provided at all, belongs
above or outside this clean object API.

## Implementation slices

### M7.3a — ownership and private Saturn view — implemented

- frozen owned-snapshot contract above;
- private explicit Saturn-address/read helper factored from the live `Read[]`
  path;
- owned object snapshot storage and coarse classification implemented without
  requiring public stack mutation.

### M7.3b — read-only stack/object inspection — implemented

- HP48gII/49g+/50g stack depth and owned peek;
- object size/prologue parsing with strict bounds and malformed-object errors;
- exact BINT/ZINT conversion, explicit classic DOREAL approximation, raw 8-bit CHAR/DOCSTR access.

### M7.3c — stack mutation — implemented for the bounded core

- push/pop/drop/replace with failure-safe ordering;
- launcher-derived native-page fences plus Launcher-V3 bias normalization/restoration;
- temporary-object allocation charged against existing Stock AVMEM reservations rather than raw geometry;
- self-contained Stock HP48/HP4950 create/push/replace/peek/pop/drop round-trip evidence.

Arbitrary TEMPOB reclamation/GC remains an internal lifecycle concern for future long-lived named-store mutation. The public stack path does not expose raw allocation blocks or Saturn addresses.

### M7.3d — named object stores — read-only core closed

- logical root/current/child handles use length-delimited calculator-byte names rather than assuming UTF-8 paths;
- HP48gII/49g+/50g read-only RPL directory resolution, independent enumeration and owned named-object snapshots are implemented. Logical handles re-resolve names instead of retaining borrowed Saturn pointers across GC/moving allocation;
- Stock HP49g+/50g validation uses the real USEROB witness `X50NG-VV.FAT`, an empty DOCSTR published by the x50ng VVFAT environment. `HPCLANG.HP` is the FAT transfer/launcher file and is not a HOME/USEROB variable;
- HP39 `HP39AscA` / `HP39BinX` parsing is retained privately as external typed-transfer protocol support. ROM and HPGCC39 evidence show `HP39DIR.*` belongs to the Disk Drive/Conn3x transfer path, so it is not an internal HOME/root backend;
- HP39 Aplet, Program and Note storage is read through the calculator's native structures rather than a synthetic filesystem. Aplet snapshots follow HOME → `'apletdir` → `'Name` and preserve the native DORRP as an owned snapshot; Program and Note catalogs/bodies use the current `PGMCXT` / `NOTESCXT` directory bindings;
- HP39 Aplet variables are a separate descriptor-driven contract, not directory entries. Topic Type property `ApletVars# = EIGHT` supplies the current Aplet schema; each extended variable ID carries a 15-nibble recall/type/store descriptor. Runtime preserves genuine schema source references (for example MiniVar's `xNoteText` at `#2C812`) and recalls values generically by quoting that genuine extended ID into Stock and invoking Stock `@`, which follows descriptor word 0 including the supported-entry path. The portal enters through `GETPTR`, commits the final live RPL pointers through Stock `SAVPTR`, and restores the suspended `#8076B` runstream pointer before returning to ARM. Runtime #320 validates this path on both HP39gs and HP40gs: `NoteText` passes as the standard witness, each model reaches HPC1 PASS, `stock-hp39.json` passes, and the complete 52-frame keyboard progression returns cleanly;
- no synthetic HP39 HOME/current hierarchy is exposed merely for API symmetry. Generic named-store entry points remain `ENOSYS` where the Stock model has no matching namespace;
- historical HPGCC 1.1/2.0 `satdir.h` use is primarily directory inspection/search/recall. Real HPGCC39 applications also read native Note/object directories directly. hpclang therefore treats safe read-only access plus owned lifetime as the M7.3 parity baseline rather than requiring a new database-style abstraction;
- named put/remove is **not implemented as a general M7.3 capability** and is not a prerequisite for HPGCC 1.1/2.0 capability parity. HPGCC3's later `sat3_sto*` / `sat3_purge*` family remains useful mutation evidence, but calculator-store mutation stays deferred until a concrete compatibility/application requirement justifies the additional lifecycle work. The presence of an HP39 descriptor store word is evidence about Stock's variable contract, not proof that hpclang already has a safe generic variable-write bridge.

### M7.3e — HPGCC adapters and closure — first adapter slice implemented

The first source-compatibility layer is now above the clean core rather than
inside it:

- high-level HPGCC 1.1/2.0 `satdir.h` C-side navigation, exact get, parent
  recall, recursive HOME search and REAL fetch are implemented on owned store
  handles/snapshots. Historical `SAT_OBJ_DSCR.addr` is deliberately an opaque
  compatibility token, not a Saturn address;
- the first safe `saturn.h` stack/object subset implements scope init/exit,
  depth, drop/dropn, C-string push/pick/pop, REAL pop and CHAR push through the
  canonical object/stack service;
- `hpclang_object_create_string()` adds the clean raw-calculator-byte DOCSTR
  constructor needed by string push while preserving explicit-length semantics
  in the canonical API;
- raw Saturn address/memory functions remain Migration/Internal. They are not
  reintroduced merely to make the historical header numerically complete;
- `sat_stack_swap`/`sat_stack_nip` are intentionally deferred until a
  transactional reorder primitive or equivalently failure-safe implementation
  exists; two sequential replacements would weaken the M7.3 mutation contract;
- Core #396 validates the code-bearing M7.3e head through host units, unified
  ARMv4T soft-float runtime/hardware-validation build, and HP39 package build.

The precise implemented/deferred/Migration split is recorded in
[HPGCC Saturn / satdir adapter map](hpgcc-saturn-adapter.md).

Remaining M7.3e closure work is targeted rather than another Saturn reverse:
add further high-value typed constructors/adapters only where the clean object
service can preserve ownership/failure semantics, and extend Stock round-trip
coverage for malformed objects, stack preservation and clean return. General
named-object mutation remains separately deferred until a concrete application
or compatibility requirement justifies it.
