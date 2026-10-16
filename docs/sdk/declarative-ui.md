# Declarative UI: declaration core

The current M8.2 slice provides bounded declaration storage, node identity,
row/column layout, button actions, focus traversal, controlled text edits and
model-aware physical-key routing, revision-driven canvas callbacks and damage
tracking and bounded asynchronous effects. A surface/font renderer and integrated
loading/error/retry application passed six-model Direct and Stock acceptance,
and its C implementation is integrated into main at `a7692b4b`. The C++ helpers
and compact-storage changes below also pass formal CI and are integrated at `e6f04dbd`.
The interface below is a working application sketch; its names are not yet a
frozen UI API.

Node metadata is selected by `kind`: read `capacity`, `cursor` and `anchor`
for inputs; `paint`, `canvas_state` and `revision` for canvases; and `value`
for toggles. These fields share storage rather than reserving every component's
state in every node. Rebuild applications and custom renderers together with
the SDK when upgrading this evolving UI interface.

The full device-manager pool (48 nodes, 512 text bytes and one effect) now
uses 13,692 bytes on ARM, down from 16,764. Layout phases reuse temporary
fields and derive transparent-scope ancestry from the declaration chain.
The complete C++/C UI fixture passes a local Direct experiment with its writable
arena limited to 32 KiB, including both display pages, theme/native-font changes,
physical input and independent pixel checks. This experiment does not replace
Stock acceptance. The production build now also passes all six Direct and all
six Stock targets with independent state, waveform, theme and font checks.

