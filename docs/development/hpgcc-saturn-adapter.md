# HPGCC Saturn / satdir adapter map

This note freezes the M7.3e compatibility boundary for the historical HPGCC
Saturn/object APIs. It is deliberately a **source adapter map**, not a promise
of historical binary layout or unrestricted Saturn-memory compatibility.

The canonical hpclang layer remains `hpclang_object`, `hpclang_stack_*` and
`hpclang_object_store_*`. Historical names sit above that layer and must not
change its ownership or failure-safety rules.

## Historical generations

The repository inventory of the complete distributions gives this public
surface progression:

| Generation | `saturn.h` | `satdir.h` | M7.3 consequence |
| --- | ---: | ---: | --- |
| HPGCC 1.0.3 beta | not in the inventoried public surface | not in the inventoried public surface | no compatibility baseline is inferred from 1.0.3 |
| HPGCC 1.1 / 39G-support tree | about 50 functions | 29 functions | primary source/behavior baseline |
| HPGCC 2.0 SP2 | 58 functions | 29 functions | 49/49 common `saturn.h` signatures and 29/29 `satdir.h` signatures remain exact from 1.1 |
| HPGCC3 R004.revA | replaced by the later `sat3_*` environment | replaced | design/mutation evidence only; not an hpclang ROMLIB ABI target |

The 1.1 and 2.0 `satdir.h` behavior is therefore stable enough to adapt as one
bounded source surface. HPGCC3 is intentionally not used to force its raw
lifetime/address model into hpclang.

## `satdir.h`: implemented high-level layer

The M7.3e adapter implements the application-level directory behavior:

| Historical interface | hpclang implementation | Status |
| --- | --- | --- |
| `sat_dir_open` / `sat_dir_close` | adapter-local lifetime above owned store handles/snapshots | implemented |
| `sat_dir_curdir` | adapter-local `HOME/...` C path | implemented |
| `sat_dir_chdir` | resolves `HOME`, `/`, relative components, `.` and `..` without changing Stock cwd | implemented |
| `sat_dir_getobject` | exact named lookup through `hpclang_object_store_get` | implemented |
| `sat_dir_rclobject` | relative lookup followed by parent-directory recall toward HOME | implemented |
| `sat_dir_searchobject` | recursive HOME traversal through independent clean iterators | implemented |
| `_sat_dir_destroy_slist` | releases adapter-owned search results | implemented |
| `sat_obj_is_real` | semantic kind query on the retained owned snapshot | implemented |
| `_sat_dir_fetchreal`, `sat_dir_getreal`, `sat_dir_rclreal` | classic REAL conversion through `hpclang_object_real_f64` | implemented |

Historical `SAT_OBJ_DSCR` is retained for source shape, but its `addr` member is
**not a Saturn address**. The adapter stores an opaque high-range token there;
the real object is an owned private `hpclang_object` snapshot. Code that treats
that field as an argument to `sat_peek`, `sat_map_s2a`, or another raw-address
routine is outside the bounded compatibility contract.

Legacy `satdir` paths are C strings, while the clean object-store API preserves
length-delimited calculator bytes. Names containing embedded NUL, `/`, or the
classic zero-length Hidden name cannot be represented losslessly as a legacy C
path. They remain available through the canonical raw-byte store API and are not
silently renamed by the adapter.

### `satdir.h` interfaces kept Migration/Internal

These historical calls fundamentally expose Saturn addresses or internal tree
records and therefore do not belong in the bounded public compatibility header:

- `sat_skipob`, `sat_objgetdir`;
- `sat_dircurrent`, `sat_objcurdir`, `sat_dirhome`, `sat_objhome`;
- `sat_dirfindfirst`, `sat_dirfindnext`, `sat_itemgetdir`;
- `sat_sknameup`, `sat_sknamedn`, `sat_objgetitem`, `sat_itemgetname`.

The old internal helpers `_sat_find_path`, `_sat_dir_path`, `_sat_dir_getroot`,
`_sat_dir_getcwd`, `_sat_dir_split_path`, and `_sat_dir_getprolog` are also not
an application ABI. Equivalent bookkeeping stays private to the adapter rather
than recreating HPGCC's borrowed-address directory tree.

## `saturn.h`: bounded stack/object subset

The bounded `saturn.h` slice now covers the representative high-level stack and
scalar exchange patterns from the complete 1.1/2.0 distributions without
exposing native addresses or weakening mutation guarantees:

