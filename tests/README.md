# Winnow test suite

Run after significant changes and before a release. Three layers, driven by
CMake/CTest + one script.

| Layer | What it checks | Runner |
|-------|----------------|--------|
| **unit** | Pure logic — byte parsers, path helpers, IFD rational decode, EXIF tag table, IFD directory walk (Qt Test) | `ctest -L unit` |
| **smoke** | Real app launches, opens a folder, loads images, no crash | `ctest -L smoke` |
| **metadata** | Full Metadata pipeline parses a real committed camera file (make/model/dims) | `ctest -L metadata` |
| **tsan**  | No data races during folder-load concurrency | `tests/tsan/run_tsan.sh` |

## Quick start (unit + smoke)

```sh
CMAKE=/Users/roryhill/Qt/Tools/CMake/CMake.app/Contents/bin/cmake

# Build the app (smoke launches it) + the unit-test binaries:
$CMAKE --build build/mac-debug --target Winnow tst_byteops tst_pathutils tst_rational tst_exiftags tst_ifd

# Run everything registered with CTest (unit + smoke):
ctest --test-dir build/mac-debug --output-on-failure
```

Filter by layer with `ctest -L unit` or `ctest -L smoke`. Unit tests run headless
(`QT_QPA_PLATFORM=offscreen`); the smoke test runs the real app via `--selftest`.

## ThreadSanitizer layer

A full instrumented build of the app, exercised via `--selftest`:

```sh
tests/tsan/run_tsan.sh        # configures mac-tsan, builds, runs, scans for races
```

Race detection is by **log scan** (the self-test ends with `std::_Exit`, which
skips TSan's atexit summary; per-race warnings still print live). Suppressions
come from `tsan.supp` in the repo root.

## How it's wired

- **Unit tests** (`tests/unit/`) link the production sources directly — no copied
  code. `tests/CMakeLists.txt` lists the link closure in `WINNOW_CORE_TEST_SOURCES`;
  add to it only if the linker reports a missing symbol.
- **Smoke / TSan** use the hidden `--selftest <folder>` flag added to `main.cpp`.
  It opens the folder via the normal startup path, settles for `WINNOW_SELFTEST_MS`
  (default 8000), then exits `0` if `dm->rowCount() > 0`, else `2`.
- **Metadata** uses the hidden `--metatest <file>` flag → `MW::runMetaTest`, which
  reads the file through the same `Metadata::loadImageMetadata` the app's Reader
  uses (so it's genuinely end-to-end through every format parser, not a rebuilt
  subset) and exits `0` if make/model/dimensions parsed. Expected make/model are
  passed via `WINNOW_METATEST_MAKE` / `WINNOW_METATEST_MODEL` (set in the CTest
  registration), so app code stays generic and the fixture's identity lives here.
- All three flags enable `QStandardPaths` test mode (so they **never touch your real
  `settings.ini`**) and bypass single-instance forwarding so they always start fresh.
- **Fixtures** (`tests/fixtures/images/`): tiny generated images (`sample0*.{jpg,tif,png}`,
  one JPEG with EXIF — regenerate via `python3 tests/fixtures/generate_fixtures.py`,
  needs Pillow) plus `sample_nikon_d700.jpg`, a small **real** camera JPEG committed
  for the metadata layer.

## Adding a unit test

1. Create `tests/unit/tst_<thing>.cpp` (a `QObject` with `private slots`, ending in
   `QTEST_GUILESS_MAIN(...)` + `#include "tst_<thing>.moc"`).
2. Add `winnow_add_unit_test(tst_<thing> unit/tst_<thing>.cpp)` to `tests/CMakeLists.txt`.
3. If it needs production code beyond the current closure, append the `.cpp` to
   `WINNOW_CORE_TEST_SOURCES`.

## Build-system note

Tests assume CMake. Disable the whole suite with `-DWINNOW_BUILD_TESTS=OFF`
(does not affect the `Winnow` app target).
