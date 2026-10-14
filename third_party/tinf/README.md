# tinf launcher decoder

`tinflate.c` and `tinf.h` are unchanged files from
[jibsen/tinf](https://github.com/jibsen/tinf/tree/57ffa1f1d5e3dde19011b2127bd26d01689b694b/src),
commit `57ffa1f1d5e3dde19011b2127bd26d01689b694b`.
Their embedded zlib license notices apply.

Only the bounded raw DEFLATE decoder is linked into the private ARMToolbox
bootstrap. No decoder is added to the application runtime. Launcher packaging
uses Python's standard `zlib` module; installed SDK builds need no download.
Public HPC1 files remain uncompressed. The private embedded HPCZ representation
retains the original image size, entry offset and static RAM size, with a raw
DEFLATE stream in place of the image bytes.
