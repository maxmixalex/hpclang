# Owned calculator objects in C++

This C++17 example constructs an integer snapshot, reads it while building a
replacement, and releases the result automatically on return. It performs no
native stack or variable mutation and works without an RPL stack on HP39.

Build it as an external application using the installed SDK and its ARMv4T
toolchain, following the SDK getting-started guide. It returns success without
drawing a screen. `owned_object::receive()` accepts any ordinary C snapshot
producer through a lambda; errors retain the old snapshot and `errno`.
