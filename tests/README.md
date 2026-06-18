# Winnow test suite

Run after significant changes and before a release. Three layers, driven by
CMake/CTest + one script.

| Layer | What it checks | Runner |
|-------|----------------|--------|
| **unit** | Pure logic — byte parsers, path helpers, IFD rational decode, EXIF tag table, IFD directory walk (Qt Test) | `ctest -L unit` |
| **smoke** | Real app launches, opens a folder, loads images, no crash | `ctest -L smoke` |
| **metadata** | Full Metadata pipeline parses a real committed camera file (make/model/dims) | `ctest -L metadata` |
| **tsan**  | No data races during folder-load concurrency | `tests/tsan/run_tsan.sh` |
| **soak**  | Races + memory growth under sustained folder bounce (load/navigate) | `tests/soak/run_soak.sh` |

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

## Soak layer

A long-running exercise of the **load/navigate pipeline** — the path where slow
races and memory growth hide. Driven by the hidden `--soaktest <folders...>`
flag (`main.cpp` → `MW::runSoakTest`): bounce between folders, reload each, and
ping-pong through its images, seeded for reproducibility. Each bounce prints a
probe line (`SOAK: bounce=… footprintMB=… imCacheMB=… dmRows=…`) so a climb can
be localized — footprint rising while `imCacheMB`/rows stay flat means a leak
*outside* the image cache.

```sh
tests/soak/run_soak.sh                                   # asan + tsan, 60 s each
WINNOW_SOAK_MS=3600000 SOAK_PASSES=tsan tests/soak/run_soak.sh   # 1-hour race hunt
SOAK_PASSES=leaks tests/soak/run_soak.sh                 # Apple `leaks` leak hunt
```

Three passes (`SOAK_PASSES`, default `"asan tsan"`):

- **asan** — `mac-asan` build. Catches use-after-free / heap-overflow /
  double-free live. The reliable **memory-safety** oracle on macOS.
- **tsan** — `mac-tsan` build. Catches **races** (log scan, fast-exit).
- **leaks** — Apple's `leaks --atExit` on a `mac-release` build. This is how
  **leaks** are found on macOS: **LeakSanitizer (ASan `detect_leaks`) is not
  supported on Apple Silicon**, so the asan pass runs with `detect_leaks=0`.
  The leak pass relies on `runSoakTest`'s orderly window-close exit so `leaks`
  scans a fully-released heap; it fails only if leaked bytes exceed
  `SOAK_LEAKS_MAX_BYTES` (default 64 KiB, to tolerate Qt/macOS one-time globals).

`runSoakTest` exits by an **orderly window close** (`closeEvent` stops the
reader/cache/decoder threads, then the event loop unwinds and the stack `MW`
destructs) so the leak scan sees only true leaks. `WINNOW_SOAK_FAST_EXIT=1`
switches to `std::_Exit` (used by the tsan pass and the quick smoke).

**Thread gate (all passes).** Every bounce logs the live OS thread count
(`threads=`); the `SOAK: done` line reports `threadsGrowth = threadsMax -
threadsBaseline`. `run_soak.sh` fails any pass whose growth exceeds
`SOAK_THREADS_MAX_GROWTH` (default 300). This is what catches the video-thumbnail
decoder-thread leak (`QMediaPlayer`/AVFoundation) that exhausts `pthread_create`
under sustained bouncing — independent of the sanitizer in use.

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
- **Soak** uses the hidden `--soaktest <folders...>` flag → `MW::runSoakTest`,
  deferred into the event loop (the bounce loop blocks on its own `processEvents`).
  Pace/duration/seed come from `WINNOW_SOAK_MS` / `WINNOW_SOAK_IMG_MS` /
  `WINNOW_SOAK_SEED`; `run_soak.sh` builds a temp tree of `SOAK_FOLDERS` fixture
  copies to bounce between. A short opt-in CTest (`-DWINNOW_BUILD_SOAK_SMOKE=ON`,
  `ctest -L soak`) just checks the harness still runs.
- All four flags enable `QStandardPaths` test mode (so they **never touch your real
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