| Historical interface | Clean backing | Status |
| --- | --- | --- |
| `sat_stack_init` / `sat_stack_exit` | source-compatible scope pair; launcher bias is already normalized by hpclang | implemented |
| `sat_stack_depth` | `hpclang_stack_depth` | implemented |
| `sat_stack_drop` | one bounded clean drop | implemented |
| `sat_stack_dropn` | one bounded multi-drop, returning the number actually removed | implemented |
| `sat_stack_push_string` | raw-byte DOCSTR constructor + `hpclang_stack_push` | implemented |
| `sat_pick_str`, `sat_pick_str_alloc` | owned peek + raw-byte DOCSTR copy | implemented |
| `sat_pop_str`, `sat_pop_str_alloc` | successful pick followed by clean drop | implemented |
| `sat_stack_pop_stdargs`, `_sat_stack_pop_stdargs` | preflight owned String snapshots, one-layer quote removal, then one bounded drop of the validated argument stack | implemented |
| `_sat_stack_push_int`, `sat_stack_push_int` | historical textual integer formatting followed by `sat_stack_push_string` | implemented |
| `sat_stack_push_double` | historical `%f` textual formatting followed by `sat_stack_push_string` | implemented |
| `sat_push_zint_llong` | exact owned ZINT constructor + clean stack push | implemented |
| `sat_pop_zint_llong` | owned peek + exact `int64_t` conversion, then drop only after successful conversion | implemented |
| `sat_push_real` | finite `double` → owned classic 12-digit DOREAL + clean stack push | implemented |
| `sat_pop_real` | owned peek + classic REAL conversion, then drop only after successful conversion | implemented |
| `sat_push_char` | clean character constructor + stack push | implemented |

`hpclang_object_create_string()` is the canonical symmetric constructor for raw
calculator-byte strings. It preserves embedded zero bytes because the clean API
has an explicit length; the historical `sat_stack_push_string()` wrapper
naturally remains C-string based.

`hpclang_object_create_integer_i64()` constructs an owned classic DOINT/ZINT
snapshot for the complete `int64_t` range and is the exact inverse, over that
range, of `hpclang_object_integer_i64()`. This is a canonical capability rather
than an HPGCC-only object hack; the compatibility `push/pop_zint_llong`
functions are thin wrappers above it.

`hpclang_object_create_real_f64()` provides the corresponding numeric REAL
constructor. It accepts finite C doubles, rounds to the classic DOREAL format's
12 significant decimal digits, preserves signed zero, and rejects NaN/infinity.
`sat_push_real()` is therefore a true calculator REAL push, while the historical
`sat_stack_push_double()` remains a textual String push.

### `stdargs`: historical shape, safer failure semantics

The HPGCC 2.0 source for `_sat_stack_pop_stdargs()` is high-level: it derives
`argc` from the current stack depth, pops String arguments, removes one matching
outer single-quote pair, and optionally supplies `argv[0]`. It does not require
Saturn addresses.

hpclang preserves that source-facing shape, including the historical absence of
a trailing `NULL` entry, but deliberately improves the failure ordering. The
adapter first owns and validates every String argument, removes quotes in-place
without returning an interior allocation pointer, and only then drops the
validated stack in one operation. A non-String argument or allocation failure
therefore leaves the calculator stack unchanged instead of partially consuming
it.

### Important historical distinction: textual numbers vs native numbers

The historical names are easy to misread. HPGCC 1.1 implements
`_sat_stack_push_int`, `sat_stack_push_int`, and `sat_stack_push_double` by
formatting the C value into a **String** and calling `sat_stack_push_string`.
They do not create calculator Integer/REAL objects. hpclang preserves that source
behavior rather than silently changing the object type:

- integer formatting accepts the historical radix argument and lower-case
  digits; `binary` adds `#...o`, `#...d`, or `#...h` only for radix 8/10/16;
- `sat_stack_push_double` retains historical `%f` text semantics, but uses a
  bounded formatter rather than reproducing the old fixed-buffer `sprintf`
  overflow hazard;
- `sat_push_zint_llong` / `sat_pop_zint_llong` exchange a real exact calculator
  ZINT object;
- `sat_push_real` / `sat_pop_real` exchange a real calculator DOREAL value.

The complete 1.1 distribution's bundled `examples/stack/stack.c` performs exact
ZINT pop/compute/push, pushes a String, queries stack depth, and exits the
compatibility scope. The 2.0 distribution also contains real `stdargs` clients
such as the sieve/lotto examples. These are stronger priorities than merely
matching every declaration in `saturn.h`.

## Deliberately not expanded further

The complete 2.0 SP2 source was checked before freezing the breadth of this
adapter. The remaining historical families are not treated as an automatic
backlog:

- `sat_stack_swap` directly rewrites the DSKTOP pointer table, and
  `sat_stack_nip` is implemented as swap+drop. A clean equivalent needs one
  transactional stack-reorder primitive; two sequential replaces would permit
  a half-swapped failure state. No bundled representative client currently
  justifies adding that primitive solely for compatibility.
- HPGCC list construction builds native nibble-form objects in a C heap buffer
  and later copies the representation into Saturn memory. A future typed list
  constructor can be added if an actual port needs list assembly; the old raw
  representation builder is not the model for the canonical API.
