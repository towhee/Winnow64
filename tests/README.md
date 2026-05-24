# Winnow test suite

Run after significant changes and before a release. Three layers, driven by
CMake/CTest + one script.

| Layer | What it checks | Runner |
|-------|----------------|--------|
| **unit** | Pure logic — byte parsers, path helpers (Qt Test) | `ctest -L unit` |
| **smoke** | Real app launches, opens a folder, loads images, no crash | `ctest -L smoke` |
| **tsan**  | No data races during folder-load concurrency | `tests/tsan/run_tsan.sh` |

## Quick start (unit + smoke)

```sh
CMAKE=/Users/roryhill/Qt/Tools/CMake/CMake.app/Contents/bin/cmake

# Build the app (smoke launches it) + the unit-test binaries:
$CMAKE --build build/mac-debug --target Winnow tst_byteops tst_pathutils

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
  (default 8000), then exits `0` if `dm->rowCount() > 0`, else `2`. It enables
  `QStandardPaths` test mode, so it **never touches your real `settings.ini`**, and
  bypasses the single-instance forwarding so it always starts fresh.
- **Fixtures** (`tests/fixtures/images/`) are tiny generated images (one JPEG with
  EXIF). Regenerate with `python3 tests/fixtures/generate_fixtures.py` (needs Pillow).

## Adding a unit test

1. Create `tests/unit/tst_<thing>.cpp` (a `QObject` with `private slots`, ending in
   `QTEST_GUILESS_MAIN(...)` + `#include "tst_<thing>.moc"`).
2. Add `winnow_add_unit_test(tst_<thing> unit/tst_<thing>.cpp)` to `tests/CMakeLists.txt`.
3. If it needs production code beyond the current closure, append the `.cpp` to
   `WINNOW_CORE_TEST_SOURCES`.

## Build-system note

Tests assume CMake. Disable the whole suite with `-DWINNOW_BUILD_TESTS=OFF`
(does not affect the `Winnow` app target).
