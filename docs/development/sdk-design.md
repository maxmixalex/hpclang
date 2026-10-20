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
from a clean HP49g+/HP50g Stock boot. It validates the six native bank identities and
an empty journal bank, preserves the target sector in a CRC-protected journal,
then erases that sector in the copied Flash image. The original state and any
existing destination are preserved. The copied state must undergo hardware
reset before transfer so Stock rebuilds its catalog from the damaged Flash.
This is explicit offline fault injection, not a model of an erase/program pulse
interrupted while the chip is busy.
Targets may be aligned sectors anywhere inside the first user bank; the journal
must remain in a separate empty bank. This permits body-only faults that leave
the bank identity and earlier library headers intact. The configuration fixture
derives its Stock 2.15 library-229 callback sector from the saved catalog and
Flash header, verifies the expected bank mapping and rejects a target sharing
the bank/library header sector. It checks exact recovery, the complete catalog,
owned recovery state and an unchanged subsequent warmstart. Successful repair
does not by itself prove which earlier factory callbacks executed or were skipped.

`tests/e2e/port-recovery/run.py` orchestrates the fresh firmware boot, offline
fault preparation, hardware reset, USB transfer and independent verification.
It then performs another hardware reset and compares the complete native library
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
Production recovery also needs an entry that survives loss of HOME RAM; the
RAM-resident witness does not establish that property.

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

### Startup resource admission

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
