#!/bin/zsh
# --------------------------------------------------------------------
# Verify a Winnow .app is "vanilla-Mac ready" — i.e. it will run on a
# clean Apple Silicon Mac with no Homebrew / Qt / Xcode installed.
#
# Usage:
#   verify_bundle.command <path-to-.app> [--structural-only]
#
#   --structural-only   run only checks 1-5 (skip Gatekeeper/notarization),
#                       used during staging before the app is notarized.
#
# Exit status: 0 if every RUN check passes, non-zero otherwise.
#
# What this CAN confirm (statically, on the bundle):
#   1. no load dependency points outside the bundle (/opt, /usr/local, /Users)
#   2. no LC_RPATH points outside the bundle
#   3. every internal (@rpath/@executable_path/@loader_path) dep resolves
#   4. architecture is arm64
#   5. minimum-OS (minos) is not newer than the supported floor
#   6. code signature is valid (deep, strict)
#   7. Gatekeeper accepts it (notarized Developer ID)
#   8. the notarization ticket is stapled (launches offline)
#
# What it CANNOT confirm: actual runtime behaviour on a clean OS, resources
# read from absolute dev paths at runtime, or plugins dlopen'd by name. The
# only definitive test is launching on a fresh macOS VM / clean user account.
# --------------------------------------------------------------------

APP="$1"
MODE="$2"

if [[ -z "$APP" || ! -d "$APP" ]]; then
    echo "Usage: $(basename "$0") <path-to-.app> [--structural-only]"
    exit 2
fi
APP="${APP:A}"   # absolute, resolved

# Supported minimum-OS floor (keep in sync with CMAKE_OSX_DEPLOYMENT_TARGET).
MIN_OS_FLOOR_MAJOR=12

EXE_NAME=$(/usr/libexec/PlistBuddy -c "Print CFBundleExecutable" "$APP/Contents/Info.plist" 2>/dev/null)
EXE_PATH="$APP/Contents/MacOS/$EXE_NAME"
MACOS_DIR="$APP/Contents/MacOS"

ok=true
fail() { echo "  ❌ $1"; ok=false; }
pass() { echo "  ✓ $1"; }

# Emit the names of LC_LOAD_DYLIB / LC_LOAD_WEAK_DYLIB (real deps; excludes LC_ID_DYLIB).
load_deps() {
    otool -l "$1" 2>/dev/null | awk '
        /cmd LC_LOAD_DYLIB|cmd LC_LOAD_WEAK_DYLIB/ {g=1; next}
        g && /name / {print $2; g=0}'
}
# Emit LC_RPATH search paths.
rpaths() {
    otool -l "$1" 2>/dev/null | awk '
        /cmd LC_RPATH/ {g=1; next}
        g && /path / {print $2; g=0}'
}

echo "==============================================================="
echo "Vanilla-Mac readiness:  $(basename "$APP")"
echo "  $APP"
echo "==============================================================="

# ---- gather all Mach-O files once ----
typeset -a MACHO
while IFS= read -r -d '' f; do
    MACHO+=("$f")
done < <(find "$APP" -type f \( -name "*.dylib" -o -perm +111 \) -print0)
echo "Scanning ${#MACHO} Mach-O files…"
echo

# Search bases shared by every file for @rpath resolution: the main
# executable's rpaths (dyld resolves @rpath using the whole load chain, mainly
# the main executable) plus Contents/Frameworks where macdeployqt places deps.
typeset -a BASE_RPATHS
BASE_RPATHS=("$APP/Contents/Frameworks")
while IFS= read -r p; do
    p="${p//@executable_path/$MACOS_DIR}"
    [[ -n "$p" ]] && BASE_RPATHS+=("$p")
done < <(rpaths "$EXE_PATH")

