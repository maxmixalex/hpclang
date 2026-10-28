# SDK API design

M7 adds the application-facing calculator services that turn the validated HPC1 execution environment into a practical SDK. This document records durable design constraints; implementation status belongs primarily in [API reference](api-reference.md), [roadmap](roadmap.md), and [validation](validation.md).

## Goals

The application API should:

- work coherently across `hp39`, `hp48`, and `hp4950`;
- remain usable from C and modern C++;
- make model limitations discoverable rather than silently changing semantics;
- allow common HPGCC 1.x/2.x source to be ported with minimal mechanical work;
- avoid carrying historical compiler or binary-ABI constraints into new code;
- keep ownership, lifetime, units, and error handling explicit;
- remain small enough for machines with very limited RAM.

## Non-goals

The core API is not required to be binary-compatible with HPGCC executables, source-identical to every historical header, ABI-compatible with HPGCC3 ROMLIB, dependent on a replacement calculator ROM, POSIX-complete, or object-oriented internally.

Compatibility is primarily a **source-porting** concern.

## Three API layers

### 1. Stable C service ABI

The platform/runtime boundary and canonical public services use a C ABI. This layer owns primitive values, explicit buffers, opaque handles, fixed-width integers, and intentionally controlled structs. C++ standard-library types never cross this boundary.

### 2. Modern C++ wrappers

`hpclang/*.hpp` may provide zero-cost or low-cost wrappers around the C layer: RAII, `std::span`, strongly typed coordinates/durations, and `std::string_view` inputs where no C++ object becomes part of the platform ABI.

### 3. HPGCC source-compatibility layer

Historical names are implemented above the clean core, never the other way around. Each historical interface is classified as Direct, Adapter, Migration, or Unsupported. Binary compatibility is not a goal.

## Execution environment and hardware ownership

hpclang separates **service semantics** from **hardware ownership policy**.

The validated stock-hosted environment borrows the HP OS and must return control afterwards. Platform code should therefore preserve IRQ/timer/clock/display state where practical and restore temporary changes. A future hpclang-controlled bootloader or OS may legitimately own those peripherals while exposing the same application-facing services.

Exit cleanup must preserve that ownership without forcing unused services into
every application. CPU-profile ownership and tracked/standard-stream bindings
register private cleanup hooks when acquired. Dispatch remains separate from
the service implementations so ordinary object-only programs can discard the
unused clock-policy and FILE-management code at link time. Normal exit still
runs language callbacks before flushing streams; immediate exit skips those
steps while retaining hardware restoration. Memory validation includes copied
Aplets with both instances resident, not only single-instance launches.

Do not freeze a public execution-environment enum until code genuinely needs one. Do not make downstream firmware, operating systems, or applications named hpclang targets or dependencies.

## Core API rules

### Ownership and allocation

Low-level calls should not allocate unless allocation is the operation. Writing functions normally accept caller-provided buffers or explicit owned handles. Every owned resource needs an obvious release path.

### Errors

The C core should use explicit status values/output parameters for normal failure. Historical wrappers may translate those results into legacy conventions.

### Coordinates and display geometry

Do not hard-code 131×64 or 131×80 into common contracts. The physical display is a borrowed opaque surface with queryable geometry and intensity capability. Framebuffer address, stride, and bit packing remain private.

Drawing primitives take explicit intensity rather than a hidden mutable global graphics context. Historical HPG's `hpg_t` color/mode/font/pattern/clip state belongs in the compatibility adapter unless a non-HPG application demonstrates a clean-core need.

Owned 2/4/16-level software surfaces and representation-independent blit are supported without exposing packing.

### Physical grayscale and page flipping

Physical LCD mode switching is separate from software surfaces. Historical HPG changes controller mode and framebuffer addresses and relies on runtime teardown to restore them. A stock-hosted hpclang app cannot assume that ownership.

The S3C2410 controller scans a 160-pixel-wide line even though the visible calculator surface is 131 pixels. Historical mono/gray4/gray16 physical storage therefore uses 20/40/80 bytes per scan line, and buffers must be physically contiguous and aligned. HPC1's ordinary heap is assembled from independently mapped pages, so it is not a safe DMA allocation contract.

M7.5 implements this boundary with an on-demand display session. It allocates
two framebuffers, validates physical continuity across every mapping and the LCD's
4 MiB bank limit, and rolls back if either allocation is unsuitable. It does
not reserve framebuffer memory for applications that never acquire a mode.
Cache publication, stopped-DMA page exchange, bounded frame waiting and six
off-screen status segments stay behind the opaque surface/display API.

