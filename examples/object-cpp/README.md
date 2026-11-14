# Owned calculator objects in C++

This C++17 example constructs an integer snapshot, reads it while building a
replacement, and releases the result automatically on return. It performs no
native stack or variable mutation and works without an RPL stack on HP39.
Where native HOME is available, it also opens an owned directory iterator,
closes the directory handle and reads the independent metadata snapshot.
Unavailable native services (`ENOSYS`) skip this optional part; other errors
are reported through the application's return value.

Build it as an external application using the installed SDK and its ARMv4T
toolchain, following the SDK getting-started guide. It returns success without
drawing a screen. `owned_object::receive()` accepts any ordinary C snapshot
producer through a lambda; errors retain the old snapshot and `errno`.
`object-store.hpp` supplies standard `unique_ptr` owners for directory handles
and iterators. Their destruction closes the resource while preserving `errno`.

`port.hpp` adds the same ownership pattern for native Port 0 metadata. The
example enumerates its independent snapshot where available, without changing
port objects. Errors other than unsupported context remain failures.
