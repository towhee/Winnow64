# libtiff (vendored, Windows-only)

Prebuilt libtiff used by the **Windows** build of Winnow only. macOS gets libtiff
from Homebrew (`/opt/homebrew/opt/libtiff`) — see the `APPLE` block in the
top-level `CMakeLists.txt`. Nothing here is compiled or linked on macOS.

This used to be a git submodule of `https://github.com/vadz/libtiff.git` built
in-source with MSVC. That dragged the entire libtiff source tree (plus generated
MSVC project files) into the repo and left the submodule perpetually "dirty" on
every machine. It is now a plain tracked folder holding only the files Winnow
actually consumes.

## Contents

```
include/   tiff.h, tiffio.h, tiffvers.h, tiffconf.h   (public API headers)
lib/       tiff.lib                                    (import library)
bin/       tiff.dll                                    (runtime, deployed next to Winnow.exe)
```

## Provenance

- Source: `vadz/libtiff`, commit `cda4b06914040aae5302b4da511ea266dad8a104`
- Version: LIBTIFF 4.0.9 (per `tiffvers.h`)
- Built: Release, x64, MSVC (vc16)

The Release import library is linked for both Debug and Release configurations —
libtiff exposes a stable C ABI, so a single Release build is safe for both
(matching how zlib is handled). `tiff.dll` depends on `zlib.dll`, which is
deployed alongside it.

## Rebuilding / upgrading

1. Clone and check out the desired libtiff commit elsewhere (outside this repo).
2. Configure + build Release x64 with CMake/MSVC.
3. Copy the four public headers into `include/` (note `tiffconf.h` is generated
   into the build dir), `tiff.lib` into `lib/`, and `tiff.dll` into `bin/`.
4. Update the provenance block above.

Do **not** build in-source inside this folder — keep it to the vendored artifacts only.