# ---- 1. external load dependencies ----
echo "[1] External load dependencies (/opt, /usr/local, /Users)"
ext=0
for f in "${MACHO[@]}"; do
    while IFS= read -r d; do
        [[ -z "$d" ]] && continue
        case "$d" in
            /opt/*|/usr/local/*|/Users/*)
                echo "       $(basename "$f")  ->  $d"; ext=$((ext+1)) ;;
        esac
    done < <(load_deps "$f")
done
(( ext == 0 )) && pass "no external load deps (self-contained)" || fail "$ext external load dep(s) — would fail on a clean Mac"

# ---- 2. external rpaths ----
echo "[2] LC_RPATH search paths outside the bundle"
extr=0
for f in "${MACHO[@]}"; do
    while IFS= read -r p; do
        [[ -z "$p" ]] && continue
        case "$p" in
            /opt/*|/usr/local/*|/Users/*)
                echo "       $(basename "$f")  rpath  $p"; extr=$((extr+1)) ;;
        esac
    done < <(rpaths "$f")
done
if (( extr == 0 )); then
    pass "no external rpaths"
else
    echo "  ⚠️  $extr external rpath(s) — cleanliness only, NOT fatal: dyld skips a"
    echo "      non-existent rpath as long as a bundle-internal rpath also resolves the dep."
fi

# ---- 3. dangling internal deps ----
echo "[3] Internal (@rpath/@executable_path/@loader_path) deps resolve"
dangling=0
for f in "${MACHO[@]}"; do
    loader_dir="${f:h}"
    typeset -a rp
    rp=("${BASE_RPATHS[@]}")
    while IFS= read -r p; do
        p="${p//@executable_path/$MACOS_DIR}"
        p="${p//@loader_path/$loader_dir}"
        rp+=("$p")
    done < <(rpaths "$f")
    while IFS= read -r d; do
        case "$d" in
            @executable_path/*) cand="${d/@executable_path/$MACOS_DIR}"; [[ -e "$cand" ]] || { echo "       $(basename "$f")  ->  $d (missing)"; dangling=$((dangling+1)); } ;;
            @loader_path/*)     cand="${d/@loader_path/$loader_dir}";    [[ -e "$cand" ]] || { echo "       $(basename "$f")  ->  $d (missing)"; dangling=$((dangling+1)); } ;;
            @rpath/*)
                rel="${d#@rpath/}"; found=false
                for base in "${rp[@]}"; do [[ -e "$base/$rel" ]] && { found=true; break; }; done
                $found || { echo "       $(basename "$f")  ->  $d (unresolved)"; dangling=$((dangling+1)); } ;;
        esac
    done < <(load_deps "$f")
done
(( dangling == 0 )) && pass "all internal deps resolve inside the bundle" || fail "$dangling unresolved internal dep(s)"

# ---- 4. architecture ----
echo "[4] Architecture"
archs=$(lipo -archs "$EXE_PATH" 2>/dev/null)
echo "       executable: $archs"
if [[ "$archs" == *arm64* ]]; then
    pass "arm64 present (runs on Apple Silicon)"
    [[ "$archs" == *x86_64* ]] && echo "       (universal — also runs on Intel)" \
                               || echo "       (arm64-only — will NOT run on Intel Macs)"
else
    fail "no arm64 slice"
fi

# ---- 5. minimum OS ----
echo "[5] Minimum macOS"
minos=$(otool -l "$EXE_PATH" 2>/dev/null | awk '/LC_BUILD_VERSION/{f=1} f&&/minos/{print $2; f=0}')
echo "       minos: ${minos:-unknown}"
minos_major="${minos%%.*}"
if [[ -n "$minos_major" && "$minos_major" -le "$MIN_OS_FLOOR_MAJOR" ]]; then
    pass "runs on macOS ${minos}+ (floor ${MIN_OS_FLOOR_MAJOR})"
else
    fail "minos ${minos} is newer than the supported floor ${MIN_OS_FLOOR_MAJOR}.x"
fi

# ---- 6-8. Gatekeeper / notarization ----
if [[ "$MODE" == "--structural-only" ]]; then
    echo
    echo "(Skipping Gatekeeper checks 6-8 — structural-only mode.)"
else
    echo "[6] Code signature (deep, strict)"
    if codesign --verify --deep --strict --verbose=2 "$APP" >/dev/null 2>&1; then
        pass "signature valid"
    else
        fail "codesign --verify failed"
    fi

    echo "[7] Gatekeeper assessment"
    assess=$(spctl --assess --type exec --verbose=2 "$APP" 2>&1)
    if echo "$assess" | grep -q "accepted"; then
        pass "$(echo "$assess" | grep -E 'accepted|source=' | tr '\n' ' ')"
    else
        fail "Gatekeeper did not accept: $assess"
    fi

    echo "[8] Notarization staple"
    if xcrun stapler validate "$APP" >/dev/null 2>&1; then
        pass "ticket stapled (launches offline)"
    else
        fail "no stapled ticket (run notarize first)"
    fi
fi

echo "==============================================================="
if $ok; then
    echo "✅ PASS — bundle is vanilla-Mac ready (static checks)."
    echo "   Definitive test remains: launch on a clean macOS VM / second account."
    exit 0
else
    echo "❌ FAIL — see ❌ items above; not safe to ship as-is."
    exit 1
fi
