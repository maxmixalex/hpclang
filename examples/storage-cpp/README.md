# Owned byte streams in C++

This C++17 example uses `hpclang/storage.hpp` to own a standard `FILE*` and a
stream-directory handle without exceptions, RTTI or allocations in the owners.
It opens the default FAT volume's root directory and `README.TXT`, reads one
byte, checks read errors, and explicitly checks both close results. It returns
zero on success and one on failure, without drawing a screen. An empty file is
valid. The example performs no file creation, writes or deletion.

Run it on HP49g+ or HP50g with a readable FAT SD card containing `README.TXT`
in the default volume's root. A missing file, absent card or unsupported model
produces a failure return; building launchers for all six models does not imply
that every model has an SD byte-stream backend. Names follow the C storage
service's encoding rules.

Copy this directory from the installed SDK and configure it with the installed
ARMv4T toolchain and `hpclang` CMake package, as shown in `docs/getting-started.md`.
The target is `storage_cpp`; the normal build produces its ELF, HPC1 image and
six-model launchers. No source-checkout path is required.

`get()` borrows the handle. Move transfers ownership, and `release()` transfers
close responsibility back to C. Do not adopt a borrowed handle or a standard
stream. `close()` consumes a handle even when it returns an error. Destruction,
`reset()` and move assignment discard close errors while preserving `errno`;
check `close()` explicitly when successful persistence matters. Ordinary return
destroys local owners, whereas `exit()` and `_Exit()` do not unwind C++ scopes.
