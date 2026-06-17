#!/usr/bin/env bash
#
# ThreadSanitizer reproduction for the QSortFilterProxyModel (dm->sf) data race.
#
# The plain run_tsan.sh loads a single, small, non-recursive folder with no
# navigation — it does not exercise the collision the user hit: a recursive
# multi-subfolder load (structural proxy inserts on the GUI thread) running
# concurrently with the image-cache / decoder / metaread threads reading dm->sf
# while the user navigates. This script reproduces exactly that.
#
# It drives MW::runSelfTest with two env knobs:
#   WINNOW_SELFTEST_RECURSE=1   recursive multi-subfolder load
#   WINNOW_SELFTEST_NAV_MS=<ms> advance the current image every <ms> during load
#
# Race detection is by LOG SCAN (--selftest ends with std::_Exit, bypassing
# TSan's atexit summary; per-race WARNINGs still print live). We confirm THE race
# specifically by grepping for proxy frames, not just any "data race".
#
# Usage:   tests/tsan/run_tsan_proxy.sh
# Env:     CMAKE=<cmake>  WINNOW_SELFTEST_MS=<ms>  WINNOW_TSAN_FOLDER=<dir>
#          NAV_MS=<ms>  COPIES=<n>
#
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || exit 1   # `cmake --preset` must run from the dir with CMakePresets.json
CMAKE="${CMAKE:-/Users/roryhill/Qt/Tools/CMake/CMake.app/Contents/bin/cmake}"
BUILD="$ROOT/build/mac-tsan"
APP="$BUILD/Winnow.app/Contents/MacOS/Winnow"
FIXTURES="$ROOT/tests/fixtures/images"
LOG_A="${LOG_A:-/tmp/winnow_tsan_proxy_A.log}"
LOG_B="${LOG_B:-/tmp/winnow_tsan_proxy_B.log}"
LOG_C="${LOG_C:-/tmp/winnow_tsan_proxy_C.log}"
SETTLE_MS="${WINNOW_SELFTEST_MS:-45000}"   # long enough to sweep a large tree to the end
NAV_MS="${NAV_MS:-8}"
COPIES="${COPIES:-300}"   # image copies per subfolder (ignored when WINNOW_TSAN_FOLDER set)
STRESS="${STRESS:-1}"     # 1 = also pick + ingest + reverse-sort during load

# Frames that positively identify the proxy-mapping race (vs. unrelated races).
PROXY_RE='QSortFilterProxyModel|SortFilter|mapFromSource|mapToSource|filterAcceptsRow|create_mapping|source_to_proxy|proxy_to_source'

echo "==> Configuring + building the ThreadSanitizer app (mac-tsan)…"
"$CMAKE" --preset mac-tsan || exit $?
"$CMAKE" --build "$BUILD" --target Winnow || exit $?

# --- Test folder: a real recursive tree if supplied, else a generated one ----
if [[ -n "${WINNOW_TSAN_FOLDER:-}" ]]; then
    TREE="$WINNOW_TSAN_FOLDER"
    echo "==> Using supplied recursive tree: $TREE"
    CLEANUP_TREE=""
else
    TREE="$(mktemp -d "${TMPDIR:-/tmp}/winnow_tsan_tree.XXXXXX")"
    CLEANUP_TREE="$TREE"
    echo "==> Generating recursive fixture tree under $TREE ($COPIES imgs x 2 subfolders)…"
    # Mimic the user's case: dcim/ with two child folders, each with many images.
    # Use the larger jpg so decode work overlaps the second-folder insert.
    SRC="$FIXTURES/sample_nikon_d700.jpg"
    for sub in 100SUB 101SUB; do
        d="$TREE/dcim/$sub"
        mkdir -p "$d"
        for i in $(seq -w 1 "$COPIES"); do
            cp "$SRC" "$d/${sub}_${i}.jpg"
        done
    done
    TREE="$TREE/dcim"
fi

