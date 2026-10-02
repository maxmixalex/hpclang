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
the public HP48gII/49g+/50g named-store API as read-only, and HP39 has no
generic named-store put/remove API. The M7.3f work branch has private classic-RPL
create/replace bridges and a bounded HP39 same-size descriptor-store witness;
neither establishes a completed public mutation contract.
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
- public named put/remove is **not implemented yet**. It is not required to close the HPGCC 1.1/2.0 read/search/recall adapter baseline, but it remains planned M7 work in M7.3f. HP48gII/49g+/50g need failure-safe named put/remove above explicit logical store handles; HP39 needs a validated generic descriptor-store invocation path for Aplet variables. HPGCC3's later `sat3_sto*` / `sat3_purge*` family and the recovered HP39 descriptor store word are implementation evidence, not permission to expose their raw address/lifetime model.

### M7.3e — HPGCC adapters and closure — complete at the bounded parity baseline

The source-compatibility layer is now closed above the clean core rather than
inside it:

- high-level HPGCC 1.1/2.0 `satdir.h` C-side navigation, exact get, parent
  recall, recursive HOME search and REAL fetch are implemented on owned store
  handles/snapshots. Historical `SAT_OBJ_DSCR.addr` is deliberately an opaque
  compatibility token, not a Saturn address;
- the bounded `saturn.h` layer implements scope init/exit, depth, drop/dropn,
  String push/pick/pop, historical textual int/double pushes, exact ZINT
  push/pop, classic REAL push/pop, CHAR push, and `stdargs` conversion;
- `stdargs` and typed pops preflight through owned snapshots, so a type or
  allocation failure does not consume a partially converted calculator stack;
- the clean constructors `hpclang_object_create_string()`,
  `hpclang_object_create_integer_i64()` and
  `hpclang_object_create_real_f64()` provide the reusable typed side of those
  adapters without exposing native prologues or Saturn addresses;
- raw Saturn address/memory functions remain Migration/Internal. Native-BCD
  complex/vector/array helpers, raw list builders, and borrowed stack
  descriptors are not reintroduced merely to make the historical header
  numerically complete;
- `sat_stack_swap` / `sat_stack_nip` stay outside the bounded subset until a
  real port justifies a transactional reorder primitive. Their omission is an
  explicit compatibility classification. General named put/remove is different:
  it remains a planned clean-core capability in M7.3f rather than part of the
  bounded historical `saturn.h` / `satdir.h` adapter surface.

Validation progressed from Core #396 (first adapter slice) through Core #399
(exact ZINT), Core #400 (the bundled HPGCC `stack.c` flow), and Core #402/#404
(REAL, failure-safe `stdargs`, and the focused calculator witness). Storage
#125/#126 then kept the normal Stock storage/return gate green while the shared
stage-alias teardown race was made baseline-aware.

The final M7.3e read/adapter closure gate is **E2E / Runtime #338
(`35812005032`) on `ba2c0398`**. Build, all three Direct families, Stock
HP39gs/HP40gs, Stock HP48gII, Stock HP49g+/HP50g, and evidence publication all
pass. The focused Stock witness preserves the incoming stack depth while
round-tripping ZINT, REAL and String values and only exercises successful
`stdargs` when the incoming stack is already empty. The canonical core witness
also returns push/replace/peek/pop/drop mutation to the original depth.

HP48 uses a deliberately small transferred `ZZE2E` DOCSTR
(`"HPCLANG-E2E"`) for successful owned named-object snapshot validation; the
large launcher remains enumerable but is no longer abused as a memory-stress
fixture. HP39/40 launch timing is gated on actual lazy-fetch completion and then
uses sparse launch-only Stock-UI ENTER retries. Once the payload announces that
it has started, only still-pending launch-retry key-downs are discarded; normal
sequence input remains available for Runtime/Gallery navigation. Those harness
fixes close launch/return races without changing object semantics.

Malformed/truncated/overflow object layouts and failed typed-pop/stdargs
preservation remain covered by host units, while Runtime #338 supplies the
cross-family Stock round-trip and clean-return evidence. This closes
**M7.3a-e** at the intended HPGCC 1.1/2.0 read/search/recall/stack-exchange
baseline.

### M7.3f — named object-store mutation — resumed, not accepted

M7.3f finishes the write side that was intentionally left out while the read
model and compatibility boundary were being proven:

- HP48gII/49g+/50g: add named put/replace/remove through explicit logical store
  handles, with owned input snapshots and no Stock-global cwd mutation;
- HP39gs/40gs: validate the descriptor store word as a generic variable-write
  bridge and preserve the native Aplet/topic context instead of inventing a
  synthetic HOME hierarchy;
- make create/replace/remove failure-safe, with bounded allocation/lifetime
  handling and original-Stock round-trip evidence;