Direct E2E snapshots writable LCD configuration before HPC1 and compares it
before harness restoration. The display session restores Stock registers before
freeing its memory, both explicitly and during normal runtime shutdown. Frame
and status pixel oracles plus Stock return gates validate all six models;
physical-panel flicker/luminance remains a hardware measurement, not an emulator
claim. See [display ownership](api-reference.md#physical-display-ownership).

## Fonts and text

The font design has **two independent source classes** and a common rendering API.

### Calculator-native fonts

`hpclang_font_mini()`, HP48/49/50-only `hpclang_font_big()`, HP39g+/39gs/40gs-only `hpclang_font_editline()`, and the convenience `hpclang_font_default()` return borrowed descriptors over calculator-owned font assets. hpclang does not embed or redistribute HP ROM glyph bitmaps.

The implementation deliberately differs by platform family:

- **HP39/40:** native mini/main/EditLine rasters are fixed physical stock-ROM assets. They are read through the physical ARM flash view, not the current logical Saturn bank mapping. The mini table is proportional; main is 5×8/advance 6; EditLine is fixed 6×10/advance 6. HP39 does not expose the HP48-compatible `bigfont`.
- **HP48gII/49g+/50g:** native minifont/bigfont are stock Saturn objects resolved through the launch-provided logical `SAT_CPU->Read[]` map.

This difference is not an hpclang invention. Historical HPGCC's calculator-native helper path explicitly resolves the HP48-compatible minifont at Saturn `0x812c8` and bigfont object at `0x84d82` through `sat_map_s2a()`, whose implementation indexes `_saturn_cpu->Read[]`. The HPGCC HP39 Aplet/Library 275 support instead binds HP39 stock ROM text entries such as `Put3x5` and `Put5x7`.

The legacy atlas coordinates (`0x07E695` mini and `0x07C651` main) are internal reference coordinates, not universal physical ROM locations. HP39g+ D1.55 and HP39gs/HP40gs 2.22 relocate the tables differently; the runtime translates them through independently verified model-specific NOR windows. The [native-font API reference](api-reference.md#calculator-native-fonts) records those physical locations and pixel evidence. These coordinates must not be described as HPGCC constants.

This distinction is implementation-only. Applications receive the same opaque font interface and must not learn physical ROM addresses or `Read[]` internals.

### Generated custom fonts

Generated fonts use the checked-in bitmap-font catalog produced from the canonical source bundle. Normal application builds do not parse source font files or require Python/fontTools/Pillow.

The **canonical bundled asset set is bitmap-source only**. The generic TTF importer has been removed and is not part of the supported project surface. Candidate additions should prefer upstream BDF/PCF or another directly auditable bitmap source; any one-time deterministic derivation needed for a reviewed candidate belongs to source-package curation/provenance rather than normal SDK importing.

`hpclang_target_font()` supports exactly three repertoire modes:

- `STATIC_AUTO`: collect only code points from explicit `HPCLANG_UI_TEXT("...")` markers in the target's sources;
- `STATIC_EXPLICIT`: retain explicitly supplied code points plus the configured fallback;
- `FULL`: retain the selected face's complete mapped repertoire.

`HPCLANG_UI_TEXT` is an identity macro at runtime. It is build metadata, not a runtime font/string type. Ordinary literals, diagnostics, paths, format strings, comments, and fake marker text do not influence `STATIC_AUTO`.

Generated glyph/bitmap sections remain individually garbage-collectable. The wrapper ABI supports arbitrary metrics and faces; convenience aliases `@mini`, `@normal`, and `@large` are policy, not a restriction on the format.

### Text API and UI policy

The canonical renderer accepts code points and UTF-8. Invalid UTF-8 becomes U+FFFD and then the selected font's fallback when necessary. `hpclang_surface_draw_char()` remains a direct byte/U+0000..U+00FF compatibility entry point.

The core does not define semantic UI font roles. Historical native accessor names such as `mini`, `big`, and `editline` identify concrete ROM fonts only; a future UI toolkit may assign its own `text`, `caption`, `editor`, or other theme roles above them.

Font Gallery is deliberately a **separate HPC1 E2E application**. It exercises native fonts, generated aliases/arbitrary faces, metrics, representative repertoire, and both 64- and 80-line layouts without expanding the canonical runtime/keyboard/storage payload. Its specimen text is part of the visual test contract and now includes face-sensitive width, punctuation, accented Latin, digits/symbols, and supported CJK/Japanese/Korean samples rather than one weak generic phrase.

The Gallery evidence UI is also part of the test surface. Final M7.1 closure is `E2E / Font Gallery` **35298949457 / #112**. HP39gs/40gs Direct expose 245 generated faces + mini/main/EditLine for **248 entries / 496 captures**; HP48gII/49g+/50g retain 245 generated + minifont/bigfont for **247 / 494**. Stock hp39gs/hp40gs additionally verify the EditLine ROM fingerprint/pixels, while HP48gII/HP50g retain live-Saturn native evidence.

### HPG compatibility consequence

Historical HPG's bundled minifont/bigfont and caller-owned custom font descriptors are compatibility semantics, not the canonical hpclang asset model. An HPG adapter may retain the historical mutable font/context state and render through hpclang surfaces/fonts without exposing native ROM storage.

## Keyboard

The public keyboard core separates raw physical state from higher-level events.

A snapshot is eight bytes: bits 0–55 are the 8×7 matrix (`column * 7 + row`), bit 56 is ON, and bits 57–59 are raw GPF4/5/6 inputs whose labels vary by family. Matrix coordinates are physical scan positions, not portable meanings and not the historical HPGCC keymatrix numbering.

The stock-hosted backend synchronously polls GPIO and does not install a keyboard IRQ or background timer. The second layer debounces that state and emits DOWN/UP/REPEAT events with timestamp, modifiers, raw scan, and physical key identity. A freestanding backend may feed the same event contract from IRQs later.

All 51 physical positions pass the established Direct E2E on HP39g+, HP39gs, HP40gs, HP48gII, HP49g+, and HP50g. Model 1 is validated against its verified real-hardware D1.55 full-NOR fixture rather than inferred from HP39gs/40gs.

The service stops at physical key identity plus modifier state. Model/UI shortcuts and text/Alpha/Shift-plane translation belong above it.

Bounded text editing and numeric conversion live in `hpclang/input.h`, above the
physical event layer. The caller owns the buffer, event loop, rendering and
accept/cancel policy. An accept event does not commit a number: the application
converts the entire line with `hpclang_text_input_parse_int64/double`, handles
`EINVAL` or `ERANGE`, and can keep editing after a rejected value. The strict
ASCII decimal grammar and finite-double range policy are independent of any
calculator's Alpha/Shift mode. No terminal, Stock modal dialog, or global stdio
redirection is acquired by these helpers.

The optional synchronous line-session runner reuses the same caller-owned editor.
It copies callbacks for event acquisition, editing policy, validation and update
notifications, and borrows their context only until return. Capacity errors and
rejected values keep the session editable; input/render failures abort without
discarding the working line. The runner neither resets the physical event queue
nor selects an Alpha plane. An application can supply its own event source or
adapt the existing keyboard poller, and can redraw through the console or other
surface operations without acquiring implicit global screen state.

`hpclang_text_mode` is the optional model-specific policy layer over that event
stream. It owns pending Alpha/Shift and text locks in caller storage, leaving
physical modifiers unchanged. HP39 menu locks and classic double-Alpha locks
are distinct transitions. The result explicitly separates Latin case, symbols
and shifted commands so a renderer/editor cannot accidentally interpret every
shift as lowercase. Mode selection consumes a key independently of whether the
editor accepts its character; the application owns symbol/command dispatch,
accept/cancel and session lifetime. No general Stock keyboard shortcut table is
added to the physical-key API.

The console layer owns a bounded Unicode cell grid and drawing buffers. It
renders an explicit viewport onto a caller-selected surface and does not retain
that destination across page flips. Standard-I/O routing is a separate explicit
process-wide lease: callbacks are copied, their context is borrowed, and a second
binder fails with `EBUSY`. Input callbacks own any waiting/editing policy. Separate
stdin/stdout/stderr status and reset-on-rebind prevent an input EOF or pushed-back
byte from leaking between owners. Unbind flushes and releases the route even on
output error. Neither layer reconfigures the physical keyboard or acquires an
LCD mode; the application still owns event polling and presentation.

## Time

All time APIs name their unit. Monotonic microseconds form the basis for delays, deadlines, and event timing. The stock backend observes existing timer/RTC state instead of taking hardware ownership.

## Storage and stdio

Storage is split by **semantics**, not by media.

### Byte-stream files

HP49g+/HP50g expose the SD card as the hpclang byte-stream filesystem. The clean runtime uses one process-lifetime owned namespace:

1. before takeover, existing SD insertion/write-protect GPIO inputs are sampled without re-entering Stock KOS or changing pin configuration;
2. hpclang acquires the S3C2410 SDI controller;
3. the independently implemented block driver mounts the card through FatFs;
4. all standard stream I/O, metadata, capacity, rename/mkdir/rmdir and directory traversal use that same namespace until process shutdown;
5. shutdown closes remaining direct descriptors, unmounts FatFs, releases SDI ownership, invokes Stock KOS syscall 19 (`ResetSD`), and only then returns to the calculator OS.

Ordinary application I/O therefore does **not** alternate between KOS and FatFs. The earlier KOS-backed stream bridge established the required POSIX-facing behavior during bring-up, but it is no longer the production file backend.

The public surface remains normal libc plus typed hpclang queries. `hpclang_stream_path_query()` reports versioned FAT metadata and validity-gated civil timestamps without inventing timezone/epoch meaning. `hpclang_stream_storage_query()` reports media state, free bytes and total capacity from the owned backend. Repeated Stock `CHECKINSERT` is avoided because HP49g+ evidence showed it is not return-safe within one native launch. Before acquisition, GPIO sampling permits an absent-card query to be retried after insertion. During ownership, backend calls resample presence and write protection, and each block transfer checks the current state. Observed removal or a transfer error invalidates the card. A subsequent path operation can remount without returning controller ownership to Stock; old file and directory objects remain invalid (`EBADF`) and closing them discards their old buffers. Cached EOF and logical seeks also validate file identity. Libc may still return bytes already buffered before removal; no old buffered writes may reach a newly mounted volume. During ownership, the card-detect pin's EINT3 falling-edge latch also detects removal/reinsertion between software observations. The lease borrows and restores only that pin's function and interrupt trigger/mask/routing fields, consumes card notifications, and leaves other interrupt sources intact. Unsupported pin modes fail before takeover with `ENOTSUP`; unreadable media state during ownership invalidates the card. This extension is accepted on `cada0f2e`. Physical hot-plug validation remains a separate, nonblocking follow-up with a reproducible test package.

HP39/40 and HP48 do not pretend to expose this SD byte-stream service; unsupported operations fail with `ENOSYS`.

### Namespace ownership

The whole-process lease is the correctness boundary. Once hpclang has taken direct SD/FAT ownership, no live Stock-KOS file handle is allowed in the runtime and no filesystem operation goes back through KOS before shutdown. Independent FatFs `DIR` objects provide true iterator ownership, explicit paths avoid Stock global cwd mutation, and multiple iterators can coexist with ordinary stdio.

The selected FAT baseline is upstream FatFs R0.16 with official patch-1 and patch-2 (2026-07-10), configured for up to four primary FAT12/16/32 volumes on one SD card, UTF-8 LFN, `FF_FS_RPATH=0`, no exFAT/mkfs, and bounded memory. Explicit `N:/` paths select a discovered volume; unqualified paths select volume zero. Discovery preserves the actual MBR partition slot as metadata, while a raw FAT image exposes one volume. Media I/O is supplied by hpclang's S3C2410 SDI driver; historical HPGCC FSystem/SD-driver code remains behavioral evidence only.

The internal HP49g+/50g HOME/Port 0, Port 1 and Port 2 stores are **not FAT volumes**. They hold calculator/RPL objects and therefore belong to the typed-object service below rather than to `FILE *` or FatFs.

`hpclang_stream_media_identity_query()` returns an owned snapshot of the physical
card's CID through the same mounted namespace. Applications can compare CID
bytes across reinsertion without relying on a partition index or mutable FAT
serial/content. Every partition on a card has the same card identity. Matching
identity does not revive old file or directory objects, and failure leaves the
caller output unchanged. The emulator assigns a stable serial to each backing
file/directory inode, so ordinary writes preserve identity and A→B→A replacement
tests exercise different-card and returning-card observations independently of
stale-handle checks. Physical CID is not a cryptographic authentication token.

### Calculator objects

HP39gs/40gs have typed native calculator objects, but Stock does not present them as one HOME-like directory namespace. The internal model is context-oriented: the Aplet directory is the top-level catalog; Program and Note catalogs use the current `PGMCXT` / `NOTESCXT` bindings; ordinary variables live in whichever Aplet/topic context is active. The generic named-store mapping therefore remains unfrozen on HP39 instead of inventing a synthetic hierarchy.

Separately, `HP39DIR.000` / `HP39DIR.CUR`, `HP39AscA` and `HP39BinX` define the external typed Disk Drive/Conn3x transfer protocol. They are useful private transfer-format evidence but are not the calculator's internal object-store root. ROM entries such as `DoGetObFrSto`, `DoSendObToSto`, `OpenIO`, and `StdIOProc` belong to that transfer/I/O path.

HP39 internal object access uses the private Saturn/System-RPL portal with genuine native catalog/context primitives; ROM addresses and calling conventions remain private. The bridge snapshots Aplet (`GetApDirList`), Program (`CatPgm -> {}N`) and Note (`CatNot -> {}N`) catalogs as owned immutable lists while rolling the temporary RPL stack cell back before returning to HPC. Public Aplet services in `hpclang/aplet.h` and Program/Note services in `hpclang/catalog.h` are accepted on `6a7f9a28`. HP48/49/50 RPL/Saturn objects belong to the same owned-object service, with family-specific namespace semantics hidden below it.

## Saturn/RPL integration

The M7.3 ownership contract is frozen in [Calculator object service design](object-service.md).

Canonical public objects are opaque, owned, immutable snapshots. Reading from a calculator stack or named store copies into an ARM-side snapshot; bounded stack push/pop/drop/replace already copies through the clean mutation layer. Named-store put/remove and HP39 variable get/set use the same owned-copy rule in M7.3f. M7.3 deliberately exposes no borrowed Stock-memory alias. Unknown native objects may remain semantically unclassified while still round-tripping losslessly on the same family.

Saturn nibble addresses remain an explicit **private runtime type**, never ordinary C pointers and never canonical object handles. The launch-preserved `SAT_CPU->Read[]` path already used by HP48/49/50 native fonts becomes the common private read primitive; mutation support must use the corresponding validated Stock mapping/operation rather than publishing raw `peek/poke/map_s2a` behavior.

Stack mutations are failure-safe: peek is non-mutating; pop snapshots before dropping; replace/push/store must not intentionally leave a partially updated calculator state. Named object stores use explicit opaque handles and independent iterators rather than changing Stock global current-directory state.

Historical `saturn.h` / `satdir.h` APIs are adapters above this model. Raw address/memory helpers remain Migration/Internal unless a clean application requirement appears.

### Independent recovery envelope

`tools/package-recovery.py` is an internal HP49g+/HP50g Stock recovery packager. It
wraps a trusted position-independent ARM-entry binary in a self-copying RPL CODE
object without invoking Library 275. This is separate from the installed SDK's
HPC1 application launcher. Its current entry relies on the hp4950 main-RAM map;
other models are rejected until their entry boundaries are validated.

```sh
python3 tools/package-recovery.py --model hp50g --elf \
  --binary-output recovery.bin recovery.elf recovery.hp
```

ELF mode uses `llvm-nm`, `llvm-readelf` and `llvm-objcopy` from PATH (or explicit
`--nm`, `--readelf`, `--objcopy` paths). It checks the retained relocation kinds,
entry symbol and BSS/image bounds, derives the runtime extent from the ELF, and
extracts the initialized image into temporary storage before writing outputs.
`--binary-output` is optional. E2E uses this same tool; there is no separate
fixture-only packager. Its accepted ELF layout is the owned recovery layout,
not an arbitrary ARM program.

For a separately verified raw image, `--runtime-size SIZE` replaces `--elf`.
`SIZE` is the eight-byte-aligned total runtime image size, including initialized
bytes, BSS, workspace and the owned stack. The packager reserves those bytes
inside the CODE object, copies initialized bytes to an eight-byte-aligned
destination and returns to RPL without adding a stack result. The ARM payload
must initialize BSS, establish its stack, preserve hardware/native state and
return through LR. Raw mode requires separate rejection of unsafe relocations; a raw
binary cannot supply that proof. The complete program, including its original
initialized payload, reserved runtime copy and RPL framing, must fit within the
256 KiB Stock main-RAM window; oversized programs are rejected before encoding.
This physical ceiling does not deduct OS/other-object use. This envelope neither
allocates MMU pages nor proves sufficient installation memory. Installation-space budgeting and a
distributable recovery tool remain pending.

Recovery-time memory is a separate installation requirement. A local retained-RAM
fault experiment on both HP49g+ and HP50g stores a verified 26,000-element DOREAL
array after leaving USB transfer, leaving a 37,823-byte RPL stack/heap gap. The
existing bound configuration witness has a 46,698-byte transfer package (including
its eight-byte header). Following reset with an erased target sector, the observed
startup does not repair Flash or retire the armed journal/authority. A Stock-ROM
program counter alone does not establish successful startup. Less constrained
states recover, so successful fresh-state installation is insufficient admission
evidence. Before destructive work, the installed protocol must ensure recovery
execution storage remains available across restart, including retained user RAM;
passing the packager's physical ceiling does not provide that guarantee.

The optional `--flash-copy` configuration mode reduces this witness from 46,698 to
29,682 bytes by aligning the initialized image downward within each fresh RAM
copy, rather than retaining a second original image. All sixteen nibble
placements preserve the instruction prefix and image bounds. Both models pass
the existing configuration recovery checks (retained RAM, RAM loss, callback-body
damage and subsequent no-op warmstarts). Both also recover under the same
37,823-byte-gap condition above, preserving every element of the real array;
independent checks validate the catalog, complete Flash transition, code, stack,
TLS and guards. Its lifetime
contract requires a fresh copy from immutable Flash on every invocation: a
modified RAM copy cannot be reused as the initialized source. Production
integration must enforce that distinction and still guarantee recovery memory;
reducing the footprint does not establish arbitrary low-memory recovery.

`--flash-copy` requires `--library-id`; the resulting library must be installed
in Flash, not Port 0/RAM. The caller remains responsible for proving that
placement and the fresh-copy contract. `--program-output PATH` optionally emits
the configuration program for independent verification; it is not a reusable
RAM application when compact mode is selected. All input/output paths must be
distinct. Without `--flash-copy`, the original image remains available for
repeated RAM execution. The maintained configuration fixture builds both the
library and its companion program and checks low-memory recovery plus a later
unchanged restart, preserving the full real array. The existing Runtime artifact
glob includes the companion; no separate workflow or resource exception is needed.

The internal packager also accepts `--reservation-name NAME`,
`--reservation-bytes SIZE` and `--reservation-prefix TEXT`, supplied together
with `--library-id`. It generates a small configuration and six hidden commands;
the large recovery program is copied to RAM only after checking/releasing the
reservation. No calculator-side compiler or exported command names are needed.
The name is a non-space ASCII identifier, the prefix is opaque UTF-8 bytes, and
the exact string capacity must cover the recovery program while remaining below
the Stock main-RAM ceiling. The library keeps ordinary and compact program
lifetimes unchanged.

The guard checks the HOME object's string type, exact capacity and prefix using
real literals and `SAME`. A mismatch preserves the object and skips recovery;
an absent object permits normal recovery. The generated library passes local
HP49g+/HP50g low-memory recovery with a controlled 32 KiB string and a 20,898-byte
initial RPL gap, preserving all 24,000 user real-array elements and passing a
later unchanged restart. Comparable HP50g pressure without releasable storage
leaves the transaction armed and Flash unrepaired.
Instrumented native rejection checks on both models preserve wrong-type,
wrong-capacity and wrong-prefix objects with unchanged Flash; an independently
cleared/recreated completion marker proves the guard ran. The maintained
bound-configuration flow runs these scenarios in the existing Stock recovery
jobs and saves both generated packages. Synthetic busy reads exercise SDK
recovery, with a required nonzero operation count and exact read budget; Stock
uses its normal installation driver to construct the fixture first.

This is an internal packaging mechanism, not a trusted installer. Production
admission must bind the expected name/header, capacity and lifetime to the actual
installation root and current transaction before destructive writes. A fixed
name or matching header alone is insufficient. Release and recovery share one
configuration invocation; ordering against earlier system callbacks remains a
separate unresolved requirement. Capacity validation alone is not a guarantee
of all startup allocation requirements.
Creation must reject an existing reservation name even when its bytes exactly
match the planned reservation. The publication fixture uses create-only object
storage and checks this boundary before publishing either transaction record;
matching content never grants permission to adopt an existing user object.
The fixture also tracks attempted native RAM writes. An error after the first
attempt retains execution in an owned failure loop with interrupts disabled,
before any Flash publication; it cannot safely return an ordinary false
predicate after a possible directory/arena mutation. A separate fault build
injects a lost acknowledgement after the first actual RAM write. Its verifier
requires exact execution code and its owning CODE object, intact stack guards,
the owned failure PC, and zero Flash operations. This is failure containment,
not rollback or recovery after a restart.
The internal `hpclang_rpl_recovery_reservation_create` step now owns this
create-and-check boundary: it accepts a validated string snapshot, observes
native write attempts, resolves HOME again after creation moves it, and checks
every stored nibble before returning the new object's address and capacity.
Preflight errors leave the output unchanged; a failure after any write attempt,
including a failed or mismatched readback, returns `MUTATION_INCOMPLETE`.
The caller must retain execution and IRQ ownership in that case. The returned
address is useful only within the current exclusive invocation. It must not be
saved as a persistent identity across native allocation, garbage collection or
restart; durable installation/transaction binding remains required.

`hpclang_rpl_recovery_reservation_check_created` revalidates that original
receipt against HOME, the required hidden directory, exact address/capacity and
the complete untouched snapshot without changing RAM or consuming the receipt.
The installation publication witness now stages/retries an unarmed authority,
validates the installed staged transaction and rechecks the fresh receipt before
activating it. A separate negative witness changes the receipt capacity after
staging: it must retain execution with interrupts disabled and the authority
unarmed, preserving the reservation, unrelated HOME data, catalog, CODE and stack
guards. The receipt remains an exclusive-invocation capability; this sequence
does not reconstruct creation provenance after reset or authorize later reuse.

An internal draft `HPCOWN1` creation claim reserves 240 bytes as a native deleted
allocation (status and metadata zero), so Stock skips its payload. A separate
phase byte records intent (`7F`), verified creation (`7E`) and closure (`00`);
other subsets of `7E` are closing, and `FF` is incomplete. The immutable CRC
covers its physical slot, complete `HPCMEM2` binding, exact printable name and
canonical padding. The codec checks record/target/library/journal/authority
separation, name limits and exact expected binding. It stores no RAM address.
Offline native fixtures cover incomplete, intent, created, closing and closed
payloads; they are invisible to Stock and preserve Flash/catalog across restart.

The optional sealed admission root now binds the claim allocation and checks
the runtime record as described below. A production publisher remains open:
the exclusive producer must verify name absence before publishing intent and revalidate the
fresh creation receipt before publishing created. Cuts during either publication
need explicit retry/cancellation handling. A matching decoded record alone must
never authorize adoption, activation or RAM release, and its storage must not be
erased/reused while a bound root or transaction can survive. These remain open
integration requirements, including interrupted native RAM creation.

The internal `hpclang_rpl_flash_publish_creation_intent` primitive now writes a
validated claim into a caller-owned erased allocation. It verifies the immutable
body, publishes the native deleted envelope, and commits phase `7F` last.
`hpclang_rpl_flash_finish_creation_intent` retries that same owned attempt: phase
`FF` permits compatible partial writes; phase `7F` requires exact complete bytes
and performs no writes. Other phases and an occupied following end marker reject.
Neither entry establishes allocation ownership or reservation-name absence;
the installer must prove both and retain exclusion. A partial write retains
ownership and prohibits returning to Stock or starting RAM creation. Native
producer integration, cancellation and restart ownership remain required;
the receipt-gated CREATED transition is described below.
`hpclang_rpl_recovery_reservation_complete_creation` connects the original
creation receipt and untouched snapshot check to the durable CREATED transition.
It takes the name and binding from the trusted original INTENT, checks the full
RAM object and its header, then calls `hpclang_rpl_flash_complete_creation`.
The Flash primitive compares every immutable byte and permits only INTENT or
CREATED; it clears the single phase bit and verifies readback. A repeated CREATED
acknowledgement makes no writes. No RAM bytes or receipt fields change. All views
must remain exclusively owned in the same invocation; neither entry supplies
name-absence-before-intent ordering, restart provenance or startup safety.
The maintained `installation-creation` native witness exercises name-absence
checking, an exact fresh fixture tail, a driver-level partial INTENT write and
retry, native RAM creation, then receipt-gated CREATED and an idempotent repeat.
Its verifier independently constructs the complete expected Flash and checks
reservation contents, unrelated HOME data, native catalog, owning CODE, guards
and Flash busy accounting. The fixture's claim address is explicit and checked;
it is not yet a production claim-bound root installation or a power-loss test.
`hpclang_rpl_recovery_installation_staged` validates an exact unarmed authority
and READY journal without a sector scratch buffer. It checks native allocation
boundaries and root/catalog identity in both current and proposed Flash views;
staging never justifies damage to current Flash. The companion
`hpclang_rpl_recovery_installation_creation_staged` additionally requires the
exact CREATED claim, complete binding/journal CRC and its deleted allocation
boundary in both views. Both are read-only; original ownership, fresh receipt
validation and startup ordering remain separate activation prerequisites.
The native fixture stages its pair after creation, then invokes a separate small
admission program to reject a wrong transaction binding and accept the exact one.
Neither program activates the authority or erases target data.
The claim-bound real-template storage witness additionally publishes its READY
journal and staged authority after the root, following the four-extent plan in
one exclusive invocation. Every driver callback is bounded to the current owned
allocation; a publication failure retains the invocation. The claim allocation
remains erased and target data is unchanged. Its independent verifier reconstructs
all records at the planned coordinates and checks complete Flash before and after
Stock restart. This is unarmed installation staging, not resource creation,
activation, recovery ownership reconstruction or startup-order acceptance.
The subsequent creation-intent extension resolves HOME and checks name absence
before the first root write. After root and pair publication, it encodes HPCOWN1
at the fourth planned extent using the resealed identity and exact transaction
CRC. It stops at INTENT without creating RAM or arming the authority. The name
check and Flash publication share one exclusive invocation; returning or resetting
does not turn the claim into permission to adopt a later same-name object. The
maintained negative case supplies an existing string and requires rejection before
Flash programming, preserving that string and the input template.
The real-root resource extension continues within that same exclusive invocation:
it creates the 32 KiB string from an immutable bound snapshot, retains the fresh
receipt, and calls receipt-gated creation completion before returning. A repeated
completion must make no extra Flash writes. The authority remains staged (7F).
Native verification compares all resource bytes and the CREATED claim, and checks
that root, Flash and resource survive Stock restart. Persistence does not by itself
approve adopting a recreated resource or satisfy safe early startup/activation.
The witness reuses its owned, word-aligned preparation buffer for the immutable
resource snapshot after the root has been stored. The Flash library and original
HOME template remain separate and verified; no extra 32 KiB CODE
allocation is needed.
The installer must not execute inside movable USEROB while creating that HOME
resource. Its entry supplies the complete live image bounds, including stack and
buffers; before Flash publication the witness validates their physical mapping
and containment in one TEMP allocation, then uses those bounds as mutation
fences. The one-shot test retains the executable on the RPL stack while purging
only its HOME binding before evaluation. Original input templates are preserved.
This contract permits the tested USEROB creation without TEMP allocation or GC;
it is not permission to relocate the running image. Negative coverage executes
directly from USEROB and requires rejection with unchanged Flash. Stack guards
and the subsequent Stock-return check remain mandatory after successful creation.
`hpclang_rpl_recovery_installation_prepare_creation_in_place` explicitly accepts
an exclusively owned mutable template as both source and destination. It performs
the same namespace, four-extent, layout, identity, resealing and repeated placement
checks as copying preparation. Partial overlap and separate destinations are
rejected; the copying APIs continue to reject all aliases. A failed operation may
have changed the owned draft, which must then be discarded, while output geometry
and Flash remain unchanged. The real store witness aliases its source/prepared
buffers for this API, preserving the original HOME template separately. After the
root is stored, the same buffer becomes the immutable resource snapshot; validation
checks complete stored library bytes and snapshot contents at their respective
lifetimes rather than requiring both copies to remain resident simultaneously.
`hpclang_rpl_flash_activate_staged_authority` handles the later phase transition
without treating the record as an unfinished tail. It requires the exact READY
journal, complete authority body and phase 7F (or 7E for a lost acknowledgement),
and changes only the authority phase byte. An occupied successor is preserved.
The existing publication-retry interfaces still require an erased end marker.
This primitive does not replace installation admission, fresh receipt validation,
original ownership or safe startup checks; callers must establish those first.
`hpclang_rpl_flash_close_creation` is the final claim-record transition after
the caller has closed the transaction and safely released its resource under
uninterrupted ownership. It compares every immutable byte against the original
INTENT, accepts CREATED, partial CLOSING or already CLOSED, and clears only the
phase byte. INTENT, unknown phases and changed records reject without writes;
an occupied adjacent allocation is preserved. Failed or unconfirmed programming
retains ownership and permits a same-owned retry. This primitive does not inspect
RAM, authorize its removal, reconstruct ownership after reset or reclaim Flash.
The combined native cancellation witness exercises fresh INTENT/CREATED and
transaction staging before cancellation, releases only its original untouched
reservation, then closes the claim and repeats that closure without programming.
Its oracle requires exact Flash contents, a consumed receipt, absence of the
released variable, preservation of unrelated HOME data and the catalog, normal
Stock return and unchanged closed records on restart. This tests same-invocation
cleanup; it does not reconstruct the receipt across a native return or reset.
Real-root installation has a separate pre-registration boundary:
`hpclang_rpl_flash_cancel_staged_pair` accepts the original READY journal and the
coordinates of a complete pair published by the same exclusive invocation. It
checks both immutable bodies and the unchanged target, then finishes the journal
tombstone before clearing authority metadata and status. A partially cancelled
pair can be retried under that same ownership; armed or partially published
records, body mismatches and reversed cancellation order reject. This primitive
does not discover allocation ownership, inspect the root/catalog, release RAM or
authorize a restart adoption. The existing registered-library cancellation API
retains its native membership and exact root/catalog checks; both share the
ordered envelope commit implementation.
The internal packager accepts optional `--creation-claim-slot` for a guarded
admission ELF with a sealed identity. It patches only the aligned initialized
`hpclang_recovery_creation_slot` symbol, which must contain `UINT32_MAX`, and
rejects bank-header/end-marker overlap or overlap with journal/authority storage.
The optional layout field `admission_creation_nibble` includes the native
allocation envelope, like the other binding coordinates. The slot is patched
before sealing, so changing it changes the root digest; an identical placeholder
without that symbol is left untouched. Missing symbols/identity and invalid
geometry leave existing output files unchanged. This provides packaging support,
not native allocation ownership. Packaging the same existing images without
the option retains byte-identical output. Claim-aware placement and draft
preparation are described below.

The internal `hpclang_rpl_flash_plan_creation_space` planner searches erased
native tails for the library, journal, authority and 240-byte claim together.
If a three-extent placement leaves no room for the claim, it tries other bank
combinations. All extents exclude the target sector and do not overlap; outputs
change only on success. The original three-extent planner uses the same logic
without allocating a claim and preserves its selection order. Neither planner
reserves Flash.

`hpclang_rpl_recovery_installation_prepare_creation` applies that planning after
the existing cross-port/catalog namespace check. The caller supplies the trusted
layout's `admission_creation_nibble`, including the allocation envelope; the
field must not overlap identity or transaction descriptors. Preparation updates
the claim address before resealing, then revalidates all four placements. Source
and output coordinates remain unchanged on failure; staging is disposable.
This prepares bytes in RAM and does not publish a claim or establish ownership.
The maintained `installation-template-creation` witness uses the actual sealed
recovery/admission template and checks its entire rebound draft against the
independent packager. Both native models verify four erased-tail extents, source
and catalog preservation, CODE/stack guards and zero Flash writes. Ordinary and
claim-bound variants share the same build generator. Integrated root storage and
publication remain separate requirements.
The `installation-template-creation-store` variant uses the same selected-slot
writer. Its verifier decodes the stored claim coordinate, independently rebuilds
the library, checks all four extents and preserves every byte outside the library.
A subsequent Stock restart must discover the exact stored library. This stores
the root only; it does not publish the journal, authority or creation claim.
The internal `hpclang_rpl_recovery_installation_creation` reader revalidates
the installed transaction and journal CRC against the caller's trusted binding.
It requires the record to occupy an exact native deleted allocation boundary,
not an interior payload, and validates its full binding, name and phase. Pending
transactions use the validated restored view for allocation membership; closed
transactions use actual Flash. Outputs change only on success. Admission with
a sealed claim slot additionally requires CREATED before checking the RAM
reservation. An unbound placeholder preserves the existing admission path.
This is a read-only gate, not creation provenance or permission to release RAM.
Native tests build the claims offline; the production publisher, interrupted
creation and lifetime/reclamation integration remain required.

The internal 80-byte `HPCMEM2` payload header records the full sealed
`HPCROOT1` identity, physical library/journal/authority coordinates, target,
status-independent journal CRC and exact RAM capacity. It contains no RAM
pointer. The installation witness derives it from its validated initial plan
and desired capsule before fresh creation; the independent native verifier
reconstructs it from the installed library and original target image.
`hpclang_rpl_recovery_reservation_match` checks a native string's type, exact
capacity and all header bytes without writing. Neither the encoder nor the
matcher supplies ownership by itself. Durable publication must follow verified
fresh creation, prevent intervening native/user writes and retain the
installation's one-use lifetime; startup admission must verify that same
installation and active transaction before this header can authorize release.
The maintained automatic predicate now compares this binding after the sealed
installation/transaction preflight. Durable creation/publication proof and the
complete lifetime/startup-order protocol remain open.

An internal sealed-admission path addresses the circularity of an executing
library needing to recognize its own complete content. The packager locates an
owned `hpclang_recovery_identity` ELF symbol and fills its 48-byte `HPCROOT1`
descriptor: magic, native object span, descriptor nibble coordinate and SHA-256.
The digest covers canonical packed object nibbles with only its own digest and
the native CRC cleared; packaging then regenerates the native CRC. Descriptor
coordinates exclude the allocation envelope. No signature search is involved.
An optional owned `hpclang_recovery_library_id` ELF symbol reserves an aligned
four-byte `UINT32_MAX` placeholder. Library packaging fills it from `--library-id`
before sealing, so the executing predicate checks the requested library rather
than a compiled fixture number. The standalone admission artifact remains
unbound and rejects execution as an installed predicate. A misplaced symbol or
non-placeholder value is rejected before output files are written.

The owned `hpclang_recovery_reservation` symbol contains a 144-byte `HPCRESV1`
contract: magic, capacity, name length and a 128-byte name field. Packaging fills
it from the reservation options before sealing; standalone artifacts retain an
unbound placeholder. This admission requires `--reservation-prefix HPCMEM2`.
The RPL wrapper checks that prefix after ARM checks the complete binary binding.
The ARM phase rejects a different installation, physical location, transaction
CRC or capacity before either release or replay. An absent object permits the
normal recovery path without releasing RAM, preserving RAM-loss recovery.

`hpclang_rpl_recovery_installation_stream()` validates a unique live library,
native catalog/bank switch, complete sealed content, exact journal/authority
membership and all native bank allocation tails. Pending transactions supply
the restored sector view; closing/retiring transactions require actual target
equality, while closed transactions use current Flash. The recovery allocation
must lie wholly outside the target. It streams the journal instead of retaining
a sector-sized scratch buffer. The descriptor, transaction coordinates, catalog
and Flash mapping must come from one trusted, stable, exclusively held view.
Outputs change only on success; the operation neither writes Flash nor releases
RAM. Content identity is not authentication, installation provenance, a fresh
transaction claim or permission to release a matching HOME object.

The Runtime `identity` suite invokes the sealed admission command through its
installed native library and checks complete initialized code, fresh CODE
ownership, stack guards, mappings, catalog/Flash preservation and native return.
It also covers changed library content with a valid native CRC and constrained
memory with preserved user data. Tests that append a configuration completion
marker reseal the resulting library at the owned descriptor coordinate, so an
invalid digest cannot masquerade as correct rejection of another condition.
The maintained automatic admission fixture uses the same streaming check and
complete resource binding before the RPL reservation guard. Its stack is 4 KiB:
the earlier 2 KiB predicate stack
was insufficient for the combined entry, catalog scan and identity call chain.
This improves installation/transaction validation but does not replace the
separate ownership checks required before releasing a reservation.

The standalone `tests/e2e/port-recovery` witness builds its ARM entry, guarded
stack/TLS layout and actual shared sector-replay implementation entirely from
repository sources. With ATfE's `bin` on PATH:

```sh
cmake -S tests/e2e/port-recovery -B build/port-recovery \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/hpclang-armv4t.cmake" \
  -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build/port-recovery
```

The linker rejects unhandled sections rather than allowing the packaging step
to silently omit newly introduced data. Core CI builds this witness and checks linked relocation kinds, image entry and
BSS bounds before producing `recovery.hp`. The fixture records replay, mapping,
TLS and clock diagnostics for independent saved-state verification; its marker
alone is not acceptance. It keeps the preflight journal snapshot separate from
the replay read buffer, using the existing two-record workspace.
`hpclang_rpl_flash_sector_replay_expected()` requires the complete durable
journal to match that snapshot before touching the target; overlapping buffers
are rejected. This binds replay to the inspected image, without establishing
transaction freshness, reference safety or authority to reclaim native data.
The saved-state verifier checks both copies independently.
`tests/e2e/port-recovery/verify.py` compares complete
before/after Flash images against the CRC-checked sector journal, checks the
native empty stack and absence of Library 275, and verifies owned image/stack,
guards, TLS, mapping restoration, clock progression, repeat-write counts and
return to Stock ROM. It takes explicit saved-state paths, built ELF/binary/package
paths and journal/target addresses; it does not modify the saved state. Its host
tests reject collateral writes, incomplete replay, corrupt journals and truncated
state.

`tests/e2e/port-recovery/prepare.py` creates a separate fault-state directory
from a clean HP49g+/HP50g Stock boot or an installed recovery fixture. It validates
the six native bank identities, allocation chain and erased journal tail,
preserves the target sector in a CRC-protected journal,
then erases that sector in the copied Flash image. The original state and any
existing destination are preserved. The copied state must undergo hardware
reset before transfer so Stock rebuilds its catalog from the damaged Flash.
This is explicit offline fault injection, not a model of an erase/program pulse
interrupted while the chip is busy.
Targets may be aligned sectors anywhere inside the first user bank; the journal
and optional authority must fit at a real free allocation boundary, leave an
erased end marker and avoid the target sector. They may share its bank. Bound
replay scans the validated restored view so a damaged bank header does not hide
its own journal; scanner scratch cannot overwrite that image. Retiring phases
still compare the actual target before applying this view. This permits body-only faults that leave
the bank identity and earlier library headers intact. The configuration fixture
derives its Stock 2.15 library-229 callback sector from the saved catalog and
Flash header, verifies the expected bank mapping and rejects a target sharing
the bank/library header sector. It checks exact recovery, the complete catalog,
owned recovery state and an unchanged subsequent warmstart. Successful repair
does not by itself prove which earlier factory callbacks executed or were skipped.

`tests/e2e/port-recovery/run.py` orchestrates the fresh firmware boot, offline
fault preparation, hardware reset, USB transfer and independent verification.
The `--suite` option separates independent acceptance flows: `recovery` (the
default) tests replay and interruption; `identity` checks sealed admission;
`installation` covers lookup, staging and creation/cancellation fixtures;
`publication` covers resource conflicts, publication, repeat and cancellation;
`template` tests the real recovery library's preparation, storage and recovery;
`creation` tests its bound RAM creation, rejection and complete cancellation
lifecycle. Each suite starts with a fresh firmware boot and writes its own
evidence directory. CI runs all six on both models; splitting the flows does
not remove checks or require a saved state from another job.
The recovery suite then performs another hardware reset and compares the complete native library
catalog against the clean boot (IDs, bindings, bank switches and flags, independent
of ordering). The repaired Flash and retained journal must remain unchanged, the
stack empty and execution back in Stock ROM. This catches recovery that returns
once but leaves libraries missing or incorrectly bound on the next startup.
The optional `--interrupted-build` accepts a second witness built with
`-DHPCLANG_RECOVERY_STOP_BEFORE_IDENTITY=ON`. It stops before the bank-identity
program command, retaining IRQ/stack ownership in its failure loop. The verifier
requires the exact completed sector body with erased identity, intact initialized
code/guards/journal and the expected failure diagnostics. After hardware reset,
the native catalog must still match the damaged-bank baseline. The runner then
uploads the normal witness under a different name and retries recovery, retaining
both program objects. Core exercises this path on both supported models. This
is interruption between completed Flash commands, not a simulated busy-chip
power loss.

`--retirement-build` accepts a witness built with
`-DHPCLANG_RECOVERY_RETIRE_JOURNAL=ON`. After replay and catalog reconstruction,
it verifies the journal and target again, clears only the journal status, and
repeats retirement with no further writes. With more than one configured busy
read, the runner first interrupts that status write after clearing one selected
bit, verifies the entire Flash image and neighboring byte, resets Stock, then
resumes retirement. A partially retired journal only authorizes finishing its
status transition when the target already matches; it never authorizes another
erase or replay. Both resets must retain the clean catalog. A final invocation
reuses the installed witness and must report no pending journal, zero erase/
program calls and an unchanged complete Flash image. The Kermit helper supports
remote commands without uploading another copy, avoiding fixture memory growth.
The verifier selects `--operation replay`, `retire` or `check`; `check` validates
the known retired record and its target as well as the no-write invocation.
This is an owned
transaction fixture, not transaction-freshness proof for arbitrary journals or
an automatic startup-recovery implementation. CI also builds the configuration
mode described below alongside these three witness modes.

The retirement witness also exercises the documented
[`STARTUP` warmstart entry](https://hpcc.org/calculators/hp50g/hp50g_aur.pdf#page=617)
in a separate copy of the fresh damaged fixture. USB installation stores the
program without recalling or evaluating it; the complete Flash image must stay
unchanged before reset. The first warmstart must execute the owned recovery
entry, restore the sector and retire its journal. A second warmstart must find
no pending journal, perform zero erase/program commands, leave Flash unchanged
and retain the clean native library catalog and empty stack. Both invocations
use the same image/TLS/stack/mapping verifier and the configured Flash busy-read
model. This path passes locally on HP49g+ and HP50g.

This establishes automatic invocation for the fresh fixture, without Library
275. Production installation must still preserve an existing user `STARTUP`,
establish ordering relative to library configuration and other startup work,
and enforce transaction ownership/freshness before allowing native mutation.
The fixture does not install a recovery service into an existing user's HOME.

Native HP49g+/HP50g Stock 2.15 ordering probes install a temporary Port 0 library
whose configuration appends `C` to a HOME string, while `STARTUP` appends `S`.
After warmstart, independent RAM inspection finds `CS` on both models, the
installed library in the rebuilt catalog and execution back in Stock. Thus HOME
`STARTUP` alone cannot guarantee recovery before library configuration accesses
a damaged bank. A separate HP50g probe puts libraries 1701 and 1702 in Port 2:
both installation orders run 1701's configuration before 1702's, then `STARTUP`.
This is evidence for that pair, not a reserved earliest callback or a guarantee
across arbitrary library IDs, ports and firmware configurations.
Two further HP50g probes compare a Port 2 library numbered 769 with Port 0
libraries: 769 runs before 1702 but after 768. Stock accepts the latter even
though it is below the documented user-library range. A callback that appends
to a HOME string demonstrates the consequence of recovery's internal restart:
the earlier 768 callback executes twice during fault recovery, whereas the
later 1702 callback executes once. Both runs finish with the complete 63-entry
catalog and exact Flash repair/retirement. Choosing 769 alone therefore cannot
establish a safe production entry; earlier callbacks may have side effects or
access the damaged bank before recovery runs.
Further native probes install a Port 0 library numbered 768 before a Port 2
library numbered 4 (unused in these clean fixtures). Both HP49g+/HP50g run 4
before 768 and then `STARTUP` on two consecutive restarts, preserving the exact
catalog ID set and Flash. This demonstrates Stock acceptance of that earlier
entry, not a reserved ID policy or safety of the preceding system callbacks.
An existing ID must never be replaced merely to obtain early execution; the
installer still needs availability, firmware and complete startup-safety checks.
Production recovery also needs an entry that survives loss of HOME RAM; the
RAM-resident witness does not establish that property.

Ordinary Stock probes on HP49g+/HP50g evaluate configuration 2 while a child directory is current. They add a duplicate HOME attachment; the child header remains `0x7FF`.

The maintained `tests/e2e/port-library/configuration_audit.py` checks those two
snapshots and three additional HP49g+ executions: inline BINT 2, inline BINT 12
and the original ROM configuration-2 program. The library-2 cases add the same
duplicate HOME record. Library 12 adds `(12, 0, 550399)` to HOME, preserving
the other records, catalog, Flash, child header and sentinel with an empty stack.
All five audits and five deliberately wrong expected-record controls pass;
reports are in `/work/oct28-configuration-audit`. The exploratory scripts that
expected the child header to equal the library number failed their assertions;
these failures do not establish failed execution. This is ordinary Stock
configuration behavior, not startup safety or permission to move its references.

`configuration_references.py --audits /work/oct28-configuration-audit --output
/work/oct28-home-reference-maintained` compiles the current C reference scanner
and replays those five hash-bound snapshots. It matches all 109 nonzero HOME
attachment edges and skips 21 zero fields, while reading every nibble of each
zero field. All 105 injected unreadable-nibble cases return `UNAVAILABLE`;
partial visitor output is not accepted. These local checks need private native
snapshots and are not ordinary CI coverage or a complete relocation proof.


Supported-ID-769 fixtures bind the recorded recovery/admission ELF artifacts
through the maintained packager. Both HP49g+ and HP50g pass installation,
guarded low-memory repair, reset checkpoint verification, repeat restart,
absent-reservation recovery and all 16 rejection cases each. Guarded recovery
preserves 23,900 array elements; 21,702 free bytes admit the 20,994-byte admission
copy but not the recovery copy. Independent saved-state checks verify all 32
rejections preserve Flash and user data, complete the configuration witness and
issue no Flash operation or reset. HP50g's missing phases continue from reverified
checkpoints without replaying its completed recovery. Inputs, results and audits
remain under `/work/oct27-supported-recovery/<model>`.
The harness accepts an explicit library ID (default 1701), rejects unsupported
IDs before fixture creation and records the selected ID in new results. Its
ID769 witness packages match the retained native fixtures byte-for-byte. These
checks establish supported bindings, not complete pre-recovery callback safety.

The internal `hpclang_rpl_library_configuration()` resolver reads the configuration
offset from a validated library ID header in a caller-selected stable mapping.
It distinguishes a zero offset from read failures, malformed IDs and out-of-bounds
relative pointers; it neither executes nor classifies the callback as safe.
Native saved-state checks on both models resolve 17 callbacks and 45 absent
callbacks in the 62-entry fixture catalog when using the emulator state's observed
physical bank bases. Entry zero starts at `0x48000`, unlike the ordinary Flash
formula's `0x40000`. The latter fails ID-header validation for libraries 221, 223
and 257; earlier exploratory inventories skipped these bindings. With the observed
mapping, 221 and 223 have callbacks and 257 has none. This does not alter the
validated physical layout of user Port 2 banks. Native watchpoints on both models
observe Stock initialization of that bank array, followed by reads in the mapping
builder and library scanner, with normal Stock return. The initialization reads
the ROM directory's count and per-bank KiB sizes: a 96 KiB first bank followed by
thirteen 128 KiB banks, ending exactly at 2 MiB. Production capture must validate
the directory, mapping and bounds together; the private probe's state-relative
offset is not an SDK contract. Callback behavior/order remains a separate proof.
No numeric-ID or skipped-entry heuristic
provides permission to start a destructive transaction.

`hpclang_rpl_rom_bank_layout()` validates a stable captured size directory against
all sixteen native bank-base cells, including zeroed unused cells. Bank extents
come from the per-bank KiB sizes; the first bank starts immediately after the
directory and the final bank must end exactly at the supplied Flash limit.
Zero/oversized banks, mismatched bases, truncated coverage and overflow are errors,
including when the requested index is unused. Output changes only for a valid
populated bank. Callers must supply a firmware-verified directory location and
capture stable inputs; plausible bytes or a successful parse do not identify a ROM
or authorize recovery. Both native saved-state inventories use this parser and
the first bank's actual 96 KiB bound when resolving configuration entries.

The maintained installation witness exercises this capture on ARM, using the
supported Stock 2.15 fixture's verified initializer literal-pool addresses and
state layout. Its existing missing-library and matched-installation invocations
emit every catalog ID, library header, selected bank and resolved configuration
entry. The host reconstructs each row independently from the ROM directory and
saved catalog, while the installation verifier checks owned execution memory,
stack guards, restored mappings and unchanged Flash/catalog. An absent callback
is recorded only after successful header validation; an unsupported mapping or
failed inventory is rejected by the verifier. This gate does not certify callback
behavior or make fixture addresses a production firmware-discovery mechanism.

Journal integrity is separate from transaction authority. A host regression
keeps a valid ready journal and adds a valid backup inside its target sector.
Whole-port overlay preflight still accepts the old image, because that image
hides the later allocation. No erase is attempted: public write preparation
rejects the ready or partially retired journal without modifying any Flash byte.
Pinning the journal bytes alone cannot detect this stale-target case. Production
startup replay requires a durable transaction ownership protocol before this
internal recovery primitive can authorize writes outside a controlled fixture.

An HP50g prototype stores the owned recovery program as a hidden Port 2 library
command. Calling its ROMPTR after Stock startup passes the image/stack/TLS and
return checks with unchanged Flash and no journal. This establishes execution
from the installed library through a RAM-owned CODE copy. Configuration-time
probes both through that hidden command and with the program directly in the
configuration entry restore an injected damaged sector and retire its journal:
complete Flash images match the independent replay oracle with 64-read busy
intervals. Stock GC moves the containing CODE object, sometimes by an odd
nibble, before the final warmstart snapshot. The verifier's explicit
`--relocated-image` mode reconstructs its bytes and original execution address
from the saved errno pointer and ELF TLS layout, then checks original alignment,
RAM bounds, stack/TLS diagnostics, complete immutable image/guards and the
owning CODE envelope. Both paths pass; ordinary probes retain strict unmoved
alignment by default. This verifies this owned witness, not arbitrary active
ARM callers or complete native-reference repair.
The return-only configuration entry also passes these checks on HP49g+, but
leaves only 59 of the original 62 catalog entries on both models. A subsequent
warmstart restores all exact bindings. Production recovery must therefore
rebuild the catalog before dependent library/application code continues.
An HP50g test with that entry also clears the
complete 512 KiB external SRAM before reset, leaving the installed Flash library
and fault journal unchanged. After cold initialization completes, exact replay/retirement,
the relocated-image checks and Stock return pass. The initial 16-second capture
was still in firmware initialization; continuing that same state establishes
completion without reintroducing saved HOME data. This remains an isolated
fixture, not complete loss of every peripheral/on-chip state, production
installation, transaction freshness or physical battery removal acceptance.

The maintained runner includes a fresh Flash-configuration fixture with
`--configuration-build`. This witness enables retirement and
`HPCLANG_RECOVERY_RESTART_AFTER_REPAIR`: after successful retirement and Flash
mapping/interrupt restoration, a nonzero discovered journal slot requests Stock's reset
service. The resulting startup finds no pending journal and returns normally.
An unexpected return from the reset service stops the witness instead of
resuming an incomplete catalog. CMake rejects restart mode without retirement
or together with the interrupted witness. This is an internal recovery mode;
the general reset API remains deferred.

The host packager wraps the owned CODE program in a configuration-only native
library with no exported commands, hash table or command-link table. It checks
the program envelope and emits the native library length and CRC. The optional
`--library-id` accepts the documented user range 769..1791; it does not reserve
the ID, install recovery or establish callback priority. Stock stores the
packaged fixture in Port 2 and the transfer variable is removed from HOME;
calculator-side library compilation is unnecessary. A clean restart establishes its catalog baseline;
after fault injection, one host reset must finish with exact replay/retirement,
the complete catalog and a verified final no-op recovery image. No second host
reset may be needed to obtain the complete catalog. A later independent reset
then checks unchanged Flash/catalog and no repeated recovery. A HOME string
must survive every phase, distinguishing reinitialization from loss of user RAM.
Both HP49g+ and HP50g pass this maintained path locally, including the HOME value,
complete catalog, exact Flash transition and final no-op image verification.
The workflow builds this configuration mode alongside normal, interrupted and
returning-retirement witnesses. The library ID and temporary names
are fixture-local, not a production reservation or existing-user installation.
The synthetic busy-read model is enabled for the recovery warmstarts, after
Stock constructs the fixture: applying it to Stock's installation driver fails
in the emulator and is not an accepted hardware-timing model for that driver.

The optional `--flash-busy-reads N` delays each program/erase operation by
`N` status reads. During that interval the emulator toggles DQ6, reports DQ7
status and ignores command writes; the array changes only at completion. The
runner requires saved operation/read counters and no pending operation after
each transfer, so an emulator that ignores the option cannot pass silently.
This deterministic polling model does not represent physical pulse duration
or partially programmed cells after power loss. The default zero preserves
the immediate-completion model for other scenarios.
The fault helper also accepts a completed-bit mask for a pending operation:
programming clears only selected requested bits, while erase sets only selected
bits within the erase extent. Host tests exhaust all program masks and independently
exercise shared replay after selected torn program/erase outcomes.
`--power-cut OPERATION READS BITS` terminates the emulator during the selected
busy operation after the given number of status reads, applying the completed-bit
mask and synchronously persisting Flash without normal emulator teardown.
The runner requires the dedicated termination status, an exact operation record
and an independent whole-Flash comparison before resetting Stock. A partially
restored bank must remain hidden; if the selected outcome completed the entire
sector, the original catalog must already be restored. Recovery is then retried
and the final catalog checked as usual. An already completed recovery must make
no further Flash writes; zero busy-operation counts are valid in that case.
The option can be repeated for consecutive failures;
for example `--power-cut 2 16 0x5555 --power-cut 1 16 0x0101` first tears a
program word and then tears the retry's erase, so the latter acts on nonempty
Flash. Use a busy-read budget larger than the selected read count. These are
explicit fault outcomes, not an exhaustive model of physical power loss.
Core's **Stock recovery** matrix uses the shared portable x50ng setup and
Kermit watchdog, and retains phase logs, screenshots and saved states. The runner
requires a new output directory to prevent stale evidence, reaps its child
processes on failure/cancellation, and emits `result.json` only after verification.
With the portable Stock runtime prepared and its tools/libraries on the process
search paths:

```sh
python3 tests/e2e/port-recovery/run.py \
  --model hp50g \
  --dist build/x50ng/dist --build build/port-recovery \
  --output build/port-recovery-evidence --nm "$ATFE/bin/llvm-nm"
```

`--model hp49gplus` selects HP49g+; the runner reads the existing target profile
and verifies that the initial Flash contains that model's bootloader. Both
models use the same recovery image and independent verifier. This gate covers
their explicit erased-header-sector scenario. It does not provide user-facing
installation/error handling, HP48/HP39 entry support or busy-chip power-loss
coverage.

### Catalog relocation plans

The internal `hpclang_rpl_catalog_plan_relocation()` assembles the catalog part
of a reference-repair plan. The caller supplies a stable reader, a resolver for
physical mapping identities and bounded output buffers. Every binding is resolved,
including unbanked entries. For N entries the plan contains 2*N edits (address
and bank-switch fields) and 1+2*N guards (count, IDs and flags). The resolver
supplies separate physical mapping identities for the library payload and its
switch function; executing the switch is what selects the payload mapping.
Nonzero switch-function targets relocate under their own mapping. Zero means
no switch and stays zero; moving a function to that sentinel is unsupported.
Duplicate IDs remain distinct records. Identical addresses in different physical
mappings do not imply the same target. On failure both output counts are zero;
partially filled buffers must be discarded. Planning performs no native writes.

The caller must retain exclusive ownership, validate the resulting guards and
edits, and incorporate all other native roots before committing any relocation.
The catalog plan alone establishes neither complete reference coverage nor
permission to return to Stock after moving data.

`hpclang_rpl_mutation_catalog_grow_repaired()` combines caller-proven main-RAM
edits/guards with a snapshot of native root repairs. Every edit names its original
field before movement. Exact duplicate repairs coalesce, including discoveries
shared with native traversal; conflicting repairs, partial overlaps and edits
that split across movement boundaries reject before writes. Unchanged native
roots also constrain caller edits. ARM-held roots are identified by their cell
addresses, independently of native nibble addresses. Geometry publication follows
field repairs and movement. The legacy internal grow entry uses the same path
without additional edits.

The operation allocates temporary ARM heap storage for the root snapshot; an
allocation or preflight error leaves native memory and ARM roots unchanged.
Callers retain exclusive ownership and stable mappings/inputs throughout.
`HPCLANG_RPL_MUTATION_INCOMPLETE` distinguishes commit failure from preflight
rejection: even the first failed native nibble write may follow an ARM-root
update. This status is not rollback or permission to resume Stock, and this
geometry operation cannot simply be replayed. Temporary plan storage is released
on success and on either failure path; this is not recovery of changed native
state. Other physical banks, complete
object/code coverage and registration publication remain separate obligations.

`hpclang_rpl_mutation_visit_guarded_roots()` also reports every native nibble
read during root enumeration, including stack/table boundaries and traversal
metadata. Root provenance and order remain unchanged. The caller can coalesce
these read dependencies into bounded guards; either callback failing invalidates
all partial output. The adapter does not forward native writes. IO configuration,
mapping identity and ARM-side root values still require caller-owned stability;
they are not native-memory read dependencies. This traversal does not replace
catalog bank resolution or object-body reference discovery.

Catalog root traversal distinguishes `LIBRARY` (an unbanked payload binding)
from `LIBRARY_SWITCH` (a nonzero executable switch address in the current
mapping). RAM switch functions participate in reference preservation and
relocation; their bank-selected payload addresses do not inherit that mapping.
Zero/no-switch fields do not produce executable roots. The structural object
visitor likewise emits a separate `EXECUTION` event for a nonzero ACPTR switch
field, in addition to the ACPTR payload event. Compact and nested objects retain
the true native field address. These execution events still require code and
mapping analysis; the ordinary reference planner does not certify them safe.

`hpclang_rpl_reference_plan_execution_shift()` explicitly plans the stored
address of a validated EXECUTION event with separately qualified field/target
mappings. It accepts only a nonzero five-nibble execution target with zero
extent/auxiliary; a move to the no-function sentinel is rejected. Catalog switch
planning uses this entry instead of reclassifying executable pointers as data.
The ordinary planners continue rejecting EXECUTION. Planning an address does not
resolve the target's code body or prove complete execution-reference coverage.

`hpclang_rpl_reference_guards_begin()` and `guards_observe()` collect those
observations in caller-owned storage, sorted by physical mapping and address.
Repeated identical reads consume no additional space; adjacent observed cells
merge into fields of up to five nibbles, without covering unobserved gaps.
Conflicting observations, invalid input or capacity exhaustion latch an error
and invalidate the output count. Restart collection after any failure and use
the guards only after the entire traversal succeeds. Storage demand depends on
the observed addresses and their order; callers must handle exhaustion rather
than assume a fixed catalog or root count. The builder allocates no memory and
performs no native reads or writes.

`hpclang_rpl_reference_visit_nested()` uses a caller-owned work array and node
budget to enumerate INLINE/COMPACT descendants in a stable mapping. Immediate
edges retain native order; queued children are visited last-in first-out.
Repeated array aliases are visited again and consume budget. Workspace or budget
exhaustion invalidates partial results. The scheduler is iterative; underlying
object parsers retain their bounded recursion, and the node budget does not
bound their reads. External targets remain for bank-qualified resolution, and
opaque/execution entries remain explicit unresolved obligations. A successful
structural walk therefore does not certify a complete relocation graph.

`hpclang_rpl_reference_edits_begin()` and `edits_add()` merge separately
discovered fields into a caller-owned, sorted batch. Identical edits consume
one slot, including unchanged fields retained for preflight. Conflicting old
or replacement values and partial field overlaps invalidate the entire batch;
capacity and input failures also latch until collection restarts. Physical
mapping aliases must already be normalized. This removes duplicate writes
when root, catalog and object visitors identify the same native field, while
retaining conflicts as errors. It performs no IO and does not replace complete
reference discovery or later expected-value validation.

Native execution context is a separate root source from the calculator's RPL
return-stack cells. During a live ARM invocation the original runner can retain
continuation addresses into movable RPL code outside the roots registered by
stage1. A successful payload marker therefore cannot establish
return safety; acceptance must include the return to Stock and a subsequent
invocation. Platform integration must establish the types, ownership and layout
of these saved execution fields rather than infer roots from address-looking
integers or command-specific offsets.

Historical relocation routines do not establish a general machine-code safety
contract. HPGCC 1.0.3's `examples/libtests/stack.c` demonstrates stack inspection;
the 1.1/2.0 `sat_createtemp.c` allocates by moving the RPL return-stack area and
adjusting boundaries. HPGCC3's `sat3_garbcol.c` calls `__sat3_fixptrs` in
`sat3_free.c`; that helper repairs enumerated system roots, rather than decoding
arbitrary object bodies or saved CPU registers. Its directory counterpart
`__sat3_fixptrs2` has the same limited role. These are implementation observations,
not proof that omitted references are safe to ignore in hpclang.

Chapter 8 of [HP 49 Assembly Language Examples](https://literature.hpcalc.org/community/hp49-asm-examples.pdf)
explicitly distinguishes collecting while machine code is running from a movable
region from returning to SysRPL and restarting code after collection. This is
evidence that the native execution boundary matters, not that a CODE prologue
alone guarantees safe relocation. The proposed distinction between native-contract
compatibility and arbitrary hidden-address repair awaits maintainer confirmation;
existing preflight and M8 completion requirements remain in force meanwhile.

The classic ROM's machine stack has eight 20-bit cells and a modulo-eight
cursor; its pop helper clears the consumed cell. This layout is not a type
guarantee: the interpreter also calls these helpers for `RSTK=C` / `C=RSTK`,
which can store ordinary numbers. Publishing all eight cells as unconditional
relocation roots would corrupt such values. A platform contract must classify
the suspended caller's entries, or use an entry/return path whose root ownership
is established independently.

A standalone entry does not by itself establish that ownership. Read-only
HP49g+/HP50g captures through the independent recovery envelope, with library
275 absent, still contain one machine-stack value inside the movable TEMP
range. The surrounding Stock evaluation path therefore remains part of the
execution-root analysis even when the ARM image and its stack are owned.

For HP49g+/HP50g, stage1 recognizes a bounded return path:
when the saved post-ARMSYS PC points to `RTN`, `RTNSC` or `RTNCC`, the next
instruction consumes the circular machine-stack top as an execution address.
It publishes that cell as an invocation-local relocation root alongside the
hidden launcher object and saved PC. If the consumed target names another
proven return, discovery continues, up to eight distinct cells. Direct
`GOTO`, `GOLONG` and `GOVLNG` may lead to that return without consuming a cell.
The path is bounded to 64 instructions, including jumps and returns; conditional
branches, calls and register-mediated jumps require separate analysis and stop
this walker. Jump semantics follow chapters 41–42 of
[Introduction to Saturn Assembly Language](https://literature.hpcalc.org/community/intro-saturn-asm.pdf).
Identifying consumed stack cells does not repair embedded jump operands.
Recognition
validates the next-push cursor and each 20-bit target; it does not scan arbitrary
lower slots or infer past calls from nearby instruction bytes. Unsupported or
unreadable instructions stop discovery; this does not certify
complete execution-reference coverage or permit public reclamation.

`hpclang_rpl_cached_program_match` verifies a caller-bounded TEMP allocation
containing a ROMP identity followed by a DOCOL program against an independently
resolved catalog/bank/command source. It requires the complete allocation extent
and identical program nibbles, and publishes its view only after all checks pass.
The caller supplies stable mappings and the validated source identity; this is
not a byte-pattern search. A matching cached program and an instruction boundary
inside it still do not establish the pointer type of an arbitrary machine-stack
value. Execution-root classification remains a separate requirement.

Live cache validation must retain the catalog binding's bank identity. A command
installed in a user Flash bank cannot be compared through the main-RAM reader;
resolve its switch through the native bank table, validate the physical reader,
then expose that bank's logical window to the command parser. The cache and
original use separate readers throughout comparison. A structural diagnostic
can distinguish nested RPL boundaries from a machine-call return inside a CODE
body, but neither reverse instruction matching nor a matching boundary proves
that a saved scalar was produced by that call. The platform still owes the
suspended caller's provenance contract before publishing relocation roots.

The internal `rpl-code` literal inspector describes LC/LA register writes at
proven instruction boundaries. A caller-proven P value determines a 64-bit
write mask and positioned value, including nibble wrap; untouched register bits
survive. The encoded literal location is retained for guards, not declared to
be a pointer field. Numeric constants alone never establish address provenance
or mapping identity. A later register copy may propagate an independently
proven address fact, but these primitives do not authorize relocation.
The exchange inspector describes simultaneous standard-field swaps between
A/B/C/D pairs or A/C and R0..R4, including the launcher's `ACEX W` instructions.
Both fields must be read from the pre-instruction state before either is
replaced; bits outside the selected field survive. P/WP fields require the
proven current P. A/C exchanges with D0/D1 are also decoded: EX swaps five low nibbles,
XS swaps four while preserving the fifth address nibble; higher A/C nibbles
survive. Plain D0/D1=A/C assignments likewise copy five low nibbles, while
D0/D1=AS/CS copy four and preserve the fifth address nibble; the source remains
unchanged. P exchanges and custom fields remain unsupported. This is a data-flow building block, not a complete
launcher analysis or permission to treat arbitrary register values as roots.

`A=PC` and `C=PC` now expose `NEXT_PC` as their assignment source, with
`instruction+4` in the descriptor's constant field. This distinguishes an
execution address in the instruction reader's mapping from an arbitrary
literal. Only the low five nibbles change; PC exchanges remain unsupported.
An address-space-crossing next PC is rejected. The maintained `pc_capture.py`
driver compares the C decoder with one synthetic instruction executed by the
original ARM Saturn interpreter: 192 cases across HP49g+/HP50g, both registers,
three instruction locations and every P value pass, preserving all unrelated
registers, flags and machine-stack cells. Reports and source/input hashes are
under `/work/oct28-pc-capture/<model>`. The earlier extreme-boundary probe reached
an unmapped interpreter fetch; it does not prove wrap semantics. These checks
establish capture semantics, not launcher reachability or relocation permission.

`hpclang_rpl_code_memory_inspect` describes DAT0/DAT1 loads and stores involving
A/C: direction, address register, selected register field and transfer length.
The register field starts at its selected nibble while memory starts at D0/D1;
the address register does not advance. Standard fields and explicit counts are
covered by 3,456 unit combinations, with truncated/unreadable instructions and
unsupported/custom fields rejected without publishing a descriptor. No data
memory is read by this inspector, and memory contents are not classified as
pointers merely because a transfer exists.

The maintained `alternate_return.py` now checks every such transfer along its
controlled original PrRUN return, including the R0-to-A-to-stack path. Both
models pass all 192 baseline, repaired and deliberately stale-register cases,
including original-allocation variants: 960 stores and 864 loads match C
descriptors while preserving unrelated register bits and D0/D1. Reports in
`/work/oct28-memory-return` retain source/input hashes and transfer observations.
These are controlled continuation checks, not complete entry reachability,
incoming-register provenance or permission to enable public relocation.

The `hpclang_rpl_code_origins_*` operations compose those descriptors over
caller-owned per-nibble tokens. Assignment snapshots its source, exchange
snapshots both registers, and memory transfers snapshot their input before
updating any destination, including overlapping token arrays. Untouched fields
retain their tokens; scalar constants clear them, while PC captures require
separately established mapping-aware tokens. All register/field bounds are
checked before changes. These are abstract origins, not numeric execution,
pointer classification or a general control-flow interpreter. Callers must stop
or invalidate facts at unmodeled writes and retain all code/mapping dependencies.

`alternate_return.py` composes the original R0.A-to-A.A, DAT1-to-C.A,
A.A-to-DAT1, D1=D1-5, C.A-to-DAT1 segment. Copies and transfers use these C
operations; `hpclang_rpl_code_address_adjust_inspect` decodes the address step.
The fixture checks the resulting D1, replaced carry and unchanged remaining
register/flag/return-stack state, and invalidates D1 origins rather than copying
tokens through arithmetic borrow. Both stack slots
have independently checked addresses and distinct token storage.
Across both models, all 192 controlled return cases pass: 96 follow all five
instructions, carry R0 tokens into the original stack cell, and preserve the
old top's tokens in the adjacent cell; the other 96 do not enter it. The 48
original-allocation variants additionally qualify the new TEMP allocation
through the current C whole-chain visitor, requiring its exact start at the
preceding TEMPTOP plus one and the corresponding new TEMPTOP. They compose
`AD0EX`, `D0=A`, `AR0EX` at `5bc2/5bc5/5bc8` using the C exchange/assignment
operations, then reject any later write to R0 before allocation return, even
if a write preserves its numeric value. The completed object's parsed span
must equal the allocation extent. These qualified R0 tokens feed the return
segment instead of fresh symbolic seeds. Interior/adjacent candidates and
unreadable chain metadata are rejected with query output unchanged (six checks
per allocated case). This covers the exercised no-GC allocation path; it does
not establish every allocator/launcher path or safe execution across native ARM.
The allocation proof's captured instruction, chain and object-span reads are
revalidated by catalog transaction guards before writing: main-RAM dependencies
use mutation IO and ROM dependencies use a distinct read-only mapping. Across
the 32 allocated move attempts, 96 deliberately mismatched witnesses reject
before any memory, entry or register update. The register repair also receives
the full R0 word captured at qualification, rather than rereading it when
constructing the repair. Another 64 cases alter either its address bits or its
non-address bits after qualification; all reject with memory, entry and register
values unchanged. After successful movement, a new
whole-chain query and span parse must find the same-sized allocation and all
object nibbles must match. The 32 baseline/repaired cases retain qualified R0;
the 16 deliberately stale-R0 controls fail the exact-allocation query. These
guards bind the recorded inputs, not unobserved launcher or caller behavior.
Unit tests cover descriptor composition,
overlapping copies, simultaneous exchanges, clearing constants, and unchanged
state on invalid inputs. `/work/oct28-continuous-return/matrix` retains current
hash-bound reports; the earlier three-step and fixed-address-step evidence
remain in `/work/oct28-origins-return` and `/work/oct28-origin-stack`.
Seeding R0 tokens identifies their origin; it does not by itself prove the
incoming value is a valid object, establish the whole caller path, or authorize
movement. The deliberately stale-R0 controls retain that distinction.

`alternate_return.py --arm-return preserve` additionally executes a six-instruction
ARM leaf through the original ARMSYS handler before the controlled relocation
and return checks. It changes ARM r1/r2/r3/r12 and condition flags, preserves the
incoming r0 continuation, and returns with BX LR. The observed boundary supplies
Saturn PC `88c73` in ARM r0 and SAT_CPU in r1. This is the Stock boundary contract,
not an ordinary C return value; stage0's saved r0 has the same role. Both models
pass all eight cursors and three relocation/control modes (48 cases). With
`--arm-return clobber`, a seventh instruction changes ARM r0 to `11`; all 16
cases observe Saturn PC `11` at the next dispatch and stop before executing it.
This 256-case subset includes the preceding 192 cases and 320 pre-write
guard rejections. These leaves establish this boundary behavior, not arbitrary
application side effects, an installed SDK application run, or continuous
launcher-to-application-to-Stock acceptance. By default, the later controlled
return sets up its caller context explicitly.

`--continuous-return` instead retains the actual allocation/ARM boundary state.
Its controlled caller is initialized before allocation; after ARM returns,
the real Saturn registers, flags, circular return stack and ARM SP survive the
catalog operation unchanged except for the explicitly proven PC/R0 repairs.
Execution resumes at the suspended interpreter dispatch, not its entry point.
After the Saturn return, ARM SP must match its boundary value and the pushed
object must still pass the exact TEMP-allocation query (or fail for stale-R0
controls). Both models pass all eight cursors and three modes: 48 additional
cases, for 304 total cases and 480 pre-write guard rejections. This closes the
state-reset gap in this controlled path. The initial caller and ARM leaf remain
test fixtures, and catalog mutation runs on the host at a suspended boundary;
it is not an installed SDK application performing the operation itself.

The address-adjust inspector describes D0/D1 immediate addition/subtraction
(`16n`, `17n`, `18n`, `1cn`) with signed deltas of 1..16. It reads only instruction
nibbles and preserves its output on errors. Execution wraps at 20 bits and
sets carry on overflow/underflow, clearing it otherwise. Consequently both
address-origin and carry facts need updating or invalidation. This descriptor
does not qualify an incoming pointer, resolve aliases or authorize wrapping
across mapping boundaries. Unit checks cover every opcode/immediate, each
truncated/unreadable/malformed nibble and output preservation. The maintained
`address_adjust.py` fixture verifies 1,024 cases per model against the original
interpreter: both registers, both signs, all immediates, eight initial addresses,
both incoming carry values, and unchanged surrounding state. Reports are in
`/work/oct28-address-adjust/{hp49gplus,hp50g}`. The 192 return cases also pass
using this decoder, including 96 five-step origin paths.

The reproducible `tests/e2e/code-relocation/dispatch_prefix.py` check accepts
`--state`, `--model` and `--output`, with optional `--through-dispatch`. It compiles
the current C decoders and executes the original ARM Saturn interpreter. The
prefix mode checks entry `351cc` through the comparison boundary `351e8`:
128 controlled-entry cases and eight changed-code controls pass across both
models and both retained-predicate/standard-Stock snapshots. D0/D1 and P restore;
the temporary machine-stack slot contains the scalar type ID (including
address-like `90001`), then clears on pop while seven other slots survive.

The extended mode continues through the true/false branches, native Boolean
writeback and next RPL dispatch. Its 256 cases cover both initial carry values;
16 modified-code controls reject incorrect address restoration or true/false
constants. Only the expected Boolean stack slot may change outside CPU state;
the complete remaining native RAM is compared byte-for-byte. D0 advances five
nibbles to the next thread entry and the interpreter reaches its expected target.
The result binds ROM/RAM, decoder sources/headers, reader and driver hashes.
These are controlled entries after OS setup, with no IRQ, not an unrestricted
caller/startup proof or permission to relocate. The individual D0/D1 checks
retain 4,096 x48 comparisons and 128 saved-ROM instruction checks.

The internal `hpclang_rpl_dispatch_reference_visit_identified` binds a
caller-reviewed schema to exact handler/dependency nibble ranges before
emitting edges. At least one range contains the dispatch entry. Successful
reads of matched code and object fields become commit-guard dependencies;
code mismatches, malformed/unreadable input and callback errors reject without
object edges during identity checking. Expected bytes must be independently
reviewed, and the caller must establish full semantic/callee coverage and a
stable shared mapping. This is neither automatic ROM authentication nor proof
that a matching prefix makes an arbitrary RAM continuation safe.

ASan/UBSan reference tests cover disjoint ranges, byte changes before/after
capture, unavailable/invalid input, dependency failures and metadata bounds.
ARM compilation and static analysis pass. Four retained ROM identity windows
match across HP49g+/HP50g; all 768 single-nibble changes reject before edges.
These windows are inspection extents, not claimed complete handler bodies.
Evidence: `/work/oct28-dispatch-identity/results.json`, reproduced by
`research/oct28-dispatch-identity-native.py`.

Catalog/cache transactions can now revalidate read-only dependencies in
proven-disjoint mappings through the optional `catalog_repairs.guard_reader`.
Main-RAM guards always use the mutation IO; failures never fall back to the
external reader. This accepts ROM identity guards without permitting foreign
edits. Distinct mapping IDs must mean proven physical disjointness, and callers
retain exclusive mapping/data ownership through publication and return.
The internal repairs structure gained an optional final member; rebuild callers.

Twenty focused unit scenarios cover ROM and equal-address disjoint mappings,
changed bytes, unavailable/invalid reads, missing readers, bounds, wrong mapping
IDs and native-guard failures. Every rejection preserves RAM and register words
with zero writes. The full mutation suite passes ASan/UBSan; ARM runtime/caller
compilation, static analysis and the eight repaired-dispatch cases also pass.
This provides commit-time checking, not complete dependency discovery or a
Stock-GC interception mechanism. ARM artifacts: `/work/oct28-mapped-guards/`.

The maintained `dispatch_prefix.py` now uses that identity entry point before
each real interpreter dispatch, with fixed reviewed window bytes rather than
learning expected bytes from its input. All 896 prefix/full/DUP cases across
two models and both snapshot families pass, checking code and object dependency
coverage. All 44 changed-code cases reject with no edges for the mismatched
handler. A separate, explicitly bypassed run retains the 44 causal execution
controls; bypass is forbidden without an injected fault. These counts do not
extend the windows' semantic scope. Current reports and independent count/hash
audit: `/work/oct28-dispatch-identity-integrated/`.

With `--through-dispatch --duplicate-entry`, execution starts at `37258`:
DUP preserves the original stack-top object, consumes one existing stack slot,
and enters the embedded predicate. The C structural decoder's two wrapper
edges and single predicate edge agree with the actual interpreter entry; the
scalar type operand is never emitted as a pointer. Capacity values 1 and 16,
both carry inputs, all eight machine-stack cursors and all P values are covered.
Across both models and both snapshot families, 512 cases and 20 changed-code
controls pass, including a wrong stack-step control. Together with the existing
modes this gives 896 cases and 44 controls. The allocation/GC branch at exhausted
capacity is deliberately outside this result, and remains an open handler
obligation rather than permission for public relocation.

Inspection of both retained ROMs identifies the exhausted-capacity dependency:
`05f0f` calls `0679b`, `0613e`, and `067d2` before decrementing the replenished
capacity. The first/last routines save/restore D0, D1, B.A and D.A through
system slots `8076b`, `806f8`, `806f3`, and `80e9b`. The inspected `0613e`
prefix pops its return address into `80fad`, follows the chain rooted at
`806ee`, and writes six-nibble entries at `06184`. Consequently, setting the
synthetic test's capacity to zero is not a valid heap fixture. Exhaustion
coverage requires a coherent Stock heap and continuation roots, with movement
and write-set assertions. These inspection windows establish dependencies,
not complete handler extents or successful GC execution. Reproduction:
`research/oct28-dispatch-capacity-callees.py`; local artifacts:
`/work/oct28-dispatch-capacity-callees/windows.json` and per-model disassembly.

The maintained `tests/e2e/code-relocation/dispatch_capacity.py` runs the
original interpreter with the retained ten-predicate Stock fixture. Invoke it
with `--state <snapshot> --model hp49gplus|hp50g --output <directory>`;
add `--ram-interior` for the unsafe movable-entry contrast. Unicorn is required;
this is a local fixture test, not part of ordinary CI. Reports distinguish
`ROM continuation preserved` from `stale RAM continuation reproduced` and
record input, driver and native-reader hashes. Neither mode claims real
low-memory or public relocation acceptance. IRQs are disabled and caller
machine-stack frames cleared; no allocator or Saturn instruction is stubbed.

The ROM mode forces D.A=0, executes 12,205 Saturn instructions through collection
and DUP, and checks all 20 user values, the duplicated top, A.A/R1.A continuation,
D0 and machine-stack cursor. The return-stack boundary falls from 556530 to
554981; the user objects remain at their original addresses.

The RAM mode models a 32-nibble TEMP DOCSTR envelope with an explicit data-stack
root and a validated footer chain. Collection moves it from 556486 to 554937
and repairs that root, but leaves A.A/R1.A at 556501 instead of 554952. Both
models reproduce this while preserving the original 20 user values. Old payload
bytes remain readable, so reaching the next handler is insufficient proof.
This diagnoses an unsupported Stock execution fixture; it does not establish
a defect in hpclang's caller-proven register repairs for catalog transactions.
Those repairs do not intercept Stock collection. Public movable entries still
require a proven native launch convention or explicit continuation ownership
and repair/pinning. Latest four reports and final SRAM are retained under
`/work/oct28-dispatch-capacity-maintained/{rom,ram}/`.

`dispatch_repaired.py` consumes the RAM contrast SRAM via `--prepared-sram`,
plus `--state`, `--model` and `--output`. The current C catalog-grow transaction
checks both reviewed ROM windows through its mapped read-only guard channel,
then resumes original ROM at `3727d`. `dispatch_identity.py` supplies the same
fixed expected bytes to this test and the identity-bound interpreter test.
Both models reach the expected type comparison after a 16-nibble move when R1
is repaired (with or without A repair). Omitting R1, including repairing A
alone, yields an observed wrong dispatch after seven instructions, not timeout.
The restore instruction overwrites A from R1. Four positive and four negative
execution cases check register bits and the moved data root. Fourteen injected
ROM-read change/unavailable/invalid cases reject before any RAM/register write,
including faults at the final identity byte. Reports retain code/profile/input
hashes. This supplies caller provenance and stops before catalog publication or
Stock return; it is not complete discovery, public registration, or a Stock-GC
repair hook. Results: `/work/oct28-dispatch-repaired-guarded/<model>/results.json`.

The internal `rpl-code` branch inspector handles caller-proven GOC/GONC,
GOTO/GOSUB, GOLONG/GOSUBL, absolute GOVLNG/GOSBVL and the GOYES operands of register, HST, ST, P and
A/C bit-test instruction boundaries. Its relocation planner
qualifies source and target mappings separately, preserves signed 2/3/4-nibble
displacements and 20-bit address wrap, and rejects split instructions, range
overflow or conversion into a conditional return. Cross-CODE calls are allowed:
the stored native `SETUP` program contains such calls, so an individual CODE
object is not a sufficient ownership boundary. Returned patches retain their
short operand width and old address. The shared reference-edit builder,
validator and RAM replay accept explicit widths of 1–5 nibbles, including mixed
code operands and object pointers. Duplicate identity includes width; overlap
and value bounds use each record's extent. Complete instruction guards and
ownership must remain valid through commit. Predicate operands are checked even
when they do not change. The read-only planner neither discovers all
instructions nor classifies opaque payloads or authorizes catalog movement.
Absolute transfers use five-nibble operands and do not change when only the
instruction moves; target relocation still requires independently qualified
mapping identity.
Indirect transfers and complete execution coverage remain open.

## Capabilities

Capabilities describe genuine model/platform differences such as display geometry, stream storage, object services, or transfer mechanisms. They must not become a substitute for clean platform implementations.

## Declarative UI core (planned)

hpclang should include a small **renderer-agnostic declarative UI engine**, not a fixed calculator theme or widget skin. The intended application model is:

```text
View = f(Model)
event -> update(Model) -> declare View again -> reconcile by stable key
```

The clean core may own stable node identity, keyed reconciliation, deterministic row/column layout, focus and keyboard navigation, event/action routing, controlled-input state, explicit async state/effect hooks, dirty-region tracking, and a bounded custom-canvas node. Ordinary C functions remain sufficient for component composition; the implementation should remain compatible with fixed-capacity pools and predictable ARM920T memory/time budgets rather than requiring a desktop-style heap-retained object graph.

The core must **not** freeze visual policy. Style identifiers are renderer/theme inputs rather than pixel definitions. Widget chrome, fonts/colors, native calculator appearance, and final rasterization belong to a backend above the layout/logic engine. A downstream project such as 39gsX can therefore implement a renderer/theme matching the independently reverse-engineered HP39 native UI while another environment can render the same declarative tree differently.

A canvas/custom-paint escape hatch is part of the boundary: declarative layout owns the node and its bounds, while application/backend code performs specialized drawing only when the node revision changes or its region is damaged. Historical HPGCC `win*` compatibility may later target this engine where useful, but the legacy widget model does not define the canonical UI API.

Public C names, node storage limits, layout units, focus traversal rules, and renderer ABI remain deliberately unfrozen until a focused design/fixture slice begins.

### Declarative application contract

The maintainer's original application sketch establishes the following behavior
for the planned engine. The example names describe the intended use; they do
not freeze public API spelling or turn illustrative platform code into an ABI.

- **Declaration in ordinary C:** `AppView(ui, const_model)` describes the desired
  view with nested row/column/component scopes, ordinary `if/switch`, and loops.
  It does not draw the LCD, start effects, or require the application to manually
  create/destroy persistent widgets. Ordinary C functions compose reusable
  components, including multiple independent instances of the same function.
- **Scoped identity:** a key need only be unique among siblings. The complete
  parent/component/key path identifies a node. Dynamic lists use persistent
  item IDs, not array positions, so insertion and reordering preserve the
  surviving items' focus and editing state. Define removal/filtering, same-key
  type changes, duplicate keys and collision handling explicitly.
- **Controlled values and actions:** model values flow into the view; typed
  actions and edit events flow back to application updates. Text edits have
  explicit capacity and UTF-8 rules, with event-text ownership and lifetime
  defined. Rendering does not silently mutate application state.
- **Explicit asynchronous state:** idle/loading/ready/error lives in the model.
  An update may request an effect; starting the operation and delivering its
  result are separate from declaring the view. Loading indicators, failure and
  retry are ordinary conditional UI. The implementation must additionally
  define cancellation and stale-completion handling.
- **Custom canvas:** a declared node supplies identity, bounds, paint callback,
  application state and revision. The framework invokes paint only when the
  revision changes or its region is damaged; exposure, movement and resize must
  cause the appropriate damage. Specialized drawing remains inside the owned,
  clipped rendering boundary.
- **Bounded execution:** fixed-capacity node storage, bounded identity paths
  and explicit capacity errors support predictable ARM920T memory/time usage.
  Frame requests can be coalesced, and reconciliation/layout precede painting
  only the damaged regions.

The sketch's `UI_SCOPE` uses a `for` increment to close a scope. That alone does
not close it on `break`, `return` or `goto`. Before accepting a C declaration
syntax, specify and test its supported control flow, balanced-scope validation
and failed-frame recovery. An interrupted or over-capacity declaration must not
silently corrupt the last accepted tree. Public macro spelling remains open.

The illustrative native surface is RGB565 and the canvas includes a pointer
callback. The six calculator targets require their real monochrome/grayscale
surface contracts and keyboard operation; pointer input remains an optional
backend capability. Preserve custom drawing and input routing without imposing
RGB565 storage or claiming touch hardware. Any native drawing lease must define
clipping, lifetime, publication/cache synchronization and release through the
existing owned display/surface services.

The application fixture must cover conditional connection controls, two
instances of a reusable status component, controlled filtering, stable-ID list
insertion/reversal, idle/loading/failure/retry/completion and revision-driven
waveform canvas updates. Its scan operation is an illustrative asynchronous
workload, not a promise of a new device-discovery service. A deterministic test
provider can exercise effect transitions, alongside real calculator input and
display verification. This fixture preserves the original application model
while fitting each target's screen and input capabilities.

### C application sketch review

The 2026-10-23 technical review uses the installed
`examples/declarative-ui/app.c` as the public sketch, together with its
`app.h` model and `main.c` event loop. This is executable ordinary C, built
from a relocated SDK, rather than a second illustrative API that could drift
from the implementation. The review preserves the application contract above.
Public names remain provisional until UI milestone acceptance.

| Original requirement | Public sketch | Independent evidence |
| --- | --- | --- |
| Nested composition and conditional children | `status_card` scopes two component instances; `demo_view` composes rows, columns and a scrolling root, selecting controls with ordinary `if` statements. | `test_ui_demo.c` executes the actual view at each state; `test_ui.c` exercises scope interruption, duplicate keys and rejected frames. |
| Keyed changing list | `device_row` uses persistent device IDs; insertion and reversal change model order without manually creating widgets. | The application test checks surviving control lifetimes after insertion/reversal and renderer replacement. |
| Controlled text and values | `demo_view` reads the filter and enabled state; `demo_update` accepts edit proposals and toggle values. | The application test edits the filter through `hpclang_ui_edit`; core input tests cover UTF-8, selection, capacity and accepted/rejected proposals. |
| Explicit asynchronous states | `demo_update` queues scan work; `demo_poll` drives its provider; loading/error/retry/cancel are model states. | The application test covers failure, retry, cancellation, stale success/error delivery during a replacement request, and exit with queued work. |
| Revision-driven custom canvas | `wave` paints the model samples inside the renderer's drawing lease; `DEMO_WAVE` updates samples and revision. | Host renderer tests exercise clipping/damage; six-model LCD verification checks model-derived waveform coordinates independently. |
| Bounded declarations and separate rendering | `demo_view` begins, lays out and commits a candidate; `main.c` renders and publishes afterward. Storage is configured before the loop. | Every application-test declaration checks that the entire application model and provider bookkeeping remain unchanged; allocation-fault and failed-frame tests cover the core/renderer boundaries. |

The view borrows the model for canvas state. Keep the model alive through paint;
change its revision whenever the canvas-visible samples change. The accepted
tree owns copied text, but does not copy arbitrary canvas state. Model updates
and accepted declarations therefore precede rendering in the example loop.

The scan deliberately uses application ownership (`owner=0`). The Scan button
disappears when Loading replaces it, so tying the operation to that button's
node lifetime would cancel valid work. Cancel/disconnect explicitly cancels and
drains the task before reusing its provider context. Completion tokens must
match the active request; shutdown cleans up outstanding work exactly once.
Node-owned effects remain available when removal really should cancel work.

`HPCLANG_UI_WITH` uses the supported compiler's cleanup attribute to balance
scopes across normal completion, `break`, `continue`, `return` and outward
`goto`. It is not an ISO-C-only macro. Explicit begin/close functions are the
alternative; jumping into a scope or using `longjmp` across it is unsupported.
The caller retains responsibility for commit/abort and error propagation.

This review finds the six application patterns represented in the public C
surface. It does not freeze names, claim pointer hardware or faithful HP39
native styling, or close physical-memory/performance acceptance. The simulated
provider, renderer palette/font choices and keyboard-only interaction are the
documented target adaptations, not removed application requirements.

### Startup resource admission

The wrapper's native request word is `0x0fc00000`, not a byte count. The
Launcher V3 format in HPGCC 1.1's `ARMToolbox/LibFmt.txt` and HPGCC 2.0 SP2's
`sources/ARMToolbox/masd_source/ARMTBX_3_SOURCES.DIR` assigns bit 22 to the MMU
packer, bit 23 to optional ARM OS RAM, and bits 24–27 to native stack-growth
reservation in 16-byte blocks. Our word sets both flags and all 15 stack blocks
(240 bytes), the largest representable request. Runtime fences still bound
each mutation; this request does not promise unlimited or unconsumed capacity.
With the packer enabled, the low 20 bits describe space to leave for TEMPOB
growth, not the application's pool size; this word leaves that field zero.
Writing a 36 KiB application limit there would change native free-space policy,
not enforce a 36 KiB allocation.

The exact-head `0e68b3b1` object-mutation CI failures on HP49g+ and HP50g
expose a concrete USEROB capacity limit on the third invocation. Independent
reads of both captured MMU tables, boot bounds and Saturn RAM give DSKTOP
`0xe0012`, RSKTOP `0x88de4`, USEROB fence `0xe0000` and five reserved nibbles.
Only 13 nibbles remain for growth, despite a raw stack gap of 356,910 nibbles.
Replacing integer 987 (14 nibbles) with the unchanged 34-character test string
(78 nibbles) needs 64 nibbles and would lower DSKTOP to `0xdffd2`, below the
required `0xe0005`. Both captured TLS errno cells contain ENOSPC (28). The
native-page fence must remain intact; total free RAM is not contiguous USEROB
capacity. The failed wrapper requested only one 16-byte stack-growth block.

Reproduction artifacts are retained in `research/oct27-object-ci`: the two
full SRAM/on-chip captures and `capacity-audit.json` with SHA-256 input identities.
`research/oct27-object-ci-capacity.py` checks the bounds against both captures.
The original CI jobs are 121826320519 and 121826320547. The historical Launcher
V3 source's MMUARMJUMP subtracts the stack-growth request from DSKTOP before
assembling main-RAM pages. The 240-byte candidate now passes the complete
three-invocation Stock object-mutation lifecycle on both models, including
settled return and phases 0x4f530400/401/402. All five retained CI code artifacts
(application ELF/HPC, two launcher ELFs and stage1 ELF) match byte-for-byte;
only the wrapping request changes. The production wrapper reproduces both
tested packages exactly. `research/oct27-port0-reserve-identity.py` and
`/work/oct27-port0-reserve/{code-identity,verified-results}.json` retain the
artifact checks and six result records. All six models now pass: three invocations each on HP48gII/HP49g+/HP50g
and 15 each on HP39g+/HP39gs/HP40gs, totaling 54 lifecycle phases with settled
return. The independently checked structured records are retained in
`/work/oct27-port0-reserve/verified-results.json`, separately from the subsequent
catalog suite which reuses the harness build-output paths. The accepted-main integration preserves
all three family payloads/result addresses and six launchers byte-for-byte;
integration port/mutation/reference/physical-mapping sanitizer tests pass.
New CI remains required; these local passes do not complete M8.0.

The code-relocation CI failures (HP49g+ job 121826775448 and HP50g job
121826775572, same `0e68b3b1` head) are test allocation failures before HOME
detachment. Independent snapshots from both models agree on the following bounds. The retained PC
`0x0122c298`, malloc return LR `0x0122c45f`, stack operands and TLS errno 12
identify the one-byte-per-nibble HOME snapshot: 246,907 requested bytes exceed
the complete 229,376-byte RW region. HOME is `0xc3a60`, its body begins at
`0xc3b04`, and Port 0 begins at `0xfff7f`. The candidate packs both nibbles into
each snapshot byte, needing 123,454 bytes for this odd-sized body, and still
compares every native nibble after detach/reattach. ARM build, formatting and
static analysis pass. All ten native detach/reattach/cache cases across
HP49g+/HP50g now pass from fresh standard Stock installations. The 62,136-byte
payload, verifier inputs and every packaged case match the saved input hashes;
`research/oct28-packed-home-audit.py` replays all checks against the retained
RAM/Flash without running the emulator. The replay verifies catalog bindings,
HOME attachments, cache lookup/message behavior and original Flash preservation.
New delivery CI is still required. Failure RAM, matching-image identity and allocation audit
remain under `research/oct27-code-ci`; `oct27-code-ci-allocation.py` reproduces
the address/size/errno checks. This fixture fix changes neither production
mutation behavior nor its memory fences.

The historical implementations assemble a pool from available native memory
and optionally append an OS allocation. Their OS probing policy varies: the
1.1 HP48 source leaves 4 KiB after its probe, while the other 1.1 source leaves
16 KiB; the 2.0 final-launch path also probes before reserving its block.
Those source facts explain why a single static native-reservation number would
be misleading. They do not prove a specific six-model firmware allocation or
allocator overhead. Runtime admission validates the supplied pool; whole-system
acceptance still needs model-specific observation of the native allocation,
stored objects and transfer staging. The linked minimum in `memory.json` is a
requirement on that pool, not an observation of its actual size.

Source audit provenance: the inspected `hpgcc-2.0-SP2-Linux.tar.bz2` has SHA-256
`e4f9d632e65316101717f39c00523c2f43608a11156d4a4d4e8fae3195771029`;
the member is `hpgcc_20/sources/ARMToolbox/masd_source/ARMTBX_3_SOURCES.DIR`.
The 1.1 comparison uses `ARMToolbox/masd_source48/SYSMEM.ARM` and
`ARMToolbox/masd_source/SYSMEM.ARM` in the retained 39G-support release archive.

For runtime observations, set `HPCLANG_E2E_CAPTURE_STAGE_POOL=1` on a Stock
harness invocation. At each existing memory checkpoint the host reads the
boot page's native-state pointer, ARMSYS page count/table/block fields and the
original coarse table through the stage mapping. It requires the table address
to match the active stage descriptor and every observed page to have a valid,
distinct physical mapping. HP39's two native workspace pages are recorded
separately from the stage count. Inconsistent or repurposed native registers
reject the observation; it does not guess a count from trailing table contents.

Each checkpoint writes immutable `*-pool-ID.json` with version 1, checkpoint
ID, model, RW bytes, stage/native-workspace page counts, native OS block pointer,
table address and physical page addresses. The pointer is not an allocation
size. The normal memory report and its acknowledgement remain unchanged.
Stock run setup removes previous pool reports. No guest bytes, fields, symbols
or allocations are added by this observer.

Run `tests/e2e/ui/verify.py stock FAMILY --pool` to require both pool reports,
correct target identity, 36 KiB RW, valid unique physical pages and identical
pool observations before PASS and after runtime cleanup. The ordinary UI
pixel/memory/return checks still apply. Initial HP50g execution with the
unmodified launcher records 94 distinct pages (376 KiB) and a 36 KiB RW arena.
HP39g+ also passes the complete UI and return flow: 34 stage pages plus two
native-workspace pages, with identical records before PASS and after cleanup.
HP39gs passes the same flow with 33 stage pages plus two native-workspace pages.
The linked layout and observed distinct pages give this pool ledger for the
maintained UI acceptance artifact (all values in KiB):

| Pool use | HP39g+ | HP39gs | HP40gs | HP48gII | HP49g+ | HP50g |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Stage extent, alignment and original-table prefix | 20 | 20 | 20 | 20 | 20 | 20 |
| Stage stack | 4 | 4 | 4 | 4 | 4 | 4 |
| Application MMU workspace | 4 | 4 | 4 | 4 | 4 | 4 |
| Application code | 56 | 56 | 56 | 56 | 56 | 56 |
| Application RW, including heap-backed LCD buffers | 36 | 36 | 36 | 36 | 36 | 36 |
| Native-call workspace | 8 | 8 | 8 | 0 | 0 | 0 |
| Supplied pages beyond this layout | 16 | 12 | 12 | 28 | 256 | 256 |
| Total distinct supplied pages | 144 | 140 | 140 | 148 | 376 | 376 |

The application RW row includes its boot page, static data, heap and reserved
8 KiB stack; these must not be added again. This is the acceptance fixture's
linked image and observed calculator state, not every UI application's budget.
It measures the supplied mapped pool, not native allocator metadata,
installation storage or transfer-time peak usage. All six Stock models pass
the complete UI/return flow and both pool checks locally. The local Runtime
workflow now opts Stock UI jobs into capture and
requires `--pool` verification, uploading the linked `launchers/memory.json`
alongside observed reports. Other suites and Direct runs retain their existing
behavior. This workflow change is submitted in `d409b014`; CI acceptance is
pending.

Stock startup has an earlier admission boundary than the application's RW
budget. Stage0 must fit the linked stage1 memory extent (including its prefix,
alignment gaps and BSS) plus a complete stage stack page in the supplied pool.
It rejects zero/undersized pools and counts beyond the coarse-table capacity
before changing the mapping, switching SP or copying bytes. The minimum is
derived from the stage1 link result. HP39's two native-workspace pages are
excluded before this check. Stage1 subsequently checks application code, its
MMU workspace, static data, boot page and application stack. This does not
reduce the legacy launcher's reservation or guarantee application heap demand.

The Direct harness exercises zero, one and 257 supplied pages before normal
application execution. Each rejection must return with the caller registers,
interrupt state and mappings intact and leave the entire temporary pool
unchanged. Normal payload PASS, pixels and Stock return are still required.

### UI resource measurements

The UI E2E application limits its mapped writable arena to 36 KiB. Its test-only
startup wrapper fills the unused reserved stack with a pattern before C/C++
initialization, then samples the lowest changed word during main and after
libc finalizers and runtime resource cleanup. The entry frame is retained. This
measures touched stack words including instrumentation overhead, not a bound on
stack-pointer excursions through unwritten storage. Heap observations count
allocator-reserved bytes; deliberate exhaustion is excluded from the normal
workload high-water observation.

Before the launcher removes the mappings, the test requests a memory checkpoint.
The shared Direct/Stock harness accepts a word-aligned descriptor within the
reported application RW arena: `{id, count, words[count]}` with 1–64 words.
The request is `0x4d100000 | offset`, where `offset` is relative to the boot page;
the acknowledgement is `0x4d200000 | offset`. Invalid geometry, inaccessible
memory or output failure rejects the checkpoint. Each ID produces one immutable
`*-frame-memory-ID.json` artifact with `version`, `id` and `words` fields.

UI report ID 1 contains eight words: schema version 1, entry stack use, main
watermark, post-cleanup watermark, reserved stack bytes, normal heap high-water
bytes, heap capacity and mapped RW bytes. The verifier requires the final sample,
monotonic stack observations below the 8 KiB reservation, valid heap bounds and
the unchanged 36 KiB RW limit. These test measurements do not reduce or measure
the Stock launcher's physical reservation and do not change the public ABI.

## Modern C++ profile

hpclang uses Clang/LLVM, libc++, libc++abi, compiler-rt, and picolibc with `-fno-exceptions` and `-fno-rtti`. This is a modern embedded C++ profile, not unrestricted hosted C++.

Current C++23 validation includes templates/concepts/lambdas/constexpr, `std::array`, `std::span`, `std::optional`, `std::string`, `std::vector`, heap-backed objects, global construction, and wrappers over real time/display services.

Exceptions, RTTI, threading, locale-heavy facilities, iostream/file streams, and `std::filesystem` are separate capabilities and are not claimed without dedicated runtime support.

## Current dependency order

The clean-core order is now:

```text
device/capability facts
+ monotonic time
+ physical keyboard/events
+ display/software surfaces
+ native/generated fonts and UTF-8 text
+ hp4950 standard stream I/O
```

M7.1 font architecture, asset curation, native-ROM census, keyboard/runtime return evidence, and validation are complete: the final bitmap catalog is **245 faces / 1,639,485 glyph mappings / 3,472 assembly sources**, Font Assets #24 is clean, Runtime #135 is fully green, and Gallery #112 is fully green. M7.2 byte-stream storage is complete with Storage #53 / Core #254. M7.3a–j object services, including directories, composite objects, Aplet-owned persistence/Reset and native catalogs, are accepted, as are M7.4 common system services and M7.5 display/runtime services. Text/numeric/console behavior and media-change services are accepted, including four SD volumes and stable card identity on `7303a3d6`. Overall M7 is accepted at functional head `2988e1de`, including the capability ledger, timed historical client, owned CPU power policy and ordinary application termination. See [final acceptance](validation.md#m7-final-acceptance). The individually approved native Port 0/1/2, whole-calculator reset and reliable serial-number follow-ups remain on the [roadmap](roadmap.md).
