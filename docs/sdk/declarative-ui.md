# Declarative UI: declaration core

The current M8.2 slice provides bounded declaration storage, node identity,
row/column layout, button actions, focus traversal and controlled text edits.
Physical-key routing, effects and rendering are still being implemented.
The interface below is a working application sketch; its names are not yet a
frozen UI API.

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
Styles are opaque numeric inputs for a future renderer; this core assigns no
font, color or widget appearance.

## Storage and frame boundaries

Choose a `hpclang_ui_config` with a nonzero node capacity and a text byte budget.
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

Focus survives reordering by lifetime ID. If its node disappears or becomes
disabled, the next surviving eligible node in the old order is preferred,
then the previous one, then the first eligible new node. Initial focus chooses
the first eligible node. `hpclang_ui_focus_step` traverses declaration order in
either direction and wraps. Explicitly clearing focus persists across commits
until the application selects a node or requests traversal. Focus/activation
operations are rejected while a candidate is being declared. Traversal currently
includes clipped controls; scroll-to-focus and physical keyboard routing remain
part of the subsequent interaction work.

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
Integration with the existing model-aware text-mode service remains pending.