The installed `examples/declarative-ui` application combines these interfaces
into the original device-manager patterns. It uses two status-card instances,
conditional connection controls, a controlled filter, keyed insertion/reversal,
a simulated asynchronous scan with failure/retry/cancel, and a revision-driven
waveform. Its host flow test exercises the actual application update/view code.
The example's `demo_renderer` keeps palette/font selection outside its view and
update functions. Build options select an inverted palette and a native default
font with an explicit packaged fallback. Host flow tests run both palettes at
2/4/16 levels and replace the renderer without losing the focused node's lifetime.
The calculator test adds a second-theme LCD checkpoint, independently checks
the surrounding UI's inverse pixels and the unchanged custom canvas, then
continues keyboard-driven scan/cancel actions with the replacement renderer.
An additional checkpoint switches to the native default font when accessible,
or keeps the packaged fallback otherwise. Its verifier checks the waveform's
new position after font-dependent measurement; Stock must expose its native
eight-row font. These font/theme checks pass locally on all six Direct and all
six Stock models, followed by further keyboard actions and normal system return.
The installed SDK builds its ARM image and all six model launchers. The complete
application flow passes Direct and Stock execution on all six models with
electrical key injection, display checkpoints and normal return. UI is included
in the shared Runtime E2E suite matrix; its independent verifier checks captured
states and waveform sample coordinates. These UI jobs passed in
[Runtime CI on `a18302a3`](https://github.com/maxmixalex/hpclang/actions/runs/38410381089).
The broader memory/update-cost acceptance remains open; the measured example
pool and the limits of emulator timing evidence are documented below.

An application owns its model and describes the current view with ordinary C
functions, loops and conditions. Separate component instances scope their keys:

```c
#include <hpclang/ui.h>

struct item { uint32_t id; const char *title; };

static void ItemList(struct hpclang_ui *ui, uint32_t key,
                     const struct item *items, size_t count)
{
    HPCLANG_UI_WITH(ui, key, HPCLANG_UI_COMPONENT, 0) {
        HPCLANG_UI_WITH(ui, 1, HPCLANG_UI_COLUMN, 0) {
            for (size_t i = 0; i < count; ++i) {
                HPCLANG_UI_WITH(ui, items[i].id, HPCLANG_UI_KEY, 0) {
                    hpclang_ui_text(ui, 1, 0, items[i].title);
                }
            }
        }
    }
}
```

Call `ItemList(ui, 10, ...)` and `ItemList(ui, 20, ...)` to create independent
instances. Use persistent item IDs, not their current positions. Inserting or
reversing the list preserves surviving node IDs. A same-key kind change creates
a new lifetime, as does removal followed by reappearance in a later frame.
Styles are opaque numeric inputs for the selected renderer; this core assigns no
font, color or widget appearance.

## Storage and frame boundaries

Choose a `hpclang_ui_config` with a nonzero node capacity, a text byte budget
and an independent `effects` capacity. Zero effects disables asynchronous tasks
without reserving task slots for ordinary labels or layout containers.
`hpclang_ui_storage_size(config)` reports the complete allocation size. Supply
that many bytes with `max_align_t` alignment, for example from `malloc`, to
`hpclang_ui_init`. Keep the storage alive until the context is no longer used.
A zero text budget is valid for a tree with no text declarations.

There is no allocation during declaration. Two fixed node/text arenas hold
the accepted tree and the candidate tree; an additional text-budget-sized buffer
holds an edit proposal. `hpclang_ui_begin` starts a candidate;
`hpclang_ui_commit` accepts it, or rejects the whole candidate on the first
declaration error. `hpclang_ui_abort` discards it explicitly. A rejected frame
leaves previously committed metadata and text intact.

Duplicate sibling keys report `EEXIST`; node/text exhaustion reports `ENOSPC`.
These errors are sticky until the candidate is discarded. Unbalanced or
out-of-order scopes report `EINVAL`. Starting a second frame reports `EBUSY`
without aborting the existing declaration. Lifetime/frame counters reject
overflow rather than allowing stale identities to alias new ones.

The implementation compares complete parent lifetime/key/kind relationships;
it does not use hash equality as proof of identity. Reconciliation currently
uses bounded linear scans, with quadratic worst-case work in the configured
node count. Target memory/update budgets remain an M8 acceptance task.

`hpclang_ui_node_count` and `hpclang_ui_node_at` expose the accepted declaration
in traversal order. Node IDs are lifetime tokens; a parent ID of zero denotes
the root. Text is copied into the context. Returned pointers remain valid until
the next successful commit or reinitialization. Declaring the view does not
draw, run asynchronous work or modify the application's model.

## Scope control flow

The C macro uses the supported Clang cleanup attribute. Normal completion,
`break`, `continue`, `return` and a `goto` out of a scope close it automatically.
Those operations do not commit a frame: the caller still decides whether the
resulting declaration should be accepted. Explicit scope begin/close functions
are also available.

Do not jump into a declaration scope or use `longjmp` across it. If the
application performs nonlocal recovery, explicitly abort the candidate before
starting another. Cleanup handles from a discarded frame cannot close scopes
belonging to a newer frame.

Host tests exercise independent components, insertion/reversal, removal,
kind changes, duplicate keys, interruption, storage exhaustion and recovery.
This establishes declaration behavior, not calculator UI/rendering acceptance.

## Layout and button actions

Call `hpclang_ui_layout` after declaring and closing all scopes, before commit.
Pass a viewport and a measurement callback that returns preferred leaf sizes,
container padding/gaps and optional growth weights. Components and keyed scopes
are transparent: they add identity without inserting gaps or layout boxes.
Root children form a column.

Children stretch across the parent's cross axis. Positive growth weights share
spare main-axis space in declaration order using integer division; the last
weighted child receives the remaining pixels. Preferred content is not shrunk
when it overflows. Each visible node exposes its uncut bounds and an ancestor-
clipped rectangle; rendering must honor that clip. Transparent identity nodes
have empty bounds. Padding cannot produce negative content dimensions.

Measurement callbacks may inspect the node but must not mutate/reenter the
context. Invalid metrics, callback failure and coordinate overflow reject the
candidate. After layout, additional declarations are rejected; begin a new
candidate to change the tree. Layout itself performs no drawing.

`hpclang_ui_button` declares a caption, action `{type, arg}` and enabled state.
`hpclang_ui_activate` returns the focused button's action and stable source ID;
the application performs its update and declares the resulting model again.
Disabled buttons retain identity but cannot receive focus or activation.

`hpclang_ui_toggle` declares a controlled boolean with a caption, checked state,
action and enabled state. Both checked and enabled must be zero or one.
Activation emits `HPCLANG_UI_CHANGE` with the proposed boolean in `event.value`;
it leaves the accepted view unchanged. The application updates its model and
declares another frame to accept, reject or transform the proposal. Further
edits and activations return `EBUSY` until a successful commit; aborting a frame
does not consume the proposal. Stable keys preserve the control's identity
when a list is reordered. The surface renderer uses `[x]` / `[ ]` markers with
equal reserved width, alongside the same focus and disabled styles as buttons.

Focus survives reordering by lifetime ID. If its node disappears or becomes
disabled, the next surviving eligible node in the old order is preferred,
then the previous one, then the first eligible new node. Initial focus chooses
the first eligible node. `hpclang_ui_focus_step` traverses declaration order in
either direction and wraps. Explicitly clearing focus persists across commits
until the application selects a node or requests traversal. Focus/activation
operations are rejected while a candidate is being declared. Traversal includes
clipped controls and reveals the selected control through its scroll ancestors,
from the innermost viewport outward. Removing the focused item chooses the
documented fallback and reveals it in the committed layout. Controls clipped by
a non-scrolling container retain that container's clipping policy.

## Controlled text edits

`hpclang_ui_text_input` declares a single-line UTF-8 model value, its capacity
including the trailing NUL, a change action and enabled state. Capacity must
fit the context's text budget. Malformed UTF-8, overlong encodings, surrogates
and control characters reject the candidate. Initial selection is collapsed
at the end of the value. `hpclang_ui_select` changes the focused input's anchor
and cursor using byte offsets at Unicode scalar boundaries.

`hpclang_ui_edit` takes an existing `hpclang_input_action`. It reuses the bounded
text editor, including whole-scalar cursor/deletion behavior. Inserting over a
selection replaces it; Backspace/Delete remove the selected range once. A value
change emits `HPCLANG_UI_CHANGE` with the declared action, source lifetime ID,
proposed text and selection. The displayed value and application model remain
unchanged. The event text belongs to the UI context until the next edit call or
reinitialization; copy it if the application needs a longer lifetime.

Declare and successfully commit the application's response before issuing
another edit. An identical value rejects the proposal and preserves the old
selection. An accepted or transformed value uses the proposed selection,
clamped to scalar boundaries in the new text. Removal retires the input's
lifetime. A failed or aborted declaration leaves the proposal outstanding so
the application can retry without silently acknowledging it.

Cursor-only navigation emits `HPCLANG_UI_SELECTION` and changes no model value.
Enter/accept and cancel emit distinct `HPCLANG_UI_ACCEPT`/`HPCLANG_UI_CANCEL`
events. Activating a focused input emits accept; activating a button emits
`HPCLANG_UI_ACTIVATE`. Inspect the event kind before applying a model update.
No-op edits return zero without replacing the event. Capacity/argument errors
preserve the displayed value and selection.

The direct edit API does not consume physical keys or implement Alpha rules.
For keyboard operation, initialize a caller-owned `hpclang_text_mode` for the
active model and feed every ordered event, including releases, to
`hpclang_ui_key`. It shares native mode/key decoding and atomic delimiter-pair
insertion with the existing line editor. Selection replacement occurs before
capacity checking, so a pair can replace a full selected field without leaving
half a delimiter behind.

Up/down traverse controls. On a button, left/right traverse and Enter activates;
on an input, left/right move/collapse selection, Enter accepts and ON cancels.
Zero means no application action, not necessarily no UI change: focus or modifier
state may have advanced. If an edit proposal is outstanding, `EBUSY` consumes
no physical state; commit its response and retry the same event. An `ENOSPC`
edit still consumes native one-shot modifier state, matching the line editor.

Host tests feed all six model layouts, including one-shot Alpha, release events,
delimiter pairs and capacity recovery. The interactive calculator example also
passes physical GPIO/display integration on all six Direct and Stock models;
this does not replace the host matrix's exhaustive edit/error cases.

## Canvas and damaged regions

`hpclang_ui_canvas` declares a paint callback, borrowed application state, a
revision and preferred height. The callback descriptor is copied into the node;
keep its state alive through painting or removal. Increment the revision when
mutating the state in place. The layout engine supplies bounds and a clipped
paint region. Canvas callbacks must obey that clip; a concrete renderer owns
the drawing context and its display/surface access.

After successful declaration/layout/commit, `hpclang_ui_paint` invokes the
selected painter only when there is damage. Nonempty declarations without
layout are rejected. Changes to visible text, style, enabled state, selection,
focus, geometry or canvas revision add old and new affected regions. Removal
therefore clears the old pixels. Exposure and theme/backend changes use
`hpclang_ui_invalidate`. Identical idle declarations produce no drawing calls.

Damage is stored as one bounded union, using wide coordinates internally and
clipping to the complete target surface before callbacks. This is conservative:
separate changes can also redraw the space between them. It does not allocate
a growing rectangle list. Canvas callbacks run when their revision changes or
their region intersects damage, including movement, resize or exposure.

A painter supplies `begin`, `node` and `end`, plus a context passed to canvas
callbacks. `begin` prepares/clears the damaged region. Ordinary nodes are painted
in declaration order; transparent identity scopes are skipped. After successful
`begin`, `end` always runs. It receives `completed=0` after drawing failure so
the backend can release resources without publishing an incomplete frame.
All damage remains pending after any failure and is retried on the next call.
Callbacks cannot mutate/reenter the UI. Successful drawing preserves errno.

This interface does not acquire a display mode, expose framebuffer addresses
or publish an LCD page. The concrete renderer must enforce clipping and manage
its own surface ownership, page contents and presentation. A host pixel backend
currently verifies idle behavior, partial updates, move/delete cleanup,
failure/retry, focus/cursor invalidation and clipping. The standard grayscale
renderer now uses an internal surface lease that intersects nested write clips
without copying pixels or changing the
owner's descriptor. Clear, geometry and font drawing respect the lease; aliased
blits retain overlap semantics. These are renderer building blocks, not a new
public framebuffer or mutable global clipping API.

## Surface/font renderer

`hpclang/ui-surface.h` provides an explicitly owned renderer. Create it once with
the viewport dimensions, 2/4/16 levels, a background intensity and a style table.
Each style supplies a font, foreground/background/disabled/accent intensities,
padding, container gap, grow weight and minimum dimensions. Style IDs index the
copied table; fonts remain borrowed. Invalid fonts/styles fail explicitly.

Use `hpclang_ui_surface_measure` as the layout callback, with the renderer as
context. Font measurement uses the same UTF-8 decoder, fallback and newline
rules as drawing, including proportional advances. Text is top-left aligned;
buttons and inputs have outlines, focus uses the accent color, and disabled
controls use the disabled color. Focused inputs invert selected text and scroll
horizontally to retain the cursor. Vertical scroll scopes provide container/list
scrolling as described below. Changing font/geometry themes requires layout again.

`hpclang_ui_surface_render(renderer, ui)` returns one after an update, zero
when clean, or minus one with errno. The renderer owns two raster pages: each
update starts from the accepted image, repaints damage into the other page and
accepts it only after every callback succeeds. Failed canvas drawing leaves the
accepted image unchanged and the UI retains damage for retry. No per-frame
allocation occurs. The two pixel buffers use
`2 * height * ceil(width * log2(levels) / 8)` bytes, plus descriptors/styles;
the declaration pool and any LCD DMA pages are additional memory.

For this backend, Canvas `paint_context` is a temporary clipped
`struct hpclang_surface *`. Use ordinary surface/font drawing APIs. Even clear
is restricted to the callback's clip. Do not destroy or retain that borrowed
surface. A custom painter can wrap Canvas dispatch using its optional `canvas`
callback; without it, the core retains direct `canvas_context` dispatch.

After successful rendering, `hpclang_ui_surface_copy(renderer, target)` copies
the complete accepted image to a same-sized surface. This is deliberately
separate from LCD acquisition and publication:

```c
int changed = hpclang_ui_surface_render(renderer, ui);
if (changed > 0) {
    struct hpclang_surface *back = hpclang_display_surface();
    if (hpclang_ui_surface_copy(renderer, back) == 0) {
        /* Handle timeout/failure; retain the accepted image for retry. */
        hpclang_display_present(100000);
    }
}
```

The example assumes an acquired display session. A failed present requires an
explicit retry even if the UI is now clean. Reborrow the back surface after each
successful present. The complete copy preserves unchanged pixels when LCD pages
exchange; copying only current damage would leave older pixels in the next back
page. Rendering itself still skips unchanged controls/canvases. Stock's borrowed
surface can also receive a copy without acquiring a display session.

Use one active painter per UI damage stream. Explicitly invalidate the viewport
when switching back to a previously used renderer or after exposure. Destroy
the renderer after callbacks finish; it does not own the UI or display session.
Host pixel tests cover all three level counts, canvas confinement, partial
updates, failed-frame retry, complete-page copying, input selection and an
inverted theme. Six-model execution and low-memory acceptance pass, with the UI
Runtime CI jobs accepted on `a18302a3`. Broader memory/update budgets remain
part of the renderer milestone's outstanding acceptance work.

The device-manager example budgets 48 nodes, 512 text bytes and one asynchronous
task for its complete eight-device view (46 live nodes). Its ARM declaration pool
is 13,692 bytes after compact-storage changes;
renderer pages, LCD DMA pages, application state and the stack are additional.
Each device row directly carries its stable key; a separate identity wrapper is
unnecessary. Independent task sizing avoids one task slot per visual node.
Host allocation
fault injection covers every renderer-construction allocation, checks cleanup
and recovery, and proves declaration/layout/render/copy still work with further
allocation disabled. Full Stock flows now pass on HP39g+ and HP39gs with this
pool, physical keyboard handoff and normal return. A local HP39g+ Stock rerun
at `413e5891` reports the current 13,692-byte pool, 46 nodes and 5,614 microseconds
for the initial paint; the paired Direct run reports 5,888 microseconds.
These single emulator observations measure only the initial renderer call,
excluding declaration, layout, commit and presentation. They are not hardware
timing guarantees or worst-case interaction costs. The displayed `RW` value is
the launcher's read/write arena size, not measured peak heap consumption; the
Direct launcher supplies a larger arena than Stock. The extended font/theme
matrix also passes on all six Stock models.

For the ARM ABI, the renderer with the example's two styles requests 188 bytes
for its descriptors and copied styles, plus its two raster buffers:

| Viewport | 2 levels | 4 levels | 16 levels |
| --- | ---: | ---: | ---: |
| 131 × 64 | 2,364 bytes | 4,412 bytes | 8,636 bytes |
| 131 × 80 | 2,908 bytes | 5,468 bytes | 10,748 bytes |

These are requested heap bytes, excluding allocator bookkeeping and alignment
overhead. Add the declaration pool, application state, stack and any owned LCD
DMA pages to obtain an application budget. LCD pages use the hardware scan
width and row count, which need not equal this renderer's logical viewport.
Host allocation tests check the two-buffer cost at every size/level combination
above, independently of host descriptor sizes, and retain allocation-failure
and allocation-free frame checks. Peak whole-application memory and complete
update-cost acceptance remain open.

## Vertical scrolling

Use `HPCLANG_UI_SCOPE(hpclang_ui_scroll_begin(ui, key, style, height))` to declare
a vertical viewport whose children form a column. Height is its preferred outer
height, including padding; the style's grow weight can add space. Content keeps
its measured height and is clipped to the viewport. Scroll scopes can nest and
can contain transparent component/key scopes.

The accepted node's `scroll_y` reports its pixel offset. Stable scope identity
retains the offset across declarations; shortened content clamps it. Focus
navigation and committed focus fallback reveal the selected control from the
innermost viewport outward. An oversized control aligns its top edge. Scrolling
updates geometry and damage without modifying application data or node IDs.
Failed declarations and coordinate overflow preserve accepted geometry/focus.

`hpclang_ui_scroll_to(ui, id, offset)` supports explicit browsing, including
text-only content. The nonnegative requested offset is clamped to the available
range. It does not change focus. A subsequent commit or focus operation reveals
focus again; explicitly clear focus for independently browsed text. Negative
offsets and non-scroll node IDs are errors. Calls require a laid-out committed
frame outside painting, declaration and effect callbacks.

This implementation retains every declared node in the bounded pool; it does
not virtualize large lists. Host geometry and pixel tests cover nested reveal,
wraparound, content shrink, manual offsets, clipping and overflow rollback.

## Asynchronous effects and cleanup

Queue work with `hpclang_ui_effect_start` outside declaration. The UI copies the
provider's `begin`, `poll` and `end` callbacks; its context remains borrowed until
`end`. Each context has `config.effects` task slots. Queue exhaustion returns
`ENOSPC` without starting work or changing the output token. Successful requests
receive monotonically increasing tokens; tokens are never recycled within a
context's lifetime.

An owner is a committed node lifetime ID, or zero for application-wide work.
Removing an owner marks its work cancelled at commit. Reintroducing the same
key creates a new lifetime and cannot receive the retired task's result.
Explicit cancellation uses `hpclang_ui_effect_cancel`. Cancellation is drained
by the next poll or shutdown; retain provider resources until `end` runs.

Call `hpclang_ui_effect_poll` from the event loop. A round-robin scan visits at
most the fixed slot count and emits at most one completion per call. Providers
must perform bounded, nonblocking work: this scheduler does not turn blocking
I/O into a background thread. `poll` returning zero keeps work pending; one
produces `HPCLANG_UI_EFFECT_READY`; minus one with errno produces
`HPCLANG_UI_EFFECT_ERROR`. The UI event copies the owner, action, request token
and scalar result/error. Provider errors are events, not failures of the UI
poll call. The application updates its own loading/error/ready state and queues
a new request to retry.

`end` runs exactly once, including begin failure and cancellation before begin.
Cancelled tasks produce no completion event. All callbacks prohibit mutation
or reentry into the same UI. Call `hpclang_ui_shutdown` before freeing or
reinitializing UI storage; it discards any candidate frame, immediately cancels
remaining tasks and is idempotent. A closed context cannot accept more work.

Ordinary `exit()` also cancels registered tasks through an exit handler; `_Exit`
skips this language cleanup. Both UI storage and provider contexts must still
exist when the handler runs. Stack storage going out of scope requires explicit
shutdown; automatic exit cleanup is not a replacement for object lifetime
management. The scheduler uses fixed task storage; first use also registers
one process-wide C-library exit handler.

Host tests cover fair progress, failed begin/poll, explicit cancellation,
owner removal/reappearance, exhausted pools, shutdown and actual process-exit
cleanup. The integrated device-manager E2E also validates interactive loading,
failure and retry on all six Direct and Stock targets; implementation acceptance
is recorded in the roadmap.

## C++ declaration guards

`<hpclang/ui.hpp>` adds C++17 lexical guards over the same C context. They do
not allocate, throw, commit implicitly or introduce a second state model.
Check construction before declaring children and explicitly commit only after
all container guards have left scope:

```cpp
#include <hpclang/ui.hpp>

int declare_status(hpclang_ui *ui, const char *status)
{
    hpclang::ui::declaration frame{ui};
    if (!frame)
        return -1;
    {
        hpclang::ui::scope column{ui, 1, HPCLANG_UI_COLUMN};
        if (!column || hpclang_ui_text(ui, 1, 0, status))
            return -1;
    }
    // Apply hpclang_ui_layout here when using the surface renderer.
    return frame.commit();
}
```

Early return closes nested scopes and aborts the candidate, preserving the
previous committed view. A failed nested declaration guard does not abort
another guard's active frame. Guards cannot be copied or moved, so lexical
nesting and cleanup ownership remain explicit. They borrow the C context;
its storage must outlive them. Do not call C begin/abort/commit directly on
the guarded declaration. Existing C leaf declarations, layout, input and
effects remain usable unchanged. These guards do not own UI storage or
perform shutdown; the existing context/provider lifetime rules still apply.

Host tests exercise early return, two component instances, stable identity
after reordering, duplicate frame acquisition and fixed-pool failure, with
exceptions and RTTI disabled. This is the first M8.5 helper slice; complete
broader owned-resource wrappers and integrated release acceptance remain open.

The installed `examples/declarative-ui-cpp` counter demonstrates these guards
with keyboard navigation and the ordinary surface renderer. The SDK relocation
gate builds it outside the installed tree, checks its ARM/HPC1 output and all
six launchers, and packages all three HP39 Aplets when the packaging dependencies
are supplied. It complements the larger C device-manager example.
The UI E2E also links the counter's actual view and checks guard rollback,
stable focus across model updates and owned-image moves before executing the
full device-manager flow. All six Direct and all six Stock models pass locally;
all six CI workflows also pass at `e6f04dbd`, merged into main. The integrated
branch passes 84 host targets and the installed SDK relocation/HP39 packaging
gate. Broader owned-resource wrappers and migration requirements remain open.

For offscreen drawing, `<hpclang/display.hpp>` also provides a C++20
`hpclang::owned_surface{width, height, levels}`. Check its Boolean conversion
before obtaining a borrowed `view()` or `get()`. The owner releases pixels
on destruction, supports move construction/assignment, and cannot be copied.
`reset()` releases the image; `release()` transfers destruction responsibility
to a C caller. Views must not outlive the image. Calculator display pages
remain borrowed and cannot be adopted by this owner. Allocation failure follows
the C constructor's empty-result contract and does not throw.
