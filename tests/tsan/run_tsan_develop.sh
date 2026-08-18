#!/usr/bin/env bash
#
# ThreadSanitizer layer for the DEVELOP interactive render.
#
# run_tsan.sh and run_soak.sh exercise the folder-load concurrency only: neither
# enters Develop, builds a mask, or triggers a proxy render. That left the whole
# interactive render path untested for races -- and it is precisely the part that
# now runs on a worker (MW::developProxyPool) while the GUI thread keeps touching
# the same caches:
#
#   DevelopStackCache   mutex-guarded, written by the worker, cleared by the GUI
#                       on image / folder / proxy / denoise-base changes
#   g_brushCache        byte-budgeted LRU, both threads
#   g_maskFoldCache     fold prefixes, both threads
#   g_spotHealMutex     LaMa / MI-GAN, now reachable from two render pools
#
# The driver is MW::runDevelopStressTest (--devtest). It simulates a brush drag on
# a multi-submask scope while switching images, toggling the veil, re-rendering
# through the crop-style caller, and re-selecting the folder.
#
# Race detection is by LOG SCAN, not exit code: --devtest ends with std::_Exit,
# which bypasses TSan's atexit summary. Per-race WARNINGs still print live.
#
# WINNOW_DEVTEST_SERIAL=1 (THE DEFAULT) caps the global thread pool at one thread,
# so every developParallelRows / maskParallelFor / Develop::parallelFor takes its
# serial path. Do not remove it casually -- it is what makes this test readable.
# Homebrew Qt is not TSan-instrumented, so QFuture::waitForFinished()'s
# happens-before edge is INVISIBLE to the tool: a parallel-for's lambdas capture
# the caller's stack by reference, and once the caller returns and reuses that
# stack every one of those reads is reported as a race. Measured on this harness:
# 49 reports with the pool parallel, 5 with it serial -- and the 44 that vanish
# are all that artifact. It is not the worker's doing either; the same reports
# appear with the proxy render forced back onto the GUI thread, where that
# concurrency cannot exist. Serial mode leaves exactly ONE axis of concurrency,
# the GUI thread against the proxy worker, which is what this test is for.
# Set WINNOW_DEVTEST_SERIAL=0 to see the noisy full-parallel picture.
#
# Usage:   tests/tsan/run_tsan_develop.sh
# Env:     CMAKE=<cmake>            WINNOW_DEVTEST_MS=<ms>      LOG=<path>
#          WINNOW_DEVTEST_SUBMASKS=<n>   WINNOW_DEVTEST_TICK_MS=<ms>
#          WINNOW_DEVTEST_FOLDER=<dir>   COPIES=<n>   NOBUILD=1
#          WINNOW_DEVTEST_SERIAL=0       (opt OUT of serial parallel-for)
#          PRESET=<configure preset>     BUILD=<binary dir>
#
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 1   # `cmake --preset` must run from the dir with CMakePresets.json
CMAKE="${CMAKE:-/Users/roryhill/Qt/Tools/CMake/CMake.app/Contents/bin/cmake}"
# PRESET / BUILD: the configure preset and its binary dir. Overridable because
# CMakePresets.json's mac-tsan is not always usable as-is -- outside Qt Creator the
# Ninja generator finds no build program (ninja ships in Qt Creator's toolchain, not
# on PATH) and _base takes CMAKE_PREFIX_PATH from $env{QT_DIR}, which is unset, so a
# plain `--preset mac-tsan` either fails to configure or silently builds against the
# wrong Qt. A machine-local CMakeUserPresets.json preset fixes both; point this at it:
#     PRESET=mac-tsan-local tests/tsan/run_tsan_develop.sh
# BUILD is separate rather than derived from PRESET, because a local preset normally
# pins binaryDir back to build/mac-tsan so the documented paths keep working. Set it
# only if your preset really does build somewhere else.
PRESET="${PRESET:-mac-tsan}"
BUILD="${BUILD:-$ROOT/build/mac-tsan}"
APP="$BUILD/Winnow.app/Contents/MacOS/Winnow"
LOG="${LOG:-/tmp/winnow_tsan_develop.log}"
COPIES="${COPIES:-6}"

if [[ "${NOBUILD:-0}" != "1" ]]; then
    echo "==> Configuring + building the ThreadSanitizer app (mac-tsan)…"
    "$CMAKE" --preset "$PRESET" || exit $?
    "$CMAKE" --build "$BUILD" --target Winnow || exit $?
fi

# --- Test folder ------------------------------------------------------------
# ALWAYS a throwaway copy, never the source folder. The driver makes thousands of
# develop edits, and Develop flushes an image's stack to its .xmp sidecar on
# navigate-away regardless of the debounce setting -- so running this directly
# against a real photo folder would write sidecars all over it.
#
# The committed fixtures are also deliberately tiny (64x48), which makes the
# render finish before the GUI thread can collide with it. Default to copies of
# the one real camera JPEG so each tick does enough work for the worker and the
# GUI thread to actually overlap; WINNOW_DEVTEST_FOLDER sources images from
# somewhere else instead (still copied).
TREE="$(mktemp -d "${TMPDIR:-/tmp}/winnow_devtest.XXXXXX")"
cleanup() { rm -rf "$TREE"; }
trap cleanup EXIT

