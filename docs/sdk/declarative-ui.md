# Declarative UI: declaration core

The first M8.2 slice provides bounded declaration storage and node identity.
Layout, interactive controls, effects and rendering are still being implemented.
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
the accepted tree and the candidate tree. `hpclang_ui_begin` starts a candidate;
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
