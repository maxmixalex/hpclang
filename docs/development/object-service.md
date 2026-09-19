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

A store handle identifies an explicit namespace:

- HP48gII/49g+/50g: HOME/current RPL directory or an explicitly opened child
  directory;
- HP39gs/40gs: the native typed object-store namespace rooted in
  `HP39DIR.000` / `HP39DIR.CUR`, with Aplet/Zaplet, Note, Program,
  Variable, List, Matrix, Library and related records represented as native
  objects rather than files.

Lookup returns owned snapshots. Put copies from a snapshot. Enumeration owns
independent iterator state. Names are exposed as text only when the underlying
Stock object model has a text name; implementation-specific record addresses
remain private.

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
HP48gII/49g+/50g launches. M7.3 should factor the common nibble-map mechanics
into a private helper rather than duplicate `sat_map_s2a`-style arithmetic in
each object implementation.

Raw `peek/poke/map_s2a/memmove` compatibility, if provided at all, belongs
above or outside this clean object API.

## Implementation slices

### M7.3a — ownership and private Saturn view

- freeze the owned-snapshot contract above;
- factor a private explicit Saturn-address/read helper from the existing live
  `Read[]` path;
- add object snapshot storage and coarse classification without stack mutation.

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

### M7.3d — named object stores — next

- define logical root/current/child handles using length-delimited calculator-byte names rather than assuming UTF-8 paths;
- HP48/49/50 read-only RPL directory resolution and independent enumeration must re-resolve logical names rather than retaining borrowed Saturn addresses across GC/moving allocation;
- HP39 typed object-store backend maps Directory, Aplet/Zaplet, Note, Program, Variable, List, Matrix, Library and related native records into the same public store model;
- close read-only get/enumeration first; add put/remove only after USEROB/HP39 mutation lifecycles are independently safe.

### M7.3e — HPGCC adapters and closure

- bounded `saturn.h` / `satdir.h` source adapters over the clean core;
- raw address/memory helpers remain Migration/Internal unless a clean
  application requirement is demonstrated;
- Stock round-trip E2E including malformed objects, stack preservation and
  clean return.