if [[ -n "${WINNOW_DEVTEST_FOLDER:-}" ]]; then
    n=0
    while IFS= read -r f; do
        cp "$f" "$TREE/" && n=$((n + 1))
        [[ "$n" -ge "$COPIES" ]] && break
    done < <(find "$WINNOW_DEVTEST_FOLDER" -maxdepth 1 -type f \
                  \( -iname '*.jpg' -o -iname '*.jpeg' -o -iname '*.tif' -o -iname '*.png' \) \
                  | sort)
    if [[ "$n" -eq 0 ]]; then
        echo "FAIL: no images found in $WINNOW_DEVTEST_FOLDER"
        exit 1
    fi
    echo "==> Copied $n image(s) from $WINNOW_DEVTEST_FOLDER to $TREE"
else
    SRC="$ROOT/tests/fixtures/images/sample_nikon_d700.jpg"
    if [[ ! -f "$SRC" ]]; then
        echo "FAIL: fixture $SRC not found"
        exit 1
    fi
    for ((i = 1; i <= COPIES; i++)); do
        cp "$SRC" "$TREE/dev_$(printf '%02d' "$i").jpg"
    done
    echo "==> Built a $COPIES-image tree at $TREE"
fi

echo "==> Running --devtest under ThreadSanitizer (log: $LOG)…"
# report_thread_leaks=0: --devtest ends with std::_Exit, so worker threads are
# never joined -- an expected "leak", not a concurrency bug.
TSAN_OPTIONS="suppressions=$ROOT/tsan.supp halt_on_error=0 history_size=7 report_thread_leaks=0" \
WINNOW_DEVTEST_SERIAL="${WINNOW_DEVTEST_SERIAL:-1}" \
WINNOW_DEVTEST_MS="${WINNOW_DEVTEST_MS:-45000}" \
WINNOW_DEVTEST_SUBMASKS="${WINNOW_DEVTEST_SUBMASKS:-8}" \
WINNOW_DEVTEST_TICK_MS="${WINNOW_DEVTEST_TICK_MS:-10}" \
WINNOW_DEVTEST_LOAD_MS="${WINNOW_DEVTEST_LOAD_MS:-6000}" \
    "$APP" --devtest "$TREE" > "$LOG" 2>&1
echo "    app exit=$? (ignored; race detection is by log scan)"

# A crash is not a race report and would otherwise slip through the scan below --
# the first run of this test SEGV'd in buildMaskBuffer and still "passed".
if grep -qE "ThreadSanitizer: SEGV|DEADLYSIGNAL|ThreadSanitizer: (heap-use-after-free|SIGSEGV)" "$LOG"; then
    echo "FAIL: the app crashed under ThreadSanitizer — see $LOG"
    grep -nE "ThreadSanitizer: SEGV|SUMMARY: ThreadSanitizer: SEGV" "$LOG" | head -5
    exit 1
fi

# --- Triage ------------------------------------------------------------------
# Scoped, like run_tsan_proxy.sh: a race is THIS test's failure only if its stack
# names the Develop render. The folder-load / metadata-reader family
# (DataModel, MetaRead, Reader, ImageMetadata queued-signal copies) is a known,
# separate concern -- see notes/Documentation.txt "Thread-safety" -- and the
# driver's folder churn surfaces it. Reporting those as failures here would keep
# this test permanently red for something it is not testing, so they are counted
# and printed as a warning instead.
DEV_RE='Develop|buildMaskBuffer|BrushStamp|WorkingImageCache|renderDevelopPreview|updateMaskOverlayTint|brushCache|maskFold|applySpots|ScaledPixmapItem'

DEV_RACES="$(awk -v re="$DEV_RE" 'BEGIN{RS="=================="}
    /WARNING: ThreadSanitizer/ && $0 ~ re {n++} END{print n+0}' "$LOG")"
OTHER_RACES="$(awk -v re="$DEV_RE" 'BEGIN{RS="=================="}
    /WARNING: ThreadSanitizer/ && $0 !~ re {n++} END{print n+0}' "$LOG")"

if [[ "$DEV_RACES" -gt 0 ]]; then
    echo "FAIL: $DEV_RACES data race(s) in the Develop render path — see $LOG"
    awk -v re="$DEV_RE" 'BEGIN{RS="=================="}
        /WARNING: ThreadSanitizer/ && $0 ~ re {print}' "$LOG" | grep "^SUMMARY" | head -10
    exit 1
fi
if [[ "$OTHER_RACES" -gt 0 ]]; then
    echo "WARN: $OTHER_RACES race(s) OUTSIDE the Develop render path (folder load /"
    echo "      metadata reader — a separate, known concern; not this test's subject):"
    awk -v re="$DEV_RE" 'BEGIN{RS="=================="}
        /WARNING: ThreadSanitizer/ && $0 !~ re {print}' "$LOG" | grep "^SUMMARY" \
        | sed 's/(Winnow.*//' | sort | uniq -c | head -10
fi

# A run that never reached the drag proves nothing, and would otherwise "pass".
if ! grep -q "DEVTEST: done" "$LOG"; then
    echo "FAIL: the develop stress driver did not complete — see $LOG"
    tail -20 "$LOG"
    exit 1
fi
TICKS="$(sed -n 's/.*DEVTEST: done ticks=\([0-9]*\).*/\1/p' "$LOG" | tail -1)"
if [[ -z "$TICKS" || "$TICKS" -lt 50 ]]; then
    echo "FAIL: only ${TICKS:-0} drag ticks ran — the driver is not exercising the render"
    exit 1
fi

echo "PASS: no Develop-path data races over $TICKS drag ticks (${OTHER_RACES} unrelated)"
exit 0
