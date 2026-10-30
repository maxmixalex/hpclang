# API contracts and ownership

Use the installed `<hpclang/...>` headers for clean C services and their `.hpp`
companions for C++ helpers. Historical headers such as `<hpsys.h>` are source
adapters; their units and formatting rules can differ. Include only the headers
you use. Internal `runtime/core` headers and `hpclang::detail` are implementation
details, even though runtime sources accompany the source SDK.

Build the application and runtime with one SDK and its matching toolchain.
The common HPC1 launch ABI does not promise that opaque objects, C++ layouts or
internal runtime structures can be exchanged between separately built SDK
versions. Rebuild when changing SDK versions. UI names remain provisional until
UI milestone acceptance; these usage conventions do not freeze those names.

## Read the result before reading errno

Return values are specific to each operation. In particular, a positive result
is not universally an error, and zero is not universally an available item.

| Operation | Result contract |
| --- | --- |
| `hpclang_object_store_home/current/open_child` | Owned handle on success; null and `errno` on failure |
| `hpclang_object_store_get/put/remove/mkdir/rmdir` | Zero on success; `-1` and `errno` on failure |
| `hpclang_object_store_iterator_read` | `1` publishes an entry, `0` means end, `-1` sets `errno` |
| `hpclang_device_capability` | A named capability state, including unsupported and context-unavailable; `-1/EINVAL` for an unknown ID |
| `owned_stream::close()` | Underlying close result; consumes the handle even on failure; an empty owner succeeds |

Check `errno` only after a reported failure and save it before calling other
services. A successful call need not clear an earlier error. Check the actual
operation even after capability discovery: available media, free memory and
exclusive hardware ownership can change independently of model support.

For structures with `size` and `version` fields, initialize both using the
installed type and named version constant. This convention applies where the
header declares those fields; do not invent a prefix for other structures.
For example, iterate a caller-owned store without transferring its ownership:

```c
#include <hpclang/object-store.h>
#include <errno.h>

int count_entries(const struct hpclang_object_store *store, uint32_t *count)
{
    if (!count) {
        errno = EFAULT;
        return -1;
    }
    struct hpclang_object_store_iterator *it = hpclang_object_store_iterator_open(store);
    if (!it)
        return -1;
    uint32_t total = 0;
    struct hpclang_object_store_entry entry = {0};
    entry.size = sizeof entry;
    entry.version = HPCLANG_OBJECT_STORE_ENTRY_VERSION;
    int result;
    while ((result = hpclang_object_store_iterator_read(it, &entry)) > 0)
        ++total;
    int error = errno;
    hpclang_object_store_iterator_close(it);
    errno = error;
    if (result < 0)
        return -1;
    *count = total;
    return 0;
}
```

Do not pass the address of a live owned pointer directly to a C producer:
for example, `hpclang_object_store_get` clears its output before attempting a
read. Use a null temporary and replace the old owner only after success, or
use `owned_object::receive`. Output preservation is an individual function's
contract, not a universal property of `hpclang_` calls.

## Distinguish owners, views and lexical guards

| C++ helper | Lifetime and responsibility |
| --- | --- |
| `owned_object` | Owns an independent calculator-object snapshot, not a live native stack/store slot; `receive` preserves the old owner on failure |
| `owned_object_store` | Owns a logical directory handle; names are resolved again when used |
| `owned_object_store_iterator` | Owns copied metadata and may outlive the directory handle used to open it |
| `owned_stream`, `owned_stream_directory` | Own one close responsibility; destruction is best-effort cleanup, not proof that writes were saved |
| `owned_surface` | Owns an offscreen image; `get()` and `view()` borrow it |
| `surface_ref`, `display_surface()` | Borrow a surface; do not destroy borrowed LCD pages or retain them past their display session |
| `ui::declaration`, `ui::scope` | Borrow a UI context and manage lexical operations; they do not own or extend the context's lifetime |

Move an owner to transfer responsibility. `get()` borrows; `release()` transfers
responsibility to the caller. Never create two owners from the same pointer.
Do not call `reset(owner.get())` on the `unique_ptr`-based directory owners.
Keep a borrowed view within its owner's lifetime and reacquire it after owner
replacement. A moved-from owner is empty; the moved-to owner keeps the resource.

Close every `ui::scope` before committing its declaration. An uncommitted
declaration aborts on ordinary scope exit; `commit()` consumes the guard even
when the C commit reports an error. Do not also begin, abort or commit the same
candidate through the C API while the guard is active.

Ordinary return runs automatic C++ destructors. `exit()` and `_Exit()` do not
unwind those scopes. Use checked `close()` before reporting a file as saved;
runtime hardware restoration is separate from application persistence. The SDK
uses an embedded no-exceptions/no-RTTI profile: C++ wrappers keep the C service's
failure and model/context limits instead of converting failures into exceptions.

See [migration](migration.md) for exact numbers, raw-byte names versus UTF-8,
legacy formatting and retired raw interfaces; [declarative UI](declarative-ui.md)
for tree/effect/renderer lifetimes and resource budgets. Installed
`examples/object-cpp`, `examples/storage-cpp` and `examples/declarative-ui-cpp`
demonstrate these contracts with normal SDK builds.
