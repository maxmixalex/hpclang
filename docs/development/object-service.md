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

These public mutation interfaces are accepted under M7.3f. RPL stack
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

The public APIs and acceptance payloads have passed local and formal CI
validation. M7.3f is complete; see the
[validation record](validation.md#m73f-final-implementation-batch).

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

The HP39 portal changes interrupt ownership in ARM assembly. Preparation and
restoration C code, including their Thumb entry/exit instructions, run with
IRQ/FIQ masked. Before resuming Stock, the assembly boundary restores the native
ARM stack and then the saved interrupt state; on return it masks interrupts
before entering C. Stock's user-mode IRQ return trampoline assumes ARM code,
so enabling interrupts inside a Thumb C epilogue can resume in the wrong
instruction set even when every saved code byte is intact.

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

## M7.3g — directory lifecycle

HP48gII/49g+/50g Stock applications can create a child directory with
`hpclang_object_store_mkdir()` and remove one with `hpclang_object_store_rmdir()`.
Names use the existing raw-byte, explicit-length contract, including the empty
name used by Hidden directories. Creation rejects every existing name with
`EEXIST`; it never overwrites a value or directory.

Removal requires an explicit mode:

- `HPCLANG_OBJECT_STORE_REMOVE_EMPTY` rejects nonempty directories with `ENOTEMPTY`;
- `HPCLANG_OBJECT_STORE_REMOVE_TREE` removes the entire child subtree;
- a non-directory returns `ENOTDIR`, a missing name `ENOENT`, and Stock cwd or
  any ancestor containing it `EBUSY`.

The existing ordinary-object `remove()` still rejects directories with `EISDIR`.
HP39 and Direct execution remain unsupported for this classic-RPL namespace.

Before writing, the mutation planner validates the removed subtree's links,
remaining directory geometry, live references and combined USEROB/TEMPOB space.
References to removed values retain a TEMP copy of the subtree with its relative
links intact. Expected allocation, malformed-tree and active-cwd failures leave
Stock memory unchanged. As with the existing mutation backend, unexpected I/O
failure after commit starts is not a rollback guarantee.

Logical handles name paths: they return `ENOENT` while the path is absent and
resolve a later directory created at the same path. Owned value and iterator
snapshots keep their prior contents independently of those handles.

Host tests cover nested/Hidden directory lifecycle, duplicate/type/nonempty
errors, cwd/ancestor protection, live interior-value retention, stale/recreated
handles, malformed links and memory exhaustion before writes. The focused
Object mutation payload creates its own nested tree through the public APIs,
retains a native value reference across recursive removal, checks active-cwd
rejection, removes empty directories and restores the original namespace before
requiring settled Stock return. M7.3g is accepted by the subsequent Object
mutation regressions, including main `3ad37ac6`; see the validation record.

## M7.3h — HP39 Aplet-owned data (accepted)

`<hpclang/aplet.h>` provides `hpclang_aplet_data_get()`,
`hpclang_aplet_data_set()` and `hpclang_aplet_data_clear()` for HP39 Stock
applications packaged with the supported HPGCC39 Aplet template. This is the
current Aplet's custom data channel, separate from predefined Aplet Variables.

Get returns an owned snapshot. Set makes an independent Stock-owned copy, so
the caller can release its input immediately. Clear restores the template's
initial empty list. The template commits the returned data when the Aplet is
exited; a successful set is not a promise that an interrupted native execution
has already saved its value. Set is bounded to 7000 native nibbles (3495 string
bytes), with `E2BIG` for larger values. Unsupported models, Direct execution and
unrecognized template callers return `ENOSYS`.

The runtime verifies the template's `_Action` instruction protocol before
accessing its data argument. Stock `TOTEMPOB` makes the copy, and the caller
frame is resolved again after allocation because garbage collection can move
both the frame and native objects. No fixed RAM address or Aplet title
identifies the data cell.

The frame resolver also follows the launcher's explicit bias metadata. When
the launcher string has already been hidden, the data argument is the top
cell; otherwise it follows the launcher string. The runtime verifies the
same `_Action` continuation in both cases, rather than guessing from the data
value's type.

Local HP39g+/39gs/40gs evidence covers growing, shrinking and empty strings across native
garbage collection, input release, clear, and settled return. Four successive
invocations, separated by Aplet exit and emulator restart, verified write,
read-and-clear, empty-and-write, and read-and-clear. The payload's lifecycle
contract checks the expected phase as well as PASS and settled return, so
reinitializing the data on every launch cannot pass the persistence gate.
The eight-invocation contract then saves a second instance through Stock SAVE.
Alternating between the two instances verifies independent writes and clears,
without lowering native stack requirements or dropping the GC stress checks.
All three HP39 models pass this contract locally. The expanded Reset contract
below also passes; formal CI acceptance is complete on `6a7f9a28`.

The historical template does not connect its reset entry by default: its
`_Reset` routine contains a data-clear operation, but `=Common.h` defines the
`StndReset` dispatch as `MINUSTHREE NOP`. A routine's presence in the source
does not establish that Stock invokes it. The public `clear()` operation
clears data directly; the packaged menu entry below supplies the UI path.

The package now supplies a native named Reset program and explicit Start/Reset
view entries. Stock's library asks for confirmation before running the named
program. That program selects the active Aplet context, calls the template's
`_Reset` entry (`INT_04`), and restores the program context. Start retains the
native ARM view. The SDK preparation validates the historical epilog structure
and adjusts its backward field link when adding these entries.

The mutation lifecycle adds cancellation, confirmed clear, relaunch and
two-instance isolation checks. HP39g+, HP39gs and HP40gs pass all 15 invocations
locally; CI acceptance is complete on `6a7f9a28`.

## M7.3i — composite objects and stack reorder (accepted)

The implementation provides owned classic complex values and homogeneous
REAL/COMPLEX numeric vectors and matrices. Arrays have rank one or two,
nonzero dimensions, and zero-based row-major indexing. Construction copies
each element; extraction and replacement return independent snapshots.
Replacement never edits its source. Native decimal components retain their
digits and exponent even when the value cannot be represented by a C `double`.
The explicit complex-to-double conversion reports range errors and changes
neither output on failure.

Heterogeneous lists support construction, indexing and splice into a new list.
Symbolic matrices use the native rectangular DOMATRIX representation and have
separate row/column accessors. Inputs remain valid after edits; extracted
children survive release of the parent. Native data-list and matrix references
are materialized while the native reader is valid, including lists inside tags.
Cycles and excessive nesting fail without returning a partial object. Internal
HP39 executable variable schemas keep their references for immediate descriptor
resolution and do not use this data-copy operation.

Identifiers distinguish global and local scope. Identifier names and tag labels
are explicit-length calculator bytes; constructing an identifier does not
define a variable. Binary values preserve their exact nibble width. Decimal
ZINT exchange preserves arbitrarily large integers within the object/memory
limits. Exact classic REAL exchange uses a sign, up-to-12-digit coefficient and
decimal exponent, so values outside C `double` range can be retained. These are
representation and exchange APIs, not arbitrary-precision arithmetic; separate
EREAL/LONGREAL conversions remain unsupported. Short copy buffers return
`ENOBUFS` without truncation.

`hpclang_stack_swap()` and `hpclang_stack_nip()` operate on the top two visible
classic Stock entries without allocating native objects. They validate stack
geometry, pointer reads and available-memory accounting before writing, and
preserve native stack reservations. This shares the mutation backend's mapped,
interrupt-masked commit contract, rather than promising rollback for arbitrary
failing write callbacks.

Boolean stack exchange uses genuine immutable SysRPL TRUE/FALSE objects. It
checks the ROM identity through the full read interface before modifying the
RAM-only stack mapping. Numeric 0 and 1 are not silently treated as Boolean
objects. The HPGCC adapters preserve `swap/nip`'s success/failure convention and
`sat_pushtruefalse`'s interpretation of any nonzero C value as TRUE.

The Objects workflow builds separate mutation, composite and native-catalog
payloads using the shared Stock harness. Suite family applicability is declared
in `tests/e2e/object-suites.json`; native catalogs run only on HP39 models.
Keeping the payloads separate leaves
enough calculator memory to save a second native Aplet while retaining the
mutation fixture's GC and oversized-write checks. The mutation payload has a
15-invocation contract including Reset; the composite payload retains the
eight-invocation persistence/isolation contract. The composite payload also
retains an owned nested value across native GC and verifies it after relaunch.

The composite contract passes locally on HP39g+, HP39gs and HP40gs.
HP48gII, HP49g+ and HP50g pass composite stack exchange, symbolic matrices,
60-digit ZINT round trips and the historical application flows.
The 47 host tests, formatting, lint and Clang analysis pass with the compatibility
adapters and historical-flow fixtures. M7.3h/i/j implementation and local
validation are complete; formal CI acceptance is complete on `6a7f9a28`.

Historical-flow source anchors are HPGCC 1.0.3
`examples/libtests/stack.c`, HPGCC 1.1 `examples/stack/rplstk.c`, HPGCC 2.0
`examples/stack.c`, and HPGCC3 `sat3.h`'s existing-object list-construction
example. The composite fixture re-expresses these workflows through clean
snapshots or the bounded adapter, checks their results and restores the caller's
stack depth. This is workflow evidence, not a claim that old raw-pointer source
or binaries run unchanged. HP39's original `_Entry`/`_Action`/`_Exit` protocol
is exercised by the native Aplet lifecycle tests.

## M7.3j — native catalogs (accepted)

`hpclang/catalog.h` exposes Program and Note catalogs on HP39g+/39gs/40gs Stock.
`list()` returns an owned list of global identifiers; `get()` returns an owned
string. Names are 1–19 raw calculator bytes. `put()` accepts string objects and
the existing UPSERT/CREATE_ONLY/REPLACE_ONLY modes. `remove()` reports missing
names with `ENOENT`. Program `Editline` is a reserved native record and cannot
be mutated (`EPERM`). These services do not define Aplet Variables or open a
classic HOME directory.

Writes use Stock's catalog save/delete routines. Program save updates the
apostrophe-prefixed source and the colon-prefixed compilation trampoline;
delete also removes the semicolon-prefixed editor record. Note operations
maintain the corresponding source/editor records. Source-only STO/PURGE would
leave stale native state. Syntax is checked when Stock runs a program, not by
the ARM API. The portal stages at most 7000 nibbles per object and reports
native allocation failure as `ENOSPC`; ARM snapshots remain independent.

The dedicated catalog fixture passes Program/Note create/read/list/replace/remove,
mode rejections, missing-name behavior, snapshot ownership and settled return
on HP39g+, HP39gs and HP40gs. Formal CI acceptance is complete on `6a7f9a28`.

`hpclang_aplet_list/clone/remove` expose the native Aplet catalog separately.
Names are 1–254 raw calculator bytes; list returns owned display-name strings.
Clone uses Stock's save operation, rejects existing destinations and preserves
the caller's selected Aplet, Program/Note contexts and current context. The
native operation selects its new copy, so the portal retains the original
contexts in GC-traced native bindings and restores them on success and error.
Remove rejects the active Aplet (`EBUSY`) and native defaults (`EPERM`). Large
clones can fail with `ENOSPC` while the ARM application's memory is reserved.
Deletion updates both native storage and Stock's cached Aplet display list;
the purge primitive alone leaves a stale menu entry. Existing owned name-list
snapshots remain independent of subsequent deletion.
The dedicated catalog fixture passes this complete public lifecycle on all
three HP39 models, including active/default protection and independent lists.

### Reviewed port limitations

On 2026-10-04 the maintainer accepted these individual M7.3j limitations:

| Namespace | Native model | Current limitation and retained work |
| --- | --- | --- |
| Port 0 | Main RAM, containing library/backup objects | Enumeration, owned reads and backup create/replace/remove have local Stock coverage, including three-launch persistence. Complete library mutation and relocation remain in M8.0. |
| Port 1 | Extended/banked RAM, containing library/backup objects | HP49g+/HP50g owned reads and backup/library creation pass local Stock, including relaunch and native RPL recall. Replacement/removal still require bank/reference handling. |
| Port 2 | Flash, containing library/backup objects | Owned backup/library reads and enumeration pass HP49g+/HP50g Stock across three launches. Backup creation uses validated erased tails with synchronous error cleanup; removal preserves the payload and clears its status only. Replacement appends before deleting the old status; reclamation and power-loss behavior remain open. |

HPGCC3 `libs_workspace/headers/sat3.h` documents these object types and memory
areas explicitly. None has HOME-style subdirectories. Acceptance of these
limits does not claim implemented port APIs or remove the follow-up goals in
the roadmap.

#### M8 native-port implementation

Native port implementation is scheduled under M8.0. Initial local Stock 2.15
fixtures on HP49g+ and HP50g store an exact integer through each of `:0:PORTZERO`,
`:1:PORTONE` and `:2:PORTTWO`; the resulting named DOBAK envelopes are captured
from RAM/Flash after the existing object-mutation application passes and returns.
The private decoder checks paired name lengths, contained-object extent,
checksum-object structure and the native nibble CRC. Host tests retain those
three independently Stock-generated encodings, truncation and corruption cases.
This establishes backup-format evidence, not completed public port services.
HP48gII's Port 0 produces the same backup encoding. All three local probe runs
retain the existing object-mutation PASS and settled native-return gates.

The bounded RAM-port iterator validates library/backup records and their CRCs,
copies names and library IDs into invocation-local metadata, and stops at the
native zero prologue. It has been checked against all three captured Port 0
chains, including HP48gII's library immediately followed by the new backup.
That adjacency exposed an older library-span bug: the length field already
includes the CRC, so adding four extra nibbles crosses into the next object.
The corrected parser agrees with the 2404-byte L275_48 transfer file (eight-byte
header plus 4792 object nibbles) and the native next-object address. All 74 host
tests and the three affected parser tests under ASan/UBSan pass locally.
Flash allocation records use the separate slot traversal described below;
their write transactions remain open. The RAM iterator must not be applied
to raw Flash records.

The initial `<hpclang/port.h>` API provides `hpclang_port_iterator_open/read/close`
and `hpclang_port_backup_get`. Iterators own their metadata; returned values are
independent `hpclang_object` snapshots released with `hpclang_object_release`.
Backup names are exact calculator bytes, while library metadata includes its
numeric identifier. `hpclang_port_library_get` copies a complete library by
number without installing, attaching or executing it. The current backend
reads classic Stock Port 0 and HP49g+/HP50g Stock Port 1/2.
Other valid port indices and unavailable runtime contexts return `ENOSYS`,
which describes this implementation rather than hardware absence. Invalid
indices return `EINVAL`, missing names `ENOENT`, damaged encodings `EILSEQ`,
and mapping failures `EIO`. Local Stock execution on HP48gII, HP49g+ and HP50g
verifies public enumeration and the exact integer backup read before the
existing object-mutation application passes and returns. Library retrieval
also passes a native HP48gII probe against the installed ARMToolbox library,
followed by the existing application and return gates. Host ownership/error
tests pass with ASan/UBSan, and Clang static analysis passes for the public backend.
The object-mutation suite now prepares `PORTZERO` through its versioned
`stock-setup.json`, enumerates it and checks its exact integer value. It also
reads installed libraries (including ARMToolbox on HP48gII). The same harness
and application pass locally on all three classic models. These initial reads
and create-only writes are integrated into main at `f36d1d80`. The further
mutation and persistence work below remains a local M8.0 extension.

`hpclang_port_backup_create` prepares a complete owned backup encoding before
changing native storage. Port 0 creation opens space at the HOME/port boundary
using the existing validated USEROB relocation machinery, writes the record,
then publishes the new port start. Existing port objects and libraries remain
at their original addresses. Exact duplicate names return `EEXIST`; native
space exhaustion returns `ENOSPC`. It never replaces an existing backup.

`hpclang_port_library_create(0, snapshot)` uses the same Port 0 space reservation
to store a complete owned library encoding. It validates the library format and
CRC before moving memory, rejects a duplicate library number in that port with
`EEXIST`, and leaves existing library records at their original addresses. The
input snapshot remains owned by the caller. A non-library value returns `EINVAL`;
Unsupported launch contexts return `ENOSYS`. Capability discovery
reports `HPCLANG_PORT_CREATE_LIBRARY` separately from backup creation.
This stores a record only: it does not register, attach or execute it. Library
replacement/removal, active-library lifecycle and relocation remain open.
The separate `port-library` fixture passes two Stock launches on HP48gII,
HP49g+ and HP50g, including original-system RCL into HOME between launches,
byte-for-byte owned readback, duplicate-ID refusal and settled return.

On HP49g+/HP50g, `hpclang_port_library_create(1, snapshot)` appends to ERAM using
the same validated chain and launcher-owned limit as backup creation. Existing
records stay at their addresses; the body and next terminator are prepared
before the new prologue is published. Space for the native six-nibble terminator
is included in the limit check. A missing ownership boundary returns `ENOSYS`;
insufficient owned space returns `ENOSPC`. The shared `port-library` fixture
passes both launches on HP49g+ and HP50g with independent native RCL witnesses
for both ports. Library registration, attachment and execution remain separate.

Port 2 creation on HP49g+/HP50g validates all six native banks and duplicate
names under one exclusive Flash mapping, then appends into erased tail space.
It preserves existing addresses and does not erase or reclaim deleted slots.
Returned write errors finish and verify a native deleted slot before releasing
the mapping and expected RAM record. Persistent hardware failure retains RAM
execution, as does a chip that never leaves BUSY; normal return and both exit
paths cannot bypass this synchronous operation. This does not guarantee
power-loss recovery.
New Flash backup envelopes round their complete allocation size to four bytes,
matching independent native STO captures across four backup-name lengths.
Existing halfword-sized SDK envelopes remain accepted for failed-write cleanup.
An internal library encoder also matches the native 43-nibble library record
captured on HP49g+ and HP50g; its unused padding remains erased. This encoder
supplies the record for public Port 2 library creation; registration remains open.
`hpclang_port_library_create(2, snapshot)` validates the full library and CRC,
checks library-ID uniqueness across all six user banks, and appends under the
same exclusive mapping and failed-write cleanup as backups. Existing libraries
and backups remain at their original addresses. The operation neither registers
nor attaches the library. Full media returns `ENOSPC` without reclamation.
The independent `port-library` client passes both Stock launches on HP49g+
and HP50g, checking original RPL witnesses for all three ports and duplicate-ID
refusal. Host coverage includes IDs with different titles, CRC/type rejection,
all-bank preflight, full media and cleanup at each programming failure point.

Library registration requires a separate relocation operation. Paired HP49g+
and HP50g Stock probes first create library 123 in Port 0 through the SDK, then
use native `:0:123 RCL 2 STO` between launches. The first launch finds no matching
system-catalog binding; the second finds one unbanked binding, a catalog count
increase of one and a TEMP-bottom increase of 16 nibbles. The probe stores its
first-launch measurements in HOME and checks the deltas in the second launch.
Consequently, registering a stored library cannot be implemented as merely
writing an ID into the current table: catalog growth must preserve the following
memory and its live references. This observation does not establish registration
or attachment support in the SDK, or safe replacement of an executing library.
The internal catalog insertion planner validates every existing record and
strict ID ordering before identifying the insertion slot and old table end.
It rejects unreadable tails even after finding an existing ID, and preserves
its output on rejection. Actual C execution against saved HP49g+/HP50g catalogs
matches the native position for ID 123 and detects its registered form. This is
read-only planning; memory reservation, relocation and registration are still
required before publishing a new catalog entry.
Paired saved states from the same native operation identify the intervening
region on HP49g+/HP50g: 17 ACPTR objects followed by graph storage. The graph
bases and pixel pointers include four five-nibble cells below the existing
SYSUPD scan (`8068D`, `80695`, `806D0`, `806D5`). All four advance by 16 nibbles
when the catalog grows. The Stock mutation backend now supplies those
model-validated display roots explicitly; the scanner reports their provenance
and includes them in relocation preflight/commit. Host tests cover one-time
updates, unchanged out-of-range values, unreadable cells and overlapping root
definitions. Actual C traversal matches all four changes in both paired states.
HP48gII display roots and complete catalog-growth relocation remain unverified.
The same paired operation updates 17 HOME attachment fields pointing to those
ACPTR objects. The relocation scanner now includes both pointer fields of each
bounded HOME attachment record, preserving its library ID. It processes them
through the old HOME root before SYSUPD updates that root, so moving HOME carries
the corrected fields. Actual C traversal matches all 17 native updates on both
HP49g+ and HP50g. Host coverage includes unreadable-field rejection without
writes, TEMP relocation, and growth/contraction where HOME and its referenced
object move together. Other embedded references and complete registration still
require their own validation.
The combined display/HOME-root changes also pass the two-launch HP50g Stock
library fixture: all three ports create and retain the library, original RPL
recalls each one, duplicate creation is refused, and return settles normally.
On both paired states, actual C root enumeration identifies 24 references into
the metadata interval following the catalog insertion point: 17 HOME attachment
fields, four display fields and three SYSUPD fields. Each matches the native
16-nibble relocation. A saved-state insertion simulation also matches the new
catalog entry, every subsequent catalog record, all 17 ACPTR objects and the
unchanged graph objects byte for byte; menu pixels change during native recall.
This establishes the metadata-prefix behavior for these probes. It does not yet
validate moving the TEMP allocations and return stack or updating references
embedded in their contents, and does not enable SDK registration.
The internal per-edge relocation planner now computes five-nibble patches for
absolute runstream references, unbanked ACPTRs and HOME attachment fields. Library
relative fields account for movement of both the field and its target, including
backward offsets. It rejects partial-field moves, address overflow, opaque
contents and unresolved banked ACPTRs. Child traversal and indexed ROM/Flash
resolution remain separate obligations; a no-patch result is not a graph-safety
claim. Actual C parsing and planning reproduce all 17 native HOME patches on
both paired models. TEMP comparison across these full application launches also
shows intervening allocations, so those snapshots do not establish a uniform
TEMP shift or complete registration safety.
An independent saved-state relocation probe now passes on HP49g+/HP50g: it
inserts the binding for library 123, moves the catalog suffix, metadata, TEMP
allocations and return stack by 16 nibbles, updates 44 enumerated root fields,
and adjusts AVMEM by the change in whole free stack cells. Native RCL then
produces the library witness, and a fresh application invocation verifies the
binding, catalog count and TEMP bottom before returning normally. The witness
program's native registration call is removed in both the probe and its
control; without the injected relocation, the control fails the missing-binding
check (`0x46410060`). This is evidence for the sampled suspended Stock states,
not for relocation while an ARM application owns pages or for arbitrary opaque
TEMP contents. Public registration remains unavailable until those ownership
and reference obligations are implemented and tested.
Live registration probing also exposed a separate return-address source:
HP49g+/HP50g Stock's Saturn instruction return stack contains eight ARM-stored
20-bit addresses at `SAT_CPU+0x978`, with a circular index at `+0x998`. These
are distinct from the RPL return stack in mapped main RAM. ROM push/pop code
masks pushed addresses, wraps the index modulo eight and clears popped slots.
The launcher now includes all eight address slots in its native roots, leaving
the index unchanged. The initial live HP50g probe passed its application checks
but crashed on return; updating these slots made the same registration,
native-RCL witness and second invocation pass. The same two-invocation HP49g+
probe passes using the launcher's published roots without a probe-side CPU fix.
This corrects root coverage;
it does not certify arbitrary opaque TEMP bodies or enable public registration.
The separate `port-flash-backup` client passes three independent launches on
HP49g+/HP50g Stock. Native RPL recalls the created value before each relaunch,
and the application consumes the witness then verifies duplicate refusal.
Normal return, `exit()` and `_Exit()` preserve the backup. Independent image
comparison confirms only the intended 30-byte allocation changed.
The current short-name regression uses `BB`: all three HP50g Stock launches
pass, and its 28-byte saved record matches native `987 :2:BB STO` byte for byte.
Port 2 removal uses a native tombstone: only the status byte changes from `53`
to `00`. Whole-port preflight rejects malformed banks and ambiguous duplicate
names before writing; missing names return `ENOENT`. Partial status writes are
completed while mapping and interrupt ownership remain held. Payload, extents
and existing pointers are unchanged, so deletion does not reclaim capacity.
Independent HP49g+/HP50g native `PURGE` probes confirm this behavior with a value
on the stack, an explicit Flash ACPTR, and a program deleting itself while
executing. This immediate deletion evidence does not establish reference safety
for compaction, space reclamation or library relocation.
The independent `port-flash-remove` client passes three Stock launches on both
models, consuming native absence/presence witnesses around deletion and
same-name recreation. Owned snapshots survive removal. Final Flash images
contain the two expected deleted records and no unrelated changes.
Port 2 replacement reuses whole-port preflight, reserves an erased tail, writes
and verifies the new backup, then tombstones the old one under the same mapping
lease. Native readers cannot observe the temporary duplicate-name interval.
Append failure discards only the attempted new record, preserving the old value;
after a verified new commit, old-status cleanup completes before return.
Existing payload addresses and references do not move. Missing names and full
media fail without deleting the old record. This synchronous ordering does not
provide power-loss recovery or reclaim the old allocation.
The separate `port-flash-replace` client passes three HP49g+/HP50g Stock launches.
Native RPL independently recalls the grown and shrunken integer values before
relaunch; owned snapshots retain the prior value. Normal return, `exit()` and
`_Exit()` pass. Saved images retain all three payloads and change no unrelated
Flash bytes. Host fault injection covers every programming position and full
media without sacrificing the old backup.
As with HOME mutation, the captured
RAM mapping must remain stable during commit; preflight does not promise
rollback for an artificial backend that fails writes midway through commit.
The three classic Stock models locally pass creation/readback of integer 987,
duplicate rejection, the subsequent HOME mutation suite and settled return.
Saved native RAM retains both the newly created backup and pre-existing port
objects. Independent local relaunch probes on all three classic models use
Stock Kermit commands to recall both backups through native RPL and store the
results in a native directory; the recorded values are 987 and 123. Resumed
USB enumeration requires restoring the saved interrupt-enable masks into the
protocol backend. A reusable native-RPL recall gate remains required; the
three-launch public-API gate below is a separate persistence witness.

`hpclang_port_backup_replace` and `hpclang_port_backup_remove` now support
existing exact names in Port 0. Missing names return `ENOENT`; replacement does
not implicitly create a missing record. Replacement can grow or shrink a backup.
References to its previous complete record, including interior value pointers,
are preserved in TEMP before mutation. The classic library-address table joins
the existing stack, environment and system reference scan; banked references
are distinguished from numerically equal main-RAM addresses. Operations requiring
relocation of a preceding library still return `ENOTSUP` while library code
alignment and active execution safety are being completed.

All three classic models locally pass grow/shrink/read/remove plus HOME mutation
and settled return. They also pass three independent Stock launches: create an
integer backup, read and replace it with a string after relaunch, then read and
remove the string after another relaunch. No reinstallation, reset or fixture
injection occurs between launches. Internal library-record creation preserves
existing record addresses and rejects duplicate library IDs; it does not yet
provide public library installation or attachment. Port 1 replacement/removal, Port 2 reclamation and full
library mutation remain M8.0 requirements.

Port 1 enumeration and owned reads use the HP49/50 physical ERAM window after
checking its live identity mapping. They do not switch Saturn banks or claim
the free tail as writable storage. Host tests cover packed-nibble bounds,
invalid mappings, backup/library snapshots and damaged records. HP49g+ and
HP50g Stock tests create `PORTONE` through native RPL before launching, then
enumerate it and verify integer 456 on all three lifecycle invocations while
Port 0 and HOME mutations run.

`hpclang_port_backup_create(1, ...)` now appends a validated record before the
launcher-provided ERAM ownership fence. Existing records and library addresses
do not move. The body, CRC and next terminator are prepared before publishing
the prologue. Insufficient unowned space returns `ENOSPC`; no native space is
borrowed from the application heap. Older launchers without the fence return
`ENOSYS`. HP49g+ and HP50g Stock create integer 789 and read it across subsequent
launches. Independent native-RPL probes then recall both that record and the
original 456 through Stock Kermit. Port 1 replacement/removal and library
mutation still require their own reference-preservation and relocation paths.

Native HP50g `RCL` probes distinguish stored records from recalled values:
both an integer and the nested list `{ 123 456 { 789 321 } }` become complete
main-RAM objects on the data stack. Native `PURGE` removes the list's Port 1
record while leaving the recalled list intact. A binary-transferred program
containing two integer literals also recalls as a byte-for-byte main-RAM copy.
This establishes copy semantics for those values, not for attached libraries, executing programs or
extended bank pointers; those cases still need independent relocation checks.

A native Port 1 program that purges its own backup still completes its two
remaining integer literals. A separate program halted through the calculator
UI retains a return-stack address inside a main-RAM copy of the program.
These probes cover ordinary program execution; library and other banked
references still require their own checks before public replacement/removal.
Internal ERAM edit primitives now validate complete backup chains, resize or
remove records within the launcher ownership fence, and refuse to move a
following library. Their caller must establish reference safety; they are not
yet connected to the public write API. Host/sanitizer tests cover growth,
shrinkage, same-size replacement, removal, CRC damage, I/O failure and bounds.

The local HP48gII fixture accepts Port 0 storage but reports native error `#A`
(Port Not Available) for `1 PVARS` and `2 PVARS`. Capability discovery must
preserve this difference from HP49g+/HP50g rather than fabricating three ports
on every model. HP39 Aplet/catalog namespaces remain separate.

Port 2 has a separate internal Flash-slot iterator. On the examined HP50g
Stock image, user banks have a `0xf0` bank type and slots begin at bank-relative
nibble `0x20a`. A slot contains a status byte, a five-nibble extent relative to
that extent field, and one metadata nibble before the ordinary backup/library
object. The metadata nibble must not be included in the extent. Live slots use
`0x53`; an erased `0xff` status ends traversal. Native `PURGE` changes only the
status byte to `0x00`, preserving the extent and stale payload. Deleted slots
are skipped without interpreting that payload.

The iterator validates live objects and CRCs within their slot boundaries;
unknown states, malformed extents and read failures leave the caller's cursor
and result unchanged. Host/sanitizer checks and captured native HP50g Flash
verify four existing libraries plus a newly stored integer 987, then the same
four libraries after native deletion. The public read backend checks the
physical Flash identity mapping and each user bank's type/number, then scans
all six banks. Metadata and objects are copied before returning; no borrowed
Flash pointers escape. Host tests cover later-bank objects, deleted entries,
missing mappings, corrupt bank headers and snapshot survival after mapping
loss. HP49g+ and HP50g local Stock acceptance passes all three lifecycle
invocations, reading native integer 987, enumerating its backup and library 275,
copying that library and rejecting a natively purged backup. Typed port discovery, erase/write and
power-loss behavior remain separate M8.0 requirements.

`hpclang_port_query()` exposes native medium, implemented operations and
launch-available operations without touching port contents. This separates
absent model ports from unavailable launch contexts and from unfinished write
services. It does not replace data/mapping checks in the actual service calls.

Internal reference traversal can resolve a bounded native ACPTR through
caller-validated ROM switch tables into a bank kind/index and relative nibble
offset. Source and ROM readers remain separate; unknown switches, malformed
tables and out-of-window targets leave output unchanged. Resolution does not
require the target allocation to be live: native HP49g+/HP50g pointers remain
directed at preserved payloads after `PURGE`. The C resolver also matches a
saved HP50g ERAM pointer. This is one component of reclamation safety; indexed
FlashPtr objects and executable references still require their own traversal.

FlashPtr decoding keeps its three-nibble selector and four-nibble table index
separate, consistent with [Rick Grevelle's original description](https://www.finseth.com/hpdata/hp49g.php).
Logical selectors are resolved by matching their live RAM configuration cells
against the physical-bank configuration table. Zero/unrecognized configurations
remain unresolved, and duplicate matches are rejected without changing output.
Selectors must never be used directly as physical-bank indices. The C mapper
matches the saved HP49g+/HP50g configuration tables; addresses are caller-supplied
and the tables must remain stable throughout lookup.
The internal ROM-table lookup takes the resolved physical bank, a caller-mapped reader and header location;
it validates the native `0F` identity and bounded entry count before reading a
target. It rejects user `F0` allocation chains and preserves output on failure
or absence. Fixed-ROM targets remain distinct from selected-bank addresses.
The C lookup matches 3,941 entries in each examined HP49g+/HP50g image. This does
not generalize those header locations or mappings to unexamined ROM versions:
selector 15 uses a different window and table format. Native probes on the two
examined models copy its 35-entry table under the original configuration code,
restore the prior configuration and return without changing Flash. Per-page
content comparisons identify the pages needed by the observed references;
ambiguous pages remain unavailable rather than being assigned guessed addresses.
The internal count-prefixed-table resolver accepts caller-validated mappings
and bounds without requiring an ordinary `0F` header. The ordinary lookup still
requires that header. C lookup matches all 35 captured entries and the missing
end index. These results do not generalize the mapping to other firmware or
establish executable-reference and relocation safety.
See the API reference for the operation masks and descriptor version contract.

## Tagged object traversal

The internal `hpclang_rpl_tagged_visit()` validates a DOTAG extent and exposes
its single inline child, including the child's prologue. Label bytes are not
references; nested ACPTR targets remain separate traversal work. Like the
sequence and array visitors, it requires a stable reader and a read-only
callback. Extent validation does not certify opaque payload contents or prove
that an allocation is safe to reclaim.

## Library catalog traversal

`hpclang_rpl_catalog_visit()` enumerates every bounded native catalog record,
preserving its library ID, address, bank-switch entry and flags. This supplements
`hpclang_rpl_mutation_visit_roots()`, whose relocation scan intentionally skips
banked library addresses. Catalog traversal retains duplicate IDs so reference
analysis cannot silently discard a live binding; ID lookup still rejects an
ambiguous match. Callers must resolve and validate the selected mapping before
reading a library, and discard partial traversal after an error. The actual C
visitor matches all 62 entries, including 56 banked bindings, in the examined
HP50g Stock catalog. This is catalog coverage, not complete execution-root or
reclamation safety.

## Port allocation ownership

The internal `hpclang_rpl_port_find()` maps an interior RAM-port address to its
enclosing backup or library, validating the complete chain and terminator before
returning a match. `hpclang_rpl_flash_allocation_find()` also retains deleted
Flash allocations and validates the entire chain and erased tail. Header and
padding addresses belong to their allocation; a deleted payload need not remain
a parseable object. Neither helper discards the bank identity supplied by its
caller or treats a found allocation as permission to reclaim it.

The C resolvers locate the same native 60-nibble Flash allocation from an ACPTR
before and after Stock PURGE on HP49g+/HP50g, preserving its deleted status.
The RAM resolver maps the examined HP50g ERAM ACPTR to the 52-nibble `PORTONE`
backup. Complete root traversal and execution-reference qualification remain
necessary before production relocation/reclamation can use these bounds.

## Root provenance

`hpclang_rpl_mutation_visit_root_sources()` enumerates the same cells as the
existing root visitor while preserving their origin: launcher, data stack,
return stack, TEMPENV, TOUCHTAB, VGER, system table or main-RAM library catalog.
This lets reference analysis distinguish execution positions from object-stack
entries without guessing from numeric addresses. The origin does not certify
the value's type: system/support tables can carry metadata, launcher roots can
be return positions, and stack values can be sentinels. No category is
automatically safe to skip. Banked library catalog entries still require the
separate catalog visitor and mapping resolution.

## Structural reference dispatch

`hpclang_rpl_reference_visit()` provides one read-only entry point for immediate
object edges. It delegates sequence, tag, array, backup and library validation
to the existing parsers. Inline objects and compact array bodies remain
distinct; a compact body carries its shared prologue and is read through a
temporary prefix without writing native memory. Linked aliases remain separate
events. Backup/library CRC failures stop traversal.

Runstream addresses, ACPTR address/switch pairs, ROM library/command indices and
logical FlashPtr selector/index pairs remain distinct unresolved references.
The caller must retain the reader's bank identity and perform mapping lookup.
The dispatcher does not recursively dereference targets or allocate graph
storage. A caller schedules children, bounds work and tracks visited nodes.

Directories expose bounded named children and both pointer fields in each HOME
library attachment, retaining the library number as provenance. Child spans
cannot overlap the next entry's metadata. Non-HOME header values other than
zero and the source-confirmed `7FF` marker produce explicit opaque events.
The saved HP50g HOME scan matches all 20 nonzero attachment pointers and finds
21 ACPTR edges, including the known ERAM reference.

Machine code and binary data produce explicit opaque events;
recognized but unsupported formats still fail. Opaque events are pending work,
not proof that an object contains no references. Callers discard partial events
after errors. Native checks match all 16 library-275 edges and their bounds,
Flash ACPTRs before/after PURGE on HP49g+/HP50g and the saved HP50g ERAM ACPTR.
The saved ROM graph also expands 74 compact array bodies through this C path;
314 code objects and 10 binary objects remain explicit opaque cases. Complete
execution traversal and the production mutation guard remain open. The HOME
scan still encounters 148 unsupported nodes; successful directory decoding
alone does not establish that reclamation is safe.
