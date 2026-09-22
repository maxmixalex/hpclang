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

The first M7.3e adapter implements the application-level directory behavior:

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

The current bounded `saturn.h` slice contains operations that can be mapped
without exposing native addresses or weakening mutation guarantees:

| Historical interface | Clean backing | Status |
| --- | --- | --- |
| `sat_stack_init` / `sat_stack_exit` | source-compatible scope pair; launcher bias is already normalized by hpclang | implemented |
| `sat_stack_depth` | `hpclang_stack_depth` | implemented |
| `sat_stack_drop` | one bounded clean drop | implemented |
| `sat_stack_dropn` | one bounded multi-drop, returning the number actually removed | implemented |
| `sat_stack_push_string` | raw-byte DOCSTR constructor + `hpclang_stack_push` | implemented |
| `sat_pick_str`, `sat_pick_str_alloc` | owned peek + raw-byte DOCSTR copy | implemented |
| `sat_pop_str`, `sat_pop_str_alloc` | successful pick followed by clean drop | implemented |
| `_sat_stack_push_int`, `sat_stack_push_int` | historical textual integer formatting followed by `sat_stack_push_string` | implemented |
| `sat_stack_push_double` | historical `%f` textual formatting followed by `sat_stack_push_string` | implemented |
| `sat_push_zint_llong` | exact owned ZINT constructor + clean stack push | implemented |
| `sat_pop_zint_llong` | owned peek + exact `int64_t` conversion, then drop only after successful conversion | implemented |
| `sat_pop_real` | owned peek + classic REAL conversion, then drop only after successful conversion | implemented |
| `sat_push_char` | clean character constructor + stack push | implemented |

`hpclang_object_create_string()` was added to the canonical object layer as the
missing symmetric constructor for raw calculator-byte strings. It preserves
embedded zero bytes because the clean API has an explicit length; the historical
`sat_stack_push_string()` wrapper naturally remains C-string based.

The second safe slice adds `hpclang_object_create_integer_i64()`. It constructs
an owned classic DOINT/ZINT snapshot for the complete `int64_t` range and is the
exact inverse, over that range, of `hpclang_object_integer_i64()`. This is a
canonical capability rather than an HPGCC-only object hack; the compatibility
`push/pop_zint_llong` functions are thin wrappers above it.

### Important historical distinction: textual numbers vs ZINT

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
- `sat_push_zint_llong` / `sat_pop_zint_llong` are different: they exchange a
  real exact calculator ZINT object.

The complete 1.1 distribution's bundled examples reinforce this split. The
representative `examples/stack/stack.c` performs exact ZINT pop/compute/push,
pushes a String, queries stack depth, and exits the compatibility scope. String
push/pop and `sat_dir_rclobject` are also common bundled-example patterns. By
contrast, `swap`/`nip` and the textual int/double pushes are not reasons to
invent new canonical object kinds merely because their names exist in the
header.

### Deferred safe adapters

Some historical calls are useful but need one more clean capability rather than
a raw workaround:

- `sat_stack_swap` and `sat_stack_nip`: do **not** implement these as two
  sequential `stack_replace` calls. If the second replacement failed, the
  calculator stack could be left half-swapped. They wait for a transactional
  stack reorder primitive or another proven atomic implementation justified by
  a representative application.
- `sat_push_zint_array` and the list-building family: add them only if a real
  port needs arbitrary-precision construction or list assembly. The current
  exact `int64_t` ZINT path already covers the historical `LONGLONG` example
  without exposing the internal digit-array representation.
- pure conversion helpers such as `sat_convert_zint_array` and
  `sat_convert_double_real` can be compatibility utilities later; they do not
  justify exposing raw Saturn memory.

### Migration/Internal `saturn.h` area

The following families are intentionally outside the bounded adapter because
their public contract is a Saturn address, emulator mapping, raw memory block,
or native prologue record:

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
- `sat_check_type(level, prologue)` as a native-prologue contract rather than a
  semantic object-kind query.

This is also why the old decNumber/RPL-stack examples do not justify exposing
raw pointers merely to compile unchanged: their application-level requirement
should first be identified and expressed as an owned typed service. If the code
fundamentally manipulates Saturn addresses, it remains an explicit migration
case.

Application ports that genuinely need one of these operations should identify
the capability they are trying to achieve. A typed hpclang service can then be
added if that capability is generally useful; otherwise the code remains an
explicit low-level migration case.

## Validation contract

M7.3e validation is layered:

1. host unit tests exercise C-side cwd/path semantics, get vs recall, recursive
   search, REAL conversion, object token lifetime, stack depth/drop behavior,
   string pick/pop/push, historical textual int/double push formatting, exact
   ZINT construction/exchange including `INT64_MIN`/`INT64_MAX`, failed-type
   pop preservation, REAL pop type safety, CHAR push, and raw-byte string
   construction;
2. the unified ARM build proves the adapters link through the same hpclang v1
   runtime used by real calculator payloads;
3. Stock E2E is used only when it adds calculator-specific evidence. Re-running
   a pre-existing launcher failure merely to obtain a green badge is not a
   substitute for focused object/stack evidence.

The first adapter slice is green in Core #396 and Storage #122. The textual
numeric-push extension is green in Core #398, including host units, ARMv4T
hardware-validation build and HP39 package. The exact ZINT slice is covered by
Core #399: its host/unit stage and ARMv4T hardware-validation build pass on
commit `552a2557`; the packaging job is orthogonal to the ZINT object semantics.
Runtime #326 remains intentionally outside the closure claim because its red
Stock jobs fail in launcher/emulator paths rather than proving an adapter
semantic failure.

General named-object put/remove remains outside the HPGCC 1.1/2.0 read/search /
recall parity baseline.