#!/usr/bin/env bash
#
# Soak layer of the Winnow test suite.
#
# Drives the folder load/navigate pipeline for a long time via the headless
# --soaktest flag (main.cpp → MW::runSoakTest): bounce between folders, reload
# each, ping-pong through its images. Hunts the two failure classes that only
# show up under sustained churn — data races and memory growth.
#
# Three passes (any subset, see SOAK_PASSES):
#
#   asan   AddressSanitizer build (mac-asan). Catches use-after-free,
#          heap-buffer-overflow and double-free LIVE as they happen. This is the
#          reliable memory-safety oracle on macOS.
#
#   tsan   ThreadSanitizer build (mac-tsan). Catches data races. Uses
#          WINNOW_SOAK_FAST_EXIT=1 (std::_Exit) so teardown thread-joins don't
#          add noise; races print live and are found by log scan.
#
#   leaks  Apple's `leaks --atExit` against a NORMAL (mac-release) build.
#          LeakSanitizer (ASan detect_leaks) is NOT supported on Apple Silicon,
#          so this is how actual leaks are detected here. Relies on runSoakTest's
#          orderly window-close exit so `leaks` can scan a fully-released heap —
#          whatever it still reports is a true leak. Output is noisy (Qt/macOS
#          one-time globals); the harness fails only if the leaked total exceeds
#          SOAK_LEAKS_MAX_BYTES.
#
# Usage:   tests/soak/run_soak.sh
# Env:
#   CMAKE=<cmake>            cmake binary
#   SOAK_PASSES="asan tsan"  which passes to run (default: "asan tsan")
#   WINNOW_SOAK_MS=<ms>      soak duration per pass (default 60000 = 60 s)
#   WINNOW_SOAK_IMG_MS=<ms>  delay between images (default 50)
#   WINNOW_SOAK_SEED=<n>     RNG seed for reproducibility (default 1)
#   SOAK_FOLDERS=<n>         number of fixture subfolders to bounce (default 3)
#   SOAK_LEAKS_MAX_BYTES=<n> leak-pass failure threshold (default 65536)
#
# A real multi-hour hunt:  WINNOW_SOAK_MS=3600000 SOAK_PASSES="tsan" tests/soak/run_soak.sh
#
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 1   # `cmake --preset` must run from the dir with CMakePresets.json
CMAKE="${CMAKE:-/Users/roryhill/Qt/Tools/CMake/CMake.app/Contents/bin/cmake}"

SOAK_PASSES="${SOAK_PASSES:-asan tsan}"
DURATION_MS="${WINNOW_SOAK_MS:-60000}"
IMG_MS="${WINNOW_SOAK_IMG_MS:-50}"
SEED="${WINNOW_SOAK_SEED:-1}"
NFOLDERS="${SOAK_FOLDERS:-3}"
LEAKS_MAX="${SOAK_LEAKS_MAX_BYTES:-65536}"
FIXTURES="$ROOT/tests/fixtures/images"