- keep raw Saturn addresses and HPGCC3 native mutation descriptors private.

#### Restart review (2026-09-28)

`m7-object-parity` incorporates accepted main `8cdaf130`.
The existing ownership and namespace direction remains the intended design;
the private mutation implementation still needs lifecycle and failure evidence.

Current implementation and evidence have different scopes:

- Classic-RPL private bridges can create and replace named entries, including
  size changes. The lower-level remove primitive has host-unit coverage, but
  no object-store remove bridge or Stock deletion witness exists yet.
- The Stock mutation payload grows and restores an existing string, then
  creates `ZZM7F`. It does not remove that new entry. A payload PASS therefore
  proves neither a complete create/remove round trip nor clean Stock teardown.
- HP39gs/40gs have a private same-size descriptor-store bridge, using an owned
  snapshot staged in the portal page. Historical Runtime #358 validated the
  bounded `NoteText` witness; general size-changing writes remain unproven.
  This does not establish HP39g+ descriptor compatibility.
- Historical Object mutation #12 reached payload PASS on HP48gII but failed
  return teardown; canonical Runtime #371 regressed HP39gs/40gs before keyboard
  progression. These are unresolved historical symptoms, not diagnoses of the
  synchronized branch. Fresh CI must establish which remain reproducible.
- The old mutation test assumed the SD-created `X50NG-VV.FAT` entry on HP49/50.
  It now uses the shared USB harness's explicit `ZZE2E` string on all three
  classic-RPL models. Main's input timing, capture and USB launch code is kept.

The resumed baseline at `0d16cd2a` reproduced both failures: Runtime
[36388263936](https://github.com/zyf722/hpclang/actions/runs/36388263936)
failed all three HP39 Stock models before keyboard progression; Object mutation
[36388264008](https://github.com/zyf722/hpclang/actions/runs/36388264008)
reached payload PASS on HP48/49/50 but failed return. The follow-up separates
stage1 file bytes from its zero-initialized memory extent, moves the classic-RPL
stage0 return path into protected native storage, and registers launcher-held
Saturn addresses as relocation roots. At `bc80ac58`, canonical Runtime
[36509415719](https://github.com/zyf722/hpclang/actions/runs/36509415719)
and Object mutation
[36509415723](https://github.com/zyf722/hpclang/actions/runs/36509415723)
are fully green. Storage
[36509415707](https://github.com/zyf722/hpclang/actions/runs/36509415707)
still failed HP49g+/HP50g after payload PASS, during SD shutdown and return;
that regression requires separate acceptance. Local verification passed Stock Object mutation on HP48gII,
HP49g+ and HP50g with strict return checks, Runtime on all three HP39 Stock
models and the HP39 Direct family, and all 32 host unit tests. The HP48 runner
now starts from fresh emulator state and waits for the server-ready capture
before sending bytes. This does not close the missing remove bridge or general
failure transaction semantics listed below.

Work proceeds in this order:

1. Establish the synchronized baseline with canonical Runtime and the focused
   Object mutation workflow. Keep payload success, return control/state and
   visible Stock recovery as separate mandatory checks.
2. Audit USEROB movement, TEMPOB allocation, live reference relocation and
   launcher accounting against original HPGCC source evidence. The current
   manual relocation algorithm is a candidate implementation, not a frozen
   architecture. A missed reference root or unsupported object layout must
   fail before memory moves. newRPL can inform design but cannot prove Stock
   layout or calling conventions.
3. Close failure semantics before publishing API: current commit paths perform
   multiple writes and can return I/O errors after earlier writes succeeded.
   Preflight/ENOSPC unit coverage does not prove rollback after a late write
   failure. Establish a non-failing commit boundary or recoverable transaction,
   and test nested directories, retained references and rejected mutations.
4. Complete the named remove bridge and Stock create/read/replace/remove
   round trip, checking absence after deletion, existing-object restoration,
   unchanged cwd and owned-input lifetime. Preserve the failing teardown gate;
   do not relax it to accept payload PASS alone.
5. Generalize HP39 variable writes only after descriptor, context and allocation
   lifetime are established. Then freeze the clean public put/remove contract
   and remove obsolete same-size/probe-only bridges where no longer needed.

The focused mutation workflow is justified by its destructive object lifecycle
and lack of keyboard interaction. It shares the normal build entry, Stock asset
preparation, target profiles, USB runners and strict return verification.
Canonical Runtime remains the independent regression gate; the focused workflow
must not become a fork of the transport or a substitute for full Runtime.

The precise adapter/Migration split remains recorded in
[HPGCC Saturn / satdir adapter map](hpgcc-saturn-adapter.md). M7.3 as a whole
closes after this named write/delete slice is implemented and validated.