- `sat_push_bint` is safe in isolation but exposes a narrower 20-bit native
  integer object and has no meaningful bundled-client pressure. The exact ZINT
  constructor already supplies the useful general integer exchange capability.
- `sat_push_comp`, `sat_push_realvector`, and `sat_push_realarray` accept native
  calculator BCD words as their public data representation. They remain
  Migration/Internal rather than being mistaken for ordinary C complex/vector
  APIs.
- `sat_push_zint_array` and raw conversion helpers can be revisited if a real
  arbitrary-precision port requires them; their digit-array/native-form
  contracts do not justify widening the core pre-emptively.

This is an intentional compatibility boundary: representative application-level
behavior is adapted; raw representation and pointer-table behavior is not
reintroduced merely to increase function counts.

### Migration/Internal `saturn.h` area

The following families remain outside the bounded adapter because their public
contract is a Saturn address, emulator mapping, raw memory block, native
prologue, or native BCD representation:

- `sat_poke`, `sat_poke_nibbles`, `sat_peek`, `sat_moveup`;
- `sat_createtemp`, `sat_map_s2a`, `sat_peek_sat_bytes` and the historical
  `sat_peek_sat_*` / `sat_poke_sat_*` address macros;
- `sat_get_stack_element` because `SAT_STACK_ELEMENT.addr` is a borrowed Saturn
  address;
- `sat_pushptr` and the raw `sat_push_string(type_prolog, size_nibbles, data)`;
- `_sat_decode_string`, `sat_decode_stack_element` where the input descriptor is
  the raw-address historical representation;
- address-taking BCD/REAL helpers such as `sat_BCD2LONGLONG`, `sat_real2double`,
  `sat_decode_real`, and decNumber glue that expects raw Saturn object storage;
- raw-BCD complex/real-vector/real-array exchange;
- `sat_check_type(level, prologue)` as a native-prologue contract rather than a
  semantic object-kind query.

The old `rplstk`/decNumber examples therefore do not justify exposing raw
pointers merely to compile unchanged. Their application-level requirement
should first be identified and expressed as an owned typed service. If the code
fundamentally manipulates Saturn addresses, it remains an explicit migration
case.

## Validation contract

M7.3e validation is layered:

1. host unit tests exercise C-side cwd/path semantics, get vs recall, recursive
   search, object token lifetime, stack depth/drop behavior, String
   pick/pop/push, failure-safe `stdargs`, historical textual int/double push
   formatting, exact ZINT construction/exchange including `INT64_MIN` /
   `INT64_MAX`, classic REAL construction/exchange and rounding, failed-type
   pop preservation, CHAR push, raw-byte String construction, and malformed /
   truncated object extents;
2. unified ARM builds prove the adapters and focused calculator witness link
   through the same hpclang v1 runtime used by calculator payloads;
3. Stock E2E adds platform-specific evidence without manufacturing a special
   object model. The focused witness records incoming stack depth, exercises
   ZINT/REAL/String exchange, restores the exact depth after each operation,
   and executes successful `stdargs` only when the incoming stack is already
   empty.

Evidence progression:

- Core #396 + Storage #122: first `satdir` / safe `saturn` slice linked;
- Core #398: historical textual numeric String pushes;
- Core #399: exact owned ZINT constructor and push/pop exchange;
- Core #400: representative bundled HPGCC `stack.c` flow;
- Core #402: REAL constructor/push plus failure-safe `stdargs` host units and
  ARMv4T hardware-validation build;
- Core #404: focused ZINT/REAL/String/stdargs calculator payload build;
- Storage #125/#126: Stock HP49g+/HP50g remains green after the shared
  pre-PrRUN stage-baseline teardown race is fixed.

The final bounded-adapter gate is **E2E / Runtime #338
(`35812005032`) on `ba2c0398`**. Build, Direct hp39/hp48/hp4950, Stock
hp39, Stock hp48, Stock hp4950, and evidence publication all pass. HP48's
successful named-object snapshot uses the bounded `ZZE2E` DOCSTR rather than
requiring an owned copy of the roughly 74 KiB launcher. HP39/40 reaches the
payload only after the serial-disk lazy fetch has actually completed, with
sparse post-gate ENTER retries covering Stock's asynchronous UI/install
settling; both models then reach HPC1 PASS and clean Stock return.

Runtime #326 remains useful historical diagnostic evidence, but it is
superseded as an M7.3e closure gate. Its launcher/pre-HPC1 failures are not
treated as adapter semantics, and no repeated workflow rerun is needed to
replace the stronger focused evidence above.

The bounded source surface is therefore frozen for **M7.3e**. General named-object
put/remove remains outside the HPGCC 1.1/2.0 read/search/recall adapter
baseline, but it remains planned hpclang clean-core work in **M7.3f**.
Raw-address `satdir`, raw Saturn memory/BCD helpers, list builders, and
non-transactional stack reorders stay Migration/Internal or demand-driven
future compatibility work rather than requirements for M7.3f.
