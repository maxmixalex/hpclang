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
- HP39g+/39gs/40gs: no generic HOME/current mapping is frozen. Stock exposes a
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

`hpclang_object_store_put()` copies an owned value into a logical directory.
Its mode selects upsert, create-only (`EEXIST` on a duplicate), or replace-only
(`ENOENT` on a missing name). `hpclang_object_store_remove()` deletes an exact
non-directory entry. Directory replacement/removal is rejected with `EISDIR`.
Caller-owned values, existing snapshots and retained Stock references keep their
old values. Store handles continue to identify paths, not physical addresses.

HP39g+/39gs/40gs instead use `<hpclang/variable.h>`:
`hpclang_variable_get/set()` resolve exact category/name bytes in the current
Aplet schema. Get returns an owned copy; set keeps caller ownership, invokes the
native descriptor's type predicate, and then dispatches native `STO`. Variables
are predefined, so these operations do not create a HOME namespace or expose
variable deletion. The native staging limit is 7000 nibbles, corresponding to
3495 string bytes. Oversize inputs fail with `E2BIG` before entering Stock;
native type errors map to `EINVAL`, and native memory exhaustion to `ENOSPC`.

These public mutation interfaces are under M7.3f acceptance testing. RPL stack
`push/pop/drop/replace` retain their existing HP48gII/49g+/50g Stock contract.

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

Classic stack allocations are bounded by the native arena fences and are reclaimed by Stock after return. The HP39 portal separately supports Stock garbage collection during native calls, as described below. Public APIs expose neither allocation blocks nor Saturn addresses.

### M7.3d — named object stores — read-only core closed