run_pass() {
    # $1 = log path, $2 = suppressions file, $3 = extra TSAN_OPTIONS
    local log="$1" supp="$2" extra="$3"
    : > "$log"
    TSAN_OPTIONS="suppressions=$supp halt_on_error=0 report_thread_leaks=0 $extra" \
    WINNOW_SELFTEST_MS="$SETTLE_MS" \
    WINNOW_SELFTEST_RECURSE=1 \
    WINNOW_SELFTEST_NAV_MS="$NAV_MS" \
    WINNOW_SELFTEST_STRESS="$STRESS" \
        "$APP" --selftest "$TREE" > "$log" 2>&1
    echo "    app exit=$? (ignored; race detection is by log scan)"
    grep -m1 "reached end" "$log" | sed 's/^/    /' || true
}

# Empty suppressions file for the unfiltered pass.
EMPTY_SUPP="$(mktemp "${TMPDIR:-/tmp}/winnow_tsan_empty.XXXXXX")"
: > "$EMPTY_SUPP"

echo "==> Pass A (primary: existing suppressions, log: $LOG_A)…"
run_pass "$LOG_A" "$ROOT/tsan.supp" "history_size=4"

echo "==> Pass B (diagnostic: deeper history + print_suppressions, log: $LOG_B)…"
run_pass "$LOG_B" "$ROOT/tsan.supp" "history_size=7 print_suppressions=1"

echo "==> Pass C (no suppressions: surfaces races tsan.supp would hide, log: $LOG_C)…"
run_pass "$LOG_C" "$EMPTY_SUPP" "history_size=7"

rm -f "$EMPTY_SUPP"

# Cleanup generated tree (keep a supplied one).
[[ -n "$CLEANUP_TREE" ]] && rm -rf "$CLEANUP_TREE"

# --- Verdict -----------------------------------------------------------------
echo
status=0

scan() {
    local log="$1" label="$2"
    if grep -qE "WARNING: ThreadSanitizer|data race" "$log"; then
        echo "[$label] TSan reported race(s):"
        grep -nE "WARNING: ThreadSanitizer|data race" "$log" | head
        if grep -qE "$PROXY_RE" "$log"; then
            echo "[$label] CONFIRMED: race stack(s) reference the sort/filter proxy:"
            grep -nE "$PROXY_RE" "$log" | head
            return 1
        else
            echo "[$label] races present but none reference proxy frames (check $log)."
            return 2
        fi
    fi
    return 0
}

scan "$LOG_A" "A"; rcA=$?
scan "$LOG_B" "B (suppressed shown)"; rcB=$?
scan "$LOG_C" "C (no suppressions)"; rcC=$?

# Every distinct race that fired across the three passes (the icon-range race and
# any others surface here even when they are not proxy-related).
echo
echo "All distinct races seen this run:"
grep -hE "SUMMARY: ThreadSanitizer: data race" "$LOG_A" "$LOG_B" "$LOG_C" 2>/dev/null \
    | sed 's/^SUMMARY: ThreadSanitizer: data race /  - /' | sort -u
echo

# Masking check: a race that appears only with suppressions off (Pass C) was being
# hidden by tsan.supp. Flag it so the QHashData/QListData/QArrayData/QString lines
# can be narrowed.
if grep -qE "$PROXY_RE" "$LOG_C" && ! grep -qE "$PROXY_RE" "$LOG_A"; then
    echo "NOTE: a proxy-frame race appears only with suppressions OFF (Pass C) — tsan.supp"
    echo "      is masking it. Narrow the QHashData/QListData/QArrayData/QString lines."
fi

if [[ $rcA -eq 1 || $rcB -eq 1 || $rcC -eq 1 ]]; then
    echo "RESULT: QSortFilterProxyModel data race CONFIRMED — see $LOG_A / $LOG_B / $LOG_C."
    exit 1   # non-zero == "proxy race reproduced" for this diagnostic script
fi

if [[ $rcA -eq 2 || $rcB -eq 2 || $rcC -eq 2 ]]; then
    echo "RESULT: data race(s) reproduced, but none on proxy frames this run (see the list"
    echo "        above). Rerun on the real ORF tree (slow load overlaps navigation) to give"
    echo "        the proxy-mapping variant a wider window."
    exit 2
fi

if ! grep -q "SELFTEST:" "$LOG_A"; then
    echo "INCONCLUSIVE: self-test did not reach the loaded state — see $LOG_A"
    tail -20 "$LOG_A"
    exit 3
fi

echo "RESULT: no race observed this run (it is intermittent — rerun, raise COPIES,"
echo "        lower NAV_MS, or point WINNOW_TSAN_FOLDER at a large real recursive tree)."
exit 0
