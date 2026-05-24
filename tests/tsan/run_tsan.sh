#!/usr/bin/env bash
#
# ThreadSanitizer layer of the Winnow test suite.
#
# Builds the app with TSan (mac-tsan preset) and exercises the folder-load
# concurrency — metadata reader threads + image cache threads — via the
# --selftest flag. Fails if TSan reports a data race.
#
# Race detection is by LOG SCAN, not exit code: --selftest ends with std::_Exit,
# which bypasses TSan's atexit summary. Per-race WARNINGs still print live, and
# that is what we grep for.
#
# Usage:   tests/tsan/run_tsan.sh
# Env:     CMAKE=<cmake>  WINNOW_SELFTEST_MS=<ms>  LOG=<path>
#
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 1   # `cmake --preset` must run from the dir with CMakePresets.json
CMAKE="${CMAKE:-/Users/roryhill/Qt/Tools/CMake/CMake.app/Contents/bin/cmake}"
BUILD="$ROOT/build/mac-tsan"
APP="$BUILD/Winnow.app/Contents/MacOS/Winnow"
FIXTURES="$ROOT/tests/fixtures/images"
LOG="${LOG:-/tmp/winnow_tsan.log}"

echo "==> Configuring + building the ThreadSanitizer app (full instrumented build)…"
"$CMAKE" --preset mac-tsan || exit $?
"$CMAKE" --build "$BUILD" --target Winnow || exit $?

echo "==> Running --selftest under ThreadSanitizer (log: $LOG)…"
# report_thread_leaks=0: the self-test ends with std::_Exit (no orderly shutdown),
# so worker threads are never joined — an expected "leak", not a concurrency bug.
TSAN_OPTIONS="suppressions=$ROOT/tsan.supp halt_on_error=0 history_size=4 report_thread_leaks=0" \
WINNOW_SELFTEST_MS="${WINNOW_SELFTEST_MS:-8000}" \
    "$APP" --selftest "$FIXTURES" > "$LOG" 2>&1
echo "    app exit=$? (ignored; race detection is by log scan)"

if grep -qE "WARNING: ThreadSanitizer|data race" "$LOG"; then
    echo "FAIL: ThreadSanitizer reported races — see $LOG"
    grep -nE "WARNING: ThreadSanitizer|data race" "$LOG" | head
    exit 1
fi

if ! grep -q "SELFTEST:" "$LOG"; then
    echo "FAIL: self-test did not reach the loaded state — see $LOG"
    tail -20 "$LOG"
    exit 1
fi

echo "PASS: folder load completed with no ThreadSanitizer races."
