# Declarative UI from C++

This small counter uses the installed SDK's C++ declaration/scope guards and
the same C layout, input and surface renderer as the larger C device-manager
example. Up/down move focus; Enter activates Add one, Reset or Quit. ON also
returns to the calculator. The counter uses unsigned arithmetic and wraps at
the type's maximum.

Configure with the installed SDK's toolchain and `hpclang_DIR`, as described
in the SDK getting-started guide. Build target `declarative_ui_cpp` and
`declarative_ui_cpp_launchers`. The generated application uses explicit ASCII
font packaging and builds without exceptions or RTTI.

`counter` is an ordinary reusable component function. Early returns close its
scope; a failed view aborts the candidate without replacing the accepted tree.
The example keeps C context and display-session teardown explicit. Its
`owned_ui_surface` adopts the C renderer and releases it on ordinary scope exit;
`get()` only borrows it for the C drawing calls. The lexical guards borrow the
UI context and do not own it. Fonts must outlive the renderer. Do not replace
or destroy its owner from a rendering callback, and use normal return rather
than `exit()` when relying on automatic C++ cleanup.