- logical root/current/child handles use length-delimited calculator-byte names rather than assuming UTF-8 paths;
- HP48gII/49g+/50g RPL directory resolution, independent enumeration, owned snapshots and named put/remove are implemented. Logical handles re-resolve names instead of retaining borrowed Saturn pointers across GC/moving allocation;
- Stock HP49g+/50g validation uses the real USEROB witness `X50NG-VV.FAT`, an empty DOCSTR published by the x50ng VVFAT environment. `HPCLANG.HP` is the FAT transfer/launcher file and is not a HOME/USEROB variable;
- HP39 `HP39AscA` / `HP39BinX` parsing is retained privately as external typed-transfer protocol support. ROM and HPGCC39 evidence show `HP39DIR.*` belongs to the Disk Drive/Conn3x transfer path, so it is not an internal HOME/root backend;
- HP39 Aplet, Program and Note storage is read through the calculator's native structures rather than a synthetic filesystem. Aplet snapshots follow HOME → `'apletdir` → `'Name` and preserve the native DORRP as an owned snapshot; Program and Note catalogs/bodies use the current `PGMCXT` / `NOTESCXT` directory bindings;
- HP39 Aplet variables are a separate descriptor-driven contract, not directory entries. Topic Type property `ApletVars# = EIGHT` supplies the current Aplet schema; each extended variable ID carries a 15-nibble recall/type/store descriptor. Runtime preserves genuine schema source references (for example MiniVar's `xNoteText` at `#2C812`) and recalls values generically by quoting that genuine extended ID into Stock and invoking Stock `@`, which follows descriptor word 0 including the supported-entry path. The portal enters through `GETPTR`, commits the final live RPL pointers through Stock `SAVPTR`, and restores the suspended `#8076B` runstream pointer before returning to ARM. Runtime #320 validates this path on both HP39gs and HP40gs: `NoteText` passes as the standard witness, each model reaches HPC1 PASS, `stock-hp39.json` passes, and the complete 52-frame keyboard progression returns cleanly;
- no synthetic HP39 HOME/current hierarchy is exposed merely for API symmetry. Generic named-store entry points remain `ENOSYS` where the Stock model has no matching namespace;
- historical HPGCC 1.1/2.0 `satdir.h` use is primarily directory inspection/search/recall. Real HPGCC39 applications also read native Note/object directories directly. hpclang therefore treats safe read-only access plus owned lifetime as the M7.3 parity baseline rather than requiring a new database-style abstraction;
- public named put/remove and HP39 variable get/set implement the write-side contract described above. Raw HPGCC3 mutation descriptors remain private; generic HP39 directory creation/deletion is not invented.

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
  it belongs to the clean-core M7.3f contract rather than the
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

### M7.3f public mutation and native-variable lifetime

The public APIs and acceptance payloads are implemented and locally validated.
Formal CI acceptance of this combined batch is the remaining closure gate;
see the [validation record](validation.md#m73f-final-implementation-batch).

#### Classic RPL writes

Put/remove re-resolve the logical directory immediately before mutation. The
production backend captures all 128 writable, non-aliased main-RAM page bases
under the launcher's interrupt mask. The operation then uses that captured
view, without calling Stock or consulting a changing mapping during commit.

Preflight validates directory structure, allocation/accounting, and every live
reference family before writing. Referenced old values are preserved in TEMPOB;
USEROB movement updates the data/return stacks, temporary environments,
TOUCHTAB, VGER groups, the system-variable table and launcher-held roots.
HPGCC3 `hplib/sat3/sat3_dir.c` provides the reference-family baseline. Native
AVMEM reservations are preserved rather than replaced with the physical gap.

Host tests exercise nested directories, interior references, moving reference
tables, malformed late roots and insufficient memory. Rejected preflight plans
leave the memory image unchanged. This is a validated stable-RAM commit model,
not rollback for an arbitrary callback backend that fails after writing.

The original nested-reference/failure audit is accepted through `71644639`.
Earlier cross-workflow evidence on `c5329a54` includes
[Object mutation](https://github.com/zyf722/hpclang/actions/runs/36981866894) and
[Runtime](https://github.com/zyf722/hpclang/actions/runs/36981866981).
The public API replaces the old private create/replace/remove entry points.

#### HP39 native calls

Variable access resolves the current `ApletVars#` schema and preserves its
actual extended ID. Reads invoke Stock `@`. Writes first invoke descriptor
word 1, the native type predicate, then quote the ID and invoke Stock `STO`.
The low-level store entry alone does not perform that type check.

One ERRSET/ERRTRAP boundary covers every portal operation, including schema
allocation. The normal result is a value plus a success marker; native errors
carry ERROR@ and may leave private operands. Cleanup measures the stack relative
to the current EDITLINE, so growth/shrinkage does not restore stale addresses.
The application retains owned input/output snapshots across all native calls.
Native setters keep their own domain rules; for example PageNum depends on the
Aplet's existing sketch pages. There is no generic rollback promise for arbitrary
custom-Aplet handler side effects.

The historical HPGCC39 launcher exposes free Saturn RAM as ARM pages without
making those pages native TEMP objects. Before the first portal call, hpclang
reserves that interval as an opaque rooted TEMP string and retains the existing
TEMP prefix. This keeps the ARM code/data physically stationary while Stock
allocates and collects temporary objects. Original caller roots stay live, and
the private roots are released before returning to Stock.

A retained outer object is not sufficient for GC safety: live embedded RPL
streams must also lead to the TEMP block's end when traversed by SKIPOB. The
portal supplies bounded DOCODE tails for its return stream, helpers, names and
staged values. This follows the invariant explained in
[Safe Garbage Collection](https://www.hpcalc.org/hp48/docs/programming/garbage2.txt);
the HP39 behavior is verified against the original Stock ROMs.

Two pool pages remain available to Stock allocation. Bootstrap C uses a separate
one-page stack; its measured C-frame bound plus assembly call overhead fits that
page. The PIC return code shares the unused half of the L2 workspace, whose
preservation is covered by the MMU unit test. No public image ABI or compiler
mode change is needed.

#### Acceptance scope

The focused Object mutation payload uses the public APIs:

- HP48gII/49g+/50g: upsert/create/replace/remove, nested paths, retained Stock
  references and owned snapshots, duplicate/missing-name rejection, unchanged
  cwd, and restoration of the transferred fixture;
- HP39g+/39gs/40gs: repeated growing/shrinking/empty NoteText writes until native
  GC occurs, byte-exact readback after releasing input snapshots, native type
  errors, oversized and missing-name rejection, REAL PageNum round trips,
  original-value/context restoration, and settled Stock return;
- Runtime independently retains its full service and 52-frame keyboard gates,
  with unsupported Direct variable access returning ENOSYS.

The focused workflow shares build, target profiles, assets, USB transport and
strict return verification with the existing harness. Payload PASS alone is
insufficient. The raw-address HPGCC adapter boundary remains documented in the
[adapter map](hpgcc-saturn-adapter.md).