# Build a temp tree of N folders (each a copy of the committed fixtures) so the
# bounce has distinct folders to move between. Removed on exit.
WORK="$(mktemp -d /tmp/winnow_soak.XXXXXX)"
trap 'rm -rf "$WORK"' EXIT
SOAK_DIRS=()
for i in $(seq 1 "$NFOLDERS"); do
    d="$WORK/folder$i"
    mkdir -p "$d"
    cp "$FIXTURES"/*.jpg "$FIXTURES"/*.tif "$FIXTURES"/*.png "$d"/ 2>/dev/null
    SOAK_DIRS+=("$d")
done
echo "==> Soak: passes='$SOAK_PASSES' duration=${DURATION_MS}ms img=${IMG_MS}ms seed=$SEED folders=$NFOLDERS"

overall=0

run_build() {
    # $1 = preset
    "$CMAKE" --preset "$1"            > /dev/null || return $?
    "$CMAKE" --build "$ROOT/build/$1" --target Winnow || return $?
}

soak_env() {
    # common env for a soak run; caller prefixes sanitizer-specific vars
    echo "WINNOW_SOAK_MS=$DURATION_MS WINNOW_SOAK_IMG_MS=$IMG_MS WINNOW_SOAK_SEED=$SEED"
}

for pass in $SOAK_PASSES; do
    case "$pass" in

    asan)
        echo "==> [asan] building (mac-asan)…"
        if ! run_build mac-asan; then echo "[asan] BUILD FAILED"; overall=1; continue; fi
        APP="$ROOT/build/mac-asan/Winnow.app/Contents/MacOS/Winnow"
        LOG="/tmp/winnow_soak_asan.log"
        echo "==> [asan] running soak (log: $LOG)…"
        # detect_leaks=0: LSan is unsupported on Apple Silicon; the leaks pass
        # covers leaks. ASan here is the use-after-free / overflow oracle.
        ASAN_OPTIONS="detect_leaks=0 abort_on_error=0 halt_on_error=0 detect_stack_use_after_return=1" \
        env $(soak_env) \
            "$APP" --soaktest "${SOAK_DIRS[@]}" > "$LOG" 2>&1
        echo "    app exit=$?"
        if grep -qE "ERROR: AddressSanitizer|runtime error:" "$LOG"; then
            echo "[asan] FAIL: AddressSanitizer reported an error — see $LOG"
            grep -nE "ERROR: AddressSanitizer|SUMMARY: AddressSanitizer" "$LOG" | head
            overall=1
        elif ! grep -q "SOAK: done" "$LOG"; then
            echo "[asan] FAIL: soak did not complete — see $LOG"; tail -20 "$LOG"; overall=1
        else
            echo "[asan] PASS: no AddressSanitizer errors over $(grep -c '^SOAK: bounce' "$LOG") bounces."
        fi
        ;;

    tsan)
        echo "==> [tsan] building (mac-tsan)…"
        if ! run_build mac-tsan; then echo "[tsan] BUILD FAILED"; overall=1; continue; fi
        APP="$ROOT/build/mac-tsan/Winnow.app/Contents/MacOS/Winnow"
        LOG="/tmp/winnow_soak_tsan.log"
        echo "==> [tsan] running soak (log: $LOG)…"
        TSAN_OPTIONS="suppressions=$ROOT/tsan.supp halt_on_error=0 history_size=4 report_thread_leaks=0" \
        WINNOW_SOAK_FAST_EXIT=1 \
        env $(soak_env) \
            "$APP" --soaktest "${SOAK_DIRS[@]}" > "$LOG" 2>&1
        echo "    app exit=$? (ignored; race detection is by log scan)"
        if grep -qE "WARNING: ThreadSanitizer|data race" "$LOG"; then
            echo "[tsan] FAIL: ThreadSanitizer reported races — see $LOG"
            grep -nE "WARNING: ThreadSanitizer|data race" "$LOG" | head
            overall=1
        elif ! grep -q "SOAK: done" "$LOG"; then
            echo "[tsan] FAIL: soak did not complete — see $LOG"; tail -20 "$LOG"; overall=1
        else
            echo "[tsan] PASS: no races over $(grep -c '^SOAK: bounce' "$LOG") bounces."
        fi
        ;;

    leaks)
        echo "==> [leaks] building (mac-release)…"
        if ! run_build mac-release; then echo "[leaks] BUILD FAILED"; overall=1; continue; fi
        APP="$ROOT/build/mac-release/Winnow.app/Contents/MacOS/Winnow"
        LOG="/tmp/winnow_soak_leaks.log"
        echo "==> [leaks] running soak under 'leaks --atExit' (log: $LOG)…"
        # MallocStackLogging gives `leaks` allocation stacks. Orderly exit
        # (default, NOT fast-exit) is required so the heap is fully released
        # before the atExit scan.
        MallocStackLogging=1 \
        env $(soak_env) \
            leaks --atExit -- "$APP" --soaktest "${SOAK_DIRS[@]}" > "$LOG" 2>&1
        echo "    leaks exit=$?"
        # `leaks` prints e.g. "Process N: 12 leaks for 4096 total leaked bytes."
        bytes=$(grep -oE "[0-9]+ total leaked bytes" "$LOG" | tail -1 | grep -oE "^[0-9]+")
        bytes="${bytes:-0}"
        if ! grep -q "SOAK: done" "$LOG"; then
            echo "[leaks] FAIL: soak did not complete — see $LOG"; tail -20 "$LOG"; overall=1
        elif [ "$bytes" -gt "$LEAKS_MAX" ]; then
            echo "[leaks] FAIL: $bytes leaked bytes > threshold $LEAKS_MAX — see $LOG"
            grep -nE "total leaked bytes|leaks Report" "$LOG" | head
            overall=1
        else
            echo "[leaks] PASS: $bytes leaked bytes (<= $LEAKS_MAX threshold)."
        fi
        ;;

    *)
        echo "Unknown pass '$pass' (expected: asan tsan leaks)"; overall=1 ;;
    esac
done

[ "$overall" -eq 0 ] && echo "SOAK: all passes PASS" || echo "SOAK: FAILURES — see logs above"
exit "$overall"
