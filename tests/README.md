# Winnow test suite

Run after significant changes and before a release. Three layers, driven by
CMake/CTest + one script.

| Layer | What it checks | Runner |
|-------|----------------|--------|
| **unit** | Pure logic — byte parsers, path helpers, IFD rational decode, EXIF tag table, IFD directory walk (Qt Test) | `ctest -L unit` |
| **smoke** | Real app launches, opens a folder, loads images, no crash | `ctest -L smoke` |
| **metadata** | Full Metadata pipeline parses a real committed camera file (make/model/dims) | `ctest -L metadata` |
| **tsan**  | No data races during folder-load concurrency | `tests/tsan/run_tsan.sh` |
| **tsan (develop)** | No races between the GUI thread and the Develop render worker | `tests/tsan/run_tsan_develop.sh` |
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

### Choosing the preset (`PRESET` / `BUILD`)

All three `tests/tsan/*.sh` scripts default to `--preset mac-tsan` and
`build/mac-tsan`, and both are overridable:

```sh
PRESET=mac-tsan-local tests/tsan/run_tsan_develop.sh
```

You need this whenever the stock `mac-tsan` preset is not usable as-is. Outside
Qt Creator it usually is not: the Ninja generator finds no build program (ninja
ships inside Qt Creator's toolchain, not on `PATH`), and `_base` takes
`CMAKE_PREFIX_PATH` from `$env{QT_DIR}`, which is normally unset. The first
failure is loud (`CMAKE_MAKE_PROGRAM-NOTFOUND`); the second is **silent** — the
build succeeds against whatever Qt is on the default search path. Define the
machine-local preset in `CMakeUserPresets.json` (gitignored) pinning both, and
point `PRESET` at it.

`BUILD` is separate from `PRESET` rather than derived from it, because a local
preset normally pins `binaryDir` back to `build/mac-tsan` so the documented paths
keep working. Set `BUILD` only if your preset really does build elsewhere.

**Gotcha:** changing `CMAKE_PREFIX_PATH` on an EXISTING build dir does not
re-resolve `Qt6_DIR` — CMake keeps the Qt it found on the first configure. Delete
the build dir and configure again, or you will keep building against the old Qt
while the cache claims otherwise.

### Reading a Develop TSan result

`PASS` on its own means little — the driver can pass while barely rendering. Check the
coverage the run actually achieved:

```sh
grep -c '\[DevTime\] proxy' /tmp/winnow_tsan_develop.log      # renders, not ticks
grep -oE 'resume -?[0-9]+ / [0-9]+' /tmp/winnow_tsan_develop.log | sort | uniq -c
grep -c 'prefix +layer' /tmp/winnow_tsan_develop.log           # cached-layer reuse
```

A useful run reaches all three states: `noPrefix` (cold), `prefix` (cached prefix
reused) and `prefix +layer` (cached layer reused). A reference run is 1206 ticks / 77
renders / 56 cold / 10 prefix / 11 prefix+layer. Runs that produced 2–7 renders, or
never left `noPrefix`, proved nothing — and looked identical at the PASS line.

Scope count matters too: `resume -1 / 21` means the stack grew to 21 scopes and every
render composited all of them, which collapses the render rate.

### Develop render (`run_tsan_develop.sh`)

`run_tsan.sh` and the soak layer only exercise folder-load concurrency — neither
enters Develop, builds a mask or triggers a proxy render. Since the interactive
proxy render moved to a worker (`MW::developProxyPool`) while the GUI thread
keeps using the same caches, that needed its own layer:

```sh
tests/tsan/run_tsan_develop.sh          # ~45 s drag under TSan
WINNOW_DEVTEST_MS=180000 tests/tsan/run_tsan_develop.sh    # longer hunt
```

Driven by the hidden `--devtest <folder>` flag → `MW::runDevelopStressTest`:
a simulated brush drag on a multi-submask scope, while switching images,
toggling the veil, re-rendering through the crop-style caller and re-selecting
the folder. It always works on a **throwaway copy** of the images (Develop
flushes sidecars on navigate-away, so it must never run against a real folder).

Two things about reading its output:

- **`WINNOW_DEVTEST_SERIAL=1` is the default and matters.** It caps the global
  pool at one thread so every `parallelFor` runs serially. Homebrew Qt is not
  TSan-instrumented, so `QFuture::waitForFinished()`'s happens-before edge is
  invisible: a parallel-for's lambdas capture the caller's stack by reference,
  and once the caller returns and reuses that stack, every one of those reads is
  reported as a race. Measured here: **49 reports parallel, 5 serial**, and the
  44 that vanish are all that artifact — confirmed by A/B, since the same
  reports appear with the render forced back onto the GUI thread where the
  concurrency cannot exist. Serial mode leaves one axis, GUI vs worker, which is
  the point. `WINNOW_DEVTEST_SERIAL=0` shows the noisy picture.
- **Failure is scoped.** Only races naming the Develop render fail the run; the
  folder-load / metadata-reader family is counted and printed as a warning (it
  is a separate, known concern). Crashes always fail — the first run of this
  test SEGV'd and would otherwise have "passed".

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
